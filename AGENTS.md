# AGENTS.md

Contributor + AI-agent guide for 007Renderer. For the user-facing intro and roadmap, see [`README.md`](./README.md).

## Before You Start

**Four rules — each one has bitten past contributors. Read before touching anything.**

1. **Build from PowerShell only.** `cmake` and `ninja` are on PATH in bash, so configure appears to succeed — then the build fails with `'fxc.exe' is not recognized`. `fxc.exe` (Windows SDK FX Compiler, used for HLSL) is only reachable after the user's PowerShell `$PROFILE` runs. Wrap every build/test command in `powershell -Command "..."`.
2. **Clangd diagnostics are noise, not build errors.** Edits to files that include `imgui.h`, `nvrhi/nvrhi.h`, or dependent project headers often produce `<new-diagnostics>` reports like *"'imgui.h' file not found"* or *"Unknown type 'ImVec2'"*. These come from clangd missing the CMake include paths — they are **not** real build errors. Verify with an actual `cmake --build` before trying to "fix" them.
3. **Run the full test suite, not the CI subset.** `run_tests` executes all 15 cases (including the 6 `Full` convergence tests that catch real rendering regressions); `run_tests_ci` skips those and exists only for GitHub Actions parity.
4. **Never substitute `faceN` for the shading normal `N` in Slang.** `faceN` is for sidedness checks and ray offsets only. Using it as a shading normal silently corrupts BSDF evaluation.

## Build, Run, Test

Prerequisites: run `.\setup.ps1` (PowerShell) to fetch Slang v2026.3.1 and DXC v1.8.2505.1 into `external/`.

```bash
powershell -Command "cmake -S . -B build/RelWithDebInfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo"
powershell -Command "cmake --build build/RelWithDebInfo --parallel"
powershell -Command "build/RelWithDebInfo/bin/RelWithDebInfo/007Renderer.exe"
powershell -Command "cmake --build build/RelWithDebInfo --target run_tests"     # full (15 cases) — use during dev
powershell -Command "cmake --build build/RelWithDebInfo --target run_tests_ci"  # CI parity only (9 non-Full)
```

**Debugging runtime issues:** launch `007Renderer.exe` and inspect `logs/007Renderer.log` — Slang compile errors and scene-load warnings land there. For debug-mode logs, build `DEBUG` instead. Never modify `Logger.h/.cpp`.

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

A DAG of `RenderPass` nodes connected by named typed ports (`RenderPassInput` / `RenderPassOutput`, defined alongside `RenderPassRegistry` in `RenderPass.h`). `RenderGraph` topo-sorts and executes passes in order, threading `RenderData` through. New passes self-register via static initializers calling `RenderPassRegistry::registerPass`. `RenderGraphBuilder` (header-only) wires the default graph at startup in `main.cpp`; runtime rewiring goes through the ImGui-based `RenderGraphEditor`.

Each pass colocates its `.slang` shaders in the same directory; shared Slang headers use `.slangh`. Utility passes live under `src/RenderPasses/Utils/`.

### Shader Passes (`src/ShaderPasses/`)

`Pass` base class owns a `BindingSetManager` and supports `pass["resourceName"] = handle;` syntax. `ComputePass` and `RayTracingPass` derive from it.

**Key design split:** `RenderPass` (graph node) and `Pass` (NVRHI shader dispatch) are separate hierarchies. `PathTracingPass` **is-a** `RenderPass` and **has-a** `RayTracingPass`. The graph owns orchestration; `Pass` subclasses own GPU dispatch.

### Scene (`src/Scene/`)

`Scene` holds geometry (vertices, indices, `MeshDesc` ranges), **per-BLAS `MeshInstance`s** (meshID + materialIndex + `localToWorld`), materials, acceleration structures (one BLAS per mesh; TLAS instances carry the transforms), and camera. `EmissiveTriangle`s key on `instanceID`, not meshID — a mesh reused by N instances contributes N emitters. `Camera` (`src/Scene/Camera/`) has paired C++ and Slang components (`Camera.slang`, `CameraData.slang`).

