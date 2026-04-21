# AGENTS.md

Contributor + AI-agent guide for 007Renderer. For the user-facing intro and roadmap, see [`README.md`](./README.md).

## Before You Start

**Four rules — each one has bitten past contributors. Read before touching anything.**

1. **Build from PowerShell only.** `cmake` and `ninja` are on PATH in bash, so configure appears to succeed — then the build fails with `'fxc.exe' is not recognized`. `fxc.exe` (Windows SDK FX Compiler, used for HLSL) is only reachable after the user's PowerShell `$PROFILE` runs. Wrap every build/test command in `powershell -Command "..."`.
2. **Clangd diagnostics are noise, not build errors.** Edits to files that include `imgui.h`, `nvrhi/nvrhi.h`, or dependent project headers often produce `<new-diagnostics>` reports like *"'imgui.h' file not found"* or *"Unknown type 'ImVec2'"*. These come from clangd missing the CMake include paths — they are **not** real build errors. Verify with an actual `cmake --build` before trying to "fix" them.
3. **Run the full test suite, not the CI subset.** `run_tests` executes all 11 cases including the `Full` convergence tests that catch real rendering regressions; `run_tests_ci` skips those and exists only for GitHub Actions parity.
4. **Never substitute `faceN` for the shading normal `N` in Slang.** `faceN` is for sidedness checks and ray offsets only. Using it as a shading normal silently corrupts BSDF evaluation.

## Build, Run, Test

Prerequisites: run `.\setup.ps1` (PowerShell) to fetch Slang v2026.3.1 and DXC v1.8.2505.1 into `external/`.

```bash
# Configure (Ninja generator, MSVC x64 toolchain)
powershell -Command "cmake -S . -B build/RelWithDebInfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo"

# Build
powershell -Command "cmake --build build/RelWithDebInfo --parallel"

# Run
powershell -Command "build/RelWithDebInfo/bin/RelWithDebInfo/007Renderer.exe"

# Full test suite — use this during development
powershell -Command "cmake --build build/RelWithDebInfo --target run_tests"

# GitHub Actions subset (9 non-Full cases) — for CI parity only
powershell -Command "cmake --build build/RelWithDebInfo --target run_tests_ci"
```

**Debugging runtime issues:** launch `007Renderer.exe` and inspect `logs/007Renderer.log` — Slang compile errors and scene-load warnings land there. If you need logs for debug mode, please use DEBUG instead of RelWithDebInfo. NEVER modify the code in Logger.h/cpp while debugging.

## Naming Conventions (Falcor-style)

| Kind | Convention | Examples |
| --- | --- | --- |
| Member variable | `m` + PascalCase | `mEnabled`, `mDirty` |
| Pointer member | `mp` + PascalCase | `mpDevice`, `mpScene` |
| Static member | `s` + PascalCase | `sRegistry` |
| Constant | `k` + PascalCase | `kDefaultFlags`, `kMaxCount` |
| Function-pointer param | `p` prefix | `pProgram`, `pState` |
| File / class / public method | PascalCase | |
| Private variable / method | camelCase | |

**Exception:** GPU-alignment padding in C++↔Slang mirrored structs uses `_padding` / `_cbPadding` (leading underscore). These fields are not real members — they exist only to make `sizeof` match the 16-byte GPU layout — and the underscore marks them as "do not touch" for readers.

## Architecture

Entry point: `src/main.cpp` wires up `Device`, `Window`, and the default `RenderGraph`. Everything else under `src/` is compiled into the `007Core` static library.

### Core (`src/Core/`)

`Device` wraps D3D12 + NVRHI init, command lists, and debug validation. `Window` wraps Win32 and the message loop. `Program` (`src/Core/Program/`) handles Slang compilation + reflection-based binding via `BindingSetManager`; `ReflectionInfo` holds per-binding metadata. `RenderData` is a `string → nvrhi::ResourceHandle` map passed between render passes. `ref<T>` (`src/Core/Pointer.h`) is the standard `shared_ptr<T>` alias; construct with `make_ref<T>(...)`.

### Render Graph (`src/RenderPasses/`)

A DAG of `RenderPass` nodes connected by named typed ports (`RenderPassInput` / `RenderPassOutput`, defined alongside `RenderPassRegistry` in `RenderPass.h`). `RenderGraph` topo-sorts and executes passes in order, threading `RenderData` through. `RenderGraphEditor` is the ImGui node-editor UI for runtime rewiring. New passes self-register via static initializers calling `RenderPassRegistry::registerPass`.

`RenderGraphBuilder` (header-only) is the fluent builder used in `main.cpp` to wire the default graph at startup; runtime edits go through `RenderGraphEditor`.

**Concrete passes:** `PathTracingPass`, `AccumulatePass`, `ErrorMeasurePass`, `ToneMappingPass`. Utility passes live under `src/RenderPasses/Utils/` (e.g., `TextureAverage`, a compute-based texture reduction). Each pass colocates its `.slang` shaders; shared Slang headers use `.slangh`.

