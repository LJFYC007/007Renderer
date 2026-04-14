## Project Overview

007Renderer is a real-time GPU path tracer built on Windows with C++17, DirectX 12, NVRHI, Slang shading language, and ImGui. It follows a Falcor-style architecture and targets research-level use in computer graphics.

## Build Commands

Prerequisites: run `.\setup.ps1` (PowerShell) to download Slang (v2026.3.1) and DXC (v1.8.2505.1) binaries into `external/`.

### Shell Environment (Important)

**All build commands must be run through `powershell -Command "..."`** — the user's PowerShell `$PROFILE` initializes MSVC/cmake/Ninja and adds Windows SDK tools to PATH. Non-PowerShell shells (bash, cmd) lack these environment variables.

**Why this matters:** `cmake` and `ninja` are available from bash, so configure may succeed, but the build will fail with `'fxc.exe' is not recognized` — `fxc.exe` (Windows SDK FX Compiler, used for HLSL shaders) is only in PATH after PowerShell's `$PROFILE` runs. Always wrap build commands in `powershell -Command "..."` to get the full SDK environment.

```bash
# Configure (Ninja generator, MSVC x64 toolchain)
powershell -Command "cmake -S . -B build/RelWithDebInfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo"

# Build
powershell -Command "cmake --build build/RelWithDebInfo --parallel"

# Run
powershell -Command "build/RelWithDebInfo/bin/RelWithDebInfo/007Renderer.exe"

# Run all tests
powershell -Command "cmake --build build/RelWithDebInfo --target run_tests"

# Run the 9-test GitHub Actions subset (non-Full tests)
powershell -Command "cmake --build build/RelWithDebInfo --target run_tests_ci"
```

## Architecture

### Core (`src/Core/`)

`Device` wraps D3D12 + NVRHI initialization, command list, and debug validation. `Window` wraps Win32 window creation and the message loop. `Program` (`src/Core/Program/`) handles Slang shader compilation and reflection-based binding via `BindingSetManager`; `ReflectionInfo` stores per-binding metadata. `RenderData` (`src/Core/RenderData.h`) is a `string → nvrhi::ResourceHandle` map for passing resources between render passes. `ref<T>` (in `src/Core/Pointer.h`) is the standard `shared_ptr<T>` alias; construct with `make_ref<T>(...)`.

### Render Graph (`src/RenderPasses/`)

A DAG of `RenderPass` nodes connected by named typed ports (`RenderPassInput`/`RenderPassOutput`, defined alongside `RenderPassRegistry` in `RenderPass.h`). `RenderGraph` performs topological sort and executes passes in order, passing `RenderData` between them. `RenderGraphEditor` provides an ImGui node-editor UI for rewiring the graph at runtime. New passes self-register via static initializers calling `RenderPassRegistry::registerPass`.

**Concrete passes:** `PathTracingPass` (ray tracing), `AccumulatePass` (temporal accumulation), `ErrorMeasure` (image quality metrics). Utility passes live under `src/RenderPasses/Utils/`: `TextureAverage` (compute-based texture reduction). Each pass has colocated `.slang` shaders; shared Slang headers use `.slangh`.

### Shader Passes (`src/ShaderPasses/`)

`Pass` base class owns a `BindingSetManager` and supports `pass["resourceName"] = handle;` syntax for binding. `ComputePass` and `RayTracingPass` derive from it.

**Key design pattern:** `RenderPass` (graph node) and `Pass` (NVRHI shader dispatch) are separate hierarchies. A concrete render pass like `PathTracingPass` **is-a** `RenderPass` and **has-a** `RayTracingPass`. The graph owns orchestration; `Pass` subclasses own GPU dispatch.

### Scene (`src/Scene/`)

`Scene` holds geometry (vertices, indices, meshes), materials, acceleration structures (TLAS/BLAS), and camera. `Camera` (`src/Scene/Camera/`) has both C++ and Slang components (`Camera.slang`, `CameraData.slang`).