**Importers** (`src/Scene/Importer/`): `Importer` is the base class; `loadSceneWithImporter()` dispatches by file extension — USD → `UsdImporter`, GLTF/OBJ → `AssimpImporter` (currently unmaintained). `TextureManager` (`src/Scene/Material/`) handles texture loading (PNG, JPG, EXR, DDS) with `kInvalidTextureId = 0xFFFFFFFF` as sentinel. Tests resolve scene-asset paths via `PROJECT_DIR + "/media/..."`.

### Utils (`src/Utils/`)

GUI (`GUI.h` namespace + `GUIManager`; `Theme::Luminograph` palette), `Logger`, EXR/image I/O, `Math/` + `MathConstants.slangh`, and `Sampling/SampleGenerator` (C++/Slang pair).

## Slang Shading Pipeline

The pipeline is layered by responsibility:

- **Geometry stage** (`VertexData` → `ShadingPrep` → `ShadingData`): produces a geometry-only surface. No normal map, no back-face flip — those are material decisions. `ShadingData` keeps immutable geometric refs (`faceN`, `tangentW`) separate from the mutable shading frame (`T/B/N`).
- **Material stage** (`Material/`): `IMaterial` interface with `GLTFMaterial` as the concrete implementation. The material owns `prepareShadingFrame()` (normal map + back-face handling) and a **Material → BSDF pipeline**: `prepareBSDF(sd)` samples all textures once and returns a `GLTFBSDF` that owns `eval(wo)`, `evalPdf(wo)`, and `sample(sg)` in shading-frame local space. `GLTFMaterial::scatter()` exists only to satisfy `IMaterial` (used by furnace mode). Per-lobe BSDF math lives in `GLTFBSDFs` / `GGXMicrofacet`; sample validation in `BSDFTypes::isValidScatter()`.
- **Orchestration** (`PathTracing.slang`): stays thin — fetch hit data, call `prepareShadingData()`, let the material refine the frame, validate, spawn next ray.

### Critical Slang Rules

- **Hoist `prepareBSDF(sd)` once per hit** — 4 texture samples per call.
- **Shadow / visibility rays must offset both endpoints** via `computeRayOrigin(pos, normal)` in `Ray.slang` — offset the shading point along its oriented face normal *and* the light point along its face normal oriented back toward the shading point. Matches PBRT4's `SpawnRayTo(pFrom, nFrom, pTo, nTo)`. Rationale lives next to the function.
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

Submodules live under `external/` (see `.gitmodules`). Non-obvious facts: NVRHI is configured D3D12-only (Vulkan/DX11 disabled), GLM is built with `GLM_ENABLE_EXPERIMENTAL` + `GLM_FORCE_RADIANS`, imgui-node-editor and TinyEXR+miniz are vendored in-tree (not submodules), and Slang/DXC are downloaded by `setup.ps1` (not in git) with DLLs copied post-build.

**Local patches** in `patches/` are applied to submodules by CMake using sentinel files to avoid re-application:

| Patch | Applied to | Sentinel |
| --- | --- | --- |
| `nvrhi-disable-coopvec.patch` | `external/nvrhi` | `.patched-coopvec` |
| `tinyusdz-raw-colorspace-passthrough.patch` | `external/tinyusdz` | (checked per-patch) |

**To modify a submodule:** edit in-place under `external/`, then `git diff > patches/<name>.patch` from within the submodule directory. Patches must be reapplied after `git submodule update`.

## Testing

Tests live in `tests/`. The shared test environment (device, logger, ImGui context) is set up in `tests/Environment.cpp`.

Suites: `PathTracerTest` (incl. `Full` 4096-spp convergence), `RoughnessSweep/WhiteFurnaceFull` (5-value GGX sweep), `SlangTest`, `ComputeShaderTest`, `RenderGraphTest`.

**CI-filter gotcha:** `run_tests_ci` uses `--gtest_filter="-*Full*"`. The trailing `*` matters — without it, the substring match on `WhiteFurnaceFull` is missed and the roughness sweep leaks into CI.

**Test fixture + artifacts (`tests/TestHelpers.{h,cpp}`):**
- Derive new suites from `DeviceTest` (grabs the process-lifetime device in `SetUp`) instead of rolling your own fixture.
- Write test outputs (EXRs, dumps) to `TestHelpers::artifactPath("name.exr")` — resolves to `tests/artifacts/<Suite>.<Test>/`. Save on failure only so passing runs stay hermetic.