### Shader Passes (`src/ShaderPasses/`)

`Pass` base class owns a `BindingSetManager` and supports `pass["resourceName"] = handle;` syntax. `ComputePass` and `RayTracingPass` derive from it.

**Key design split:** `RenderPass` (graph node) and `Pass` (NVRHI shader dispatch) are separate hierarchies. `PathTracingPass` **is-a** `RenderPass` and **has-a** `RayTracingPass`. The graph owns orchestration; `Pass` subclasses own GPU dispatch.

### Scene (`src/Scene/`)

`Scene` holds geometry (vertices, indices, `MeshDesc` ranges), **per-BLAS `MeshInstance`s** (meshID + materialIndex + `localToWorld`), materials, acceleration structures (one BLAS per mesh; TLAS instances carry the transforms), and camera. `EmissiveTriangle`s key on `instanceID`, not meshID — a mesh reused by N instances contributes N emitters. `Camera` (`src/Scene/Camera/`) has paired C++ and Slang components (`Camera.slang`, `CameraData.slang`).

**Importers** (`src/Scene/Importer/`): `Importer` is the base class; `loadSceneWithImporter()` dispatches by file extension — USD → `UsdImporter`, GLTF/OBJ → `AssimpImporter` (currently unmaintained). `TextureManager` (`src/Scene/Material/`) handles texture loading (PNG, JPG, EXR, DDS) with `kInvalidTextureId = 0xFFFFFFFF` as sentinel.

**Scene assets:** `media/` holds `cornell_box.usdc` / `.gltf`, `sphere.usdc`, and `reference.exr` (used by `PathTracerTest.Full`). Tests resolve paths via `PROJECT_DIR + "/media/..."`.

### Utils (`src/Utils/`)

GUI (`GUI` namespace + `GUIManager` class in `GUI.h`; shared `Widgets`; `Theme::Luminograph` palette in `Theme.h`), logging (`Logger`), image I/O (`ExrUtils`, `ResourceIO`), math (`Math/` + `MathConstants.slangh`), and sampling (`Sampling/SampleGenerator` with a Slang interface).

## Slang Shading Pipeline

Slang files live under `src/Scene/`, `src/Scene/Material/`, `src/RenderPasses/*/`, and `src/Utils/`. The pipeline is layered by responsibility:

- **Geometry stage** (`VertexData` → `ShadingPrep` → `ShadingData`): produces a geometry-only surface. No normal map, no back-face flip — those are material decisions. `ShadingData` keeps immutable geometric refs (`faceN`, `tangentW`) separate from the mutable shading frame (`T/B/N`).
- **Material stage** (`Material/`): `IMaterial` interface with `GLTFMaterial` as the concrete implementation. The material owns `prepareShadingFrame()` (normal map + back-face handling) and a **Material → BSDF pipeline**: `prepareBSDF(sd)` samples all textures once and returns a `GLTFBSDF` that owns `eval(wo)`, `evalPdf(wo)`, and `sample(sg)` in shading-frame local space. `GLTFMaterial::scatter()` exists only to satisfy `IMaterial` (used by furnace mode). Per-lobe BSDF math lives in `GLTFBSDFs` / `GGXMicrofacet`; sample validation in `BSDFTypes::isValidScatter()`.
- **Orchestration** (`PathTracing.slang`): stays thin — fetch hit data, call `prepareShadingData()`, let the material refine the frame, validate, spawn next ray.

### Critical Slang Rules

- **`faceN` vs `N`** — never substitute `faceN` for the shading normal. `faceN` is for sidedness checks and ray offsets; `N` is for BSDF evaluation. *(Also in Before You Start.)*
- **Hoist `prepareBSDF(sd)` once per hit** — 4 texture samples per call. *(Also in Before You Start.)*
- **Shadow / visibility rays must offset both endpoints.** `computeRayOrigin(pos, normal)` (in `Ray.slang`, Wächter & Binder integer offsets) pushes a point slightly off a surface along `normal`. For NEE shadow rays, offset the origin along the shading surface's oriented face normal *and* the target along the light's face normal oriented toward the shading point. Matches PBRT4's `SpawnRayTo(pFrom, nFrom, pTo, nTo)`. Offsetting only the origin causes false occlusion for nearly-coplanar shading/light points (e.g., ceiling points near a ceiling light) because the ray travels at a shallow angle from below the surface to a point sitting on it, clipping through geometry in between.
- **`import` vs `#include` macro scoping.** Use `import` for module dependencies; `#include` only where needed *before* `import` (e.g., `GLTFMaterial.slang`). Macros from `#include` do **not** cross `import` boundaries — a macro defined in a header you `#include` is invisible inside a module you `import`.