**Importers** (`src/Scene/Importer/`): `Importer` is the base class; `loadSceneWithImporter()` dispatches by file extension — USD extensions → `UsdImporter`, GLTF/OBJ → `AssimpImporter` (didn't maintain for a while). `TextureManager` (`src/Scene/Material/`) handles texture loading (PNG, JPG, EXR, DDS) with `kInvalidTextureId = 0xFFFFFFFF` as sentinel.

### Slang Shading Pipeline

Slang files live under `src/Scene/`, `src/Scene/Material/`, `src/RenderPasses/*/`, and `src/Utils/`. The pipeline is layered by responsibility:

- **Geometry stage** (`VertexData` → `ShadingPrep` → `ShadingData`): produces a geometry-only surface. No normal map, no back-face flip — those are material decisions. `ShadingData` keeps immutable geometric references (`faceN`, `tangentW`) separate from the mutable shading frame (`T/B/N`).
- **Material stage** (`Material/`): `IMaterial` interface with `GLTFMaterial` as the concrete implementation. The material owns `prepareShadingFrame()` (normal mapping, back-face handling) and a **Material → BSDF pipeline**: `prepareBSDF(sd)` samples all textures once and returns a `GLTFBSDF` that owns `eval(wo)`, `evalPdf(wo)`, and `sample(sg)` in shading-frame local space. **Callers must hoist `prepareBSDF()` once per hit and call methods on the returned `GLTFBSDF` directly** — each call re-samples 4 textures, so calling `material.scatter()` and then evaluating again via a second `prepareBSDF()` is a perf bug. `GLTFMaterial::scatter()` remains only to satisfy `IMaterial` (used by furnace mode). Per-lobe BSDF math lives in `GLTFBSDFs` / `GGXMicrofacet`; sample validation in `BSDFTypes::isValidScatter()`.
- **Orchestration** (`PathTracing.slang`): stays thin — fetch hit data, call `prepareShadingData()`, let the material refine the frame, validate, spawn next ray.

**Critical rule:** never substitute `faceN` for shading normal `N`. `faceN` is for sidedness checks and ray offsets; `N` is for BSDF evaluation.

**Shadow / visibility rays must offset both endpoints.** `computeRayOrigin(pos, normal)` (in `Ray.slang`, Wächter & Binder integer-offset method) pushes a point slightly off a surface along `normal`. For NEE shadow rays, offset the origin along the shading surface's oriented face normal *and* the target along the light's face normal oriented toward the shading point. This matches PBRT4's `SpawnRayTo(pFrom, nFrom, pTo, nTo)`. Offsetting only the origin causes false occlusion for nearly-coplanar shading/light points (e.g., ceiling points near a ceiling light) because the ray travels at a shallow angle from below the surface to a point sitting on it, clipping through geometry in between.

**Light sampling (NEE + MIS).** `Scene` collects emissive triangles into an area-weighted CDF at scene-load (`EmissiveTriangle` struct mirrored in `Scene.h` and `Scene.slang`). `LightSampler.slang` exposes `sampleLight()` (area-measure, 1/totalArea constant pdf) and `evalLightPdf()` (solid-angle conversion for MIS). `PathTracing.slang` combines NEE direct lighting with BSDF scattering using the balance heuristic; see the MIS blocks in `closestHitMain`. Emissive hits at bounce 0 accumulate unweighted (camera-direct), later bounces weight by BSDF-pdf-share.

`Scene.slang` exposes the GPU scene as `ParameterBlock<Scene> gScene` with `StructuredBuffer<>` for mesh/material data; material textures are bindless via `ParameterBlock<MaterialTextures> gMaterialTextures`.

**Slang idioms:** `import` for module dependencies; `ParameterBlock<>` for GPU globals; `StructuredBuffer<>` for geometry arrays; `#include` only where needed before `import` (e.g., `GLTFMaterial.slang`).

### Utils (`src/Utils/`)

GUI (`GUI` namespace + `GUIManager` class in `GUI.h`; shared `Widgets`; `Theme::Luminograph` palette in `Theme.h`), logging (`Logger`), image I/O (`ExrUtils`, `ResourceIO`), math (`Math/`, including `MathConstants.slangh`), and sampling (`Sampling/SampleGenerator` with a Slang interface).

## Naming Conventions (Falcor-style)

- Member variables: `m` + PascalCase (`mEnabled`, `mDirty`)
- Pointer members: `mp` + PascalCase (`mpDevice`, `mpScene`)
- Static members: `s` + PascalCase (`sRegistry`)
- Constants: `k` + PascalCase (`kDefaultFlags`, `kMaxCount`)
- Function pointer params: `p` prefix (`pProgram`, `pState`)
- File/class/public method names: PascalCase
- Private variables/methods: camelCase

**Exception:** GPU-alignment padding fields in structs mirrored between C++ and Slang use the `_padding` / `_cbPadding` naming (leading underscore). These are not real members — they exist only to make `sizeof` match the 16-byte GPU layout — and the underscore marks them as "do not touch" for readers.

## Patching External Dependencies

Local patches live in `patches/` and are applied to submodules by CMake with sentinel files (e.g., `external/nvrhi/.patched-coopvec`) to avoid re-applying:
- `nvrhi-disable-coopvec.patch` — applied to `external/nvrhi`
- `tinyusdz-raw-colorspace-passthrough.patch` — applied to `external/tinyusdz`

To modify a submodule: edit in-place under `external/`, then `git diff > patches/<name>.patch` from within the submodule directory. Patches must be reapplied after `git submodule update`.

## Key Dependencies

Dependencies live under `external/` in three forms:
- **Submodules** (see `.gitmodules`): NVRHI (D3D12-only, Vulkan/DX11 disabled), ImGui (DX12 + Win32), TinyUSDZ, Assimp (GLTF/OBJ), DirectXTex (DDS), GLM (experimental + force-radians), spdlog (bundled fmt), GoogleTest.
- **Vendored in-tree**: imgui-node-editor (render graph UI), TinyEXR + miniz (EXR I/O).
- **Downloaded by `setup.ps1`** (not in git): Slang v2026.3.1, DXC v1.8.2505.1 — DLLs copied post-build.

NVRHI and TinyUSDZ carry local patches — see the Patching section above.

## Build System Notes

Single root `CMakeLists.txt` (no nested CMakeLists under `src/` or `tests/`). Sources are collected via `file(GLOB_RECURSE)`. The `007Core` static library contains all `src/` code except `main.cpp`; the `007Renderer` executable links `007Core`; `007Tests` links `007Core` + GoogleTest.

**Note:** `PROJECT_SHADER_DIR` is defined as `${CMAKE_SOURCE_DIR}/shaders` but no `shaders/` directory exists — shader files live under `src/` and are found via `PROJECT_SRC_DIR` search paths in `Program`.

**Clangd/intellisense noise:** editing files that include `imgui.h`, `nvrhi/nvrhi.h`, or project headers that depend on them often produces `<new-diagnostics>` reports like `'imgui.h' file not found`, `Unknown type 'ImVec2'`, `Unknown type 'RenderPassRefreshFlags'`. These come from clangd lacking the CMake include paths — they are **not** real build errors. Verify with `powershell -Command "cmake --build build/RelWithDebInfo --parallel"` instead of trying to "fix" them.

## Testing

Tests live in `tests/`. The shared test environment is set up in `tests/Environment.cpp` (device, logger, ImGui context initialization shared across test cases).

**Always run the full test suite (`run_tests`) during development.** The `run_tests_ci` target is only for GitHub Actions — it excludes the `Full` convergence tests that catch real rendering regressions.

**Test inventory** (11 total, 9 non-Full):
- `PathTracerTest`: Basic (4 spp smoke), Full (4096 spp + error threshold, writes `output.exr`), WhiteFurnaceFull (1024 spp × roughness sweep)
- `SlangTest`: Basic (Slang reflection + ComputePass verification)
- `ComputeShaderTest`: Basic (buffer add via ComputePass)
- `RenderGraphTest`: 6 tests — linear build, duplicate node names, missing required input, optional inputs unconnected, cycles, unknown slot connections

The CI subset (`--gtest_filter="-*Full"`) runs 9 tests, excluding `PathTracerTest.Full` and `PathTracerTest.WhiteFurnaceFull`.