### Light Sampling (NEE + MIS)

`Scene` collects emissive triangles into an area-weighted CDF at scene-load (`EmissiveTriangle` struct mirrored in `Scene.h` and `Scene.slang`). `LightSampler.slang` exposes `sampleLight()` (area-measure, 1/totalArea constant pdf) and `evalLightPdf()` (solid-angle conversion for MIS). `PathTracing.slang` combines NEE direct lighting with BSDF scattering using the balance heuristic; see MIS blocks in `closestHitMain`. Emissive hits at bounce 0 accumulate unweighted (camera-direct); later bounces weight by BSDF-pdf-share.

### Slang Idioms

- `import` for module dependencies.
- `ParameterBlock<>` for GPU globals.
- `StructuredBuffer<>` for geometry arrays.
- `Scene.slang` exposes the GPU scene as `ParameterBlock<Scene> gScene`; material textures are bindless via `ParameterBlock<MaterialTextures> gMaterialTextures`.

**Session cache:** `ShaderCompiler` keeps a process-lifetime Slang session, so successive `Program` constructions reuse the global session/options. Side effect: edits to `.slang` files don't hot-reload in a long-running session — restart the app to pick them up.

## External Dependencies & Patches

Under `external/` in three forms:

- **Submodules** (`.gitmodules`): NVRHI (D3D12-only; Vulkan/DX11 disabled), ImGui (DX12 + Win32), TinyUSDZ, Assimp (GLTF/OBJ), DirectXTex (DDS), GLM (experimental + force-radians), spdlog (bundled fmt), GoogleTest.
- **Vendored in-tree**: imgui-node-editor (render-graph UI), TinyEXR + miniz (EXR I/O).
- **Downloaded by `setup.ps1`** (not in git): Slang v2026.3.1, DXC v1.8.2505.1 — DLLs are copied post-build.

**Local patches** live in `patches/` and are applied to submodules by CMake using sentinel files to avoid re-application:

| Patch | Applied to | Sentinel |
| --- | --- | --- |
| `nvrhi-disable-coopvec.patch` | `external/nvrhi` | `.patched-coopvec` |
| `tinyusdz-raw-colorspace-passthrough.patch` | `external/tinyusdz` | (checked per-patch) |

**To modify a submodule:** edit in-place under `external/`, then `git diff > patches/<name>.patch` from within the submodule directory. Patches must be reapplied after `git submodule update`.

## Build System Notes

- Single root `CMakeLists.txt` (no nested `CMakeLists` under `src/` or `tests/`). Sources collected via `file(GLOB_RECURSE)`.
- `007Core` static library contains all `src/` code except `main.cpp`; `007Renderer` executable links `007Core`; `007Tests` links `007Core` + GoogleTest.
- `PROJECT_SHADER_DIR` is defined as `${CMAKE_SOURCE_DIR}/shaders` but no such directory exists — shader files live under `src/` and are found via `PROJECT_SRC_DIR` search paths in `Program`.

## Testing

Tests live in `tests/`. The shared test environment (device, logger, ImGui context) is set up in `tests/Environment.cpp`.

**Always run `run_tests` during development.** `run_tests_ci` is a GitHub Actions target and skips the `Full` convergence tests that catch real rendering regressions.

**Test inventory (15 total, 9 non-Full — the roughness sweep expands to 5 gtest instances):**

| Suite | Cases |
| --- | --- |
| `PathTracerTest` | `Basic` (4 spp smoke), **`Full`** (4096 spp + error threshold, saves `output.exr` on failure) |
| `RoughnessSweep/WhiteFurnaceFull` | **`ConvergesToWhite/r{5,25,50,75,100}`** (1024 spp × 5-value roughness sweep via `INSTANTIATE_TEST_SUITE_P`) |
| `SlangTest` | `All` (Slang reflection + ComputePass across plain globals, RWTexture, ParameterBlock, bindless) |
| `ComputeShaderTest` | `Basic` (buffer add via ComputePass) |
| `RenderGraphTest` | 6 cases: linear build, duplicate node names, missing required input, optional inputs unconnected, cycles, unknown slot connections |

The CI subset (`--gtest_filter="-*Full*"`) runs 9 tests, excluding `PathTracerTest.Full` plus all 5 `RoughnessSweep/WhiteFurnaceFull.ConvergesToWhite/r*` instances. The trailing wildcard matters — without it the substring match on `WhiteFurnaceFull` is missed.

**Test fixture + artifacts (`tests/TestHelpers.{h,cpp}`):**
- Derive new suites from `DeviceTest` (grabs the process-lifetime device in `SetUp`) instead of rolling your own fixture.
- Write test outputs (EXRs, dumps) to `TestHelpers::artifactPath("name.exr")` — resolves to `tests/artifacts/<Suite>.<Test>/`. Save on failure only so passing runs stay hermetic.
