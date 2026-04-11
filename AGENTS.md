## Project Overview

007Renderer is a real-time GPU path tracer built on Windows with C++17, DirectX 12, NVRHI, Slang shading language, and ImGui. It follows a Falcor-style architecture and targets research-level use in computer graphics.

## Build Commands

Prerequisites: run `.\setup.ps1` (PowerShell) to download Slang (v2026.3.1) and DXC (v1.8.2505.1) binaries into `external/`.

### Shell Environment (Important)

**All build commands must be run through `powershell -Command "..."`** — the user's PowerShell `$PROFILE` initializes MSVC/cmake/Ninja. Non-PowerShell shells (bash, cmd) lack these environment variables.

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

`Device` wraps D3D12 + NVRHI initialization, command list, and debug validation. `Window` wraps Win32 window creation and the application message loop. `Program` (`src/Core/Program/`) handles Slang shader compilation and reflection-based binding via `BindingSetManager`; `ReflectionInfo` stores per-binding metadata extracted from Slang reflection. `RenderData` (`src/Core/RenderData.h`) is a `string → nvrhi::ResourceHandle` map used to pass resources between render passes.

`ref<T>` (alias for `shared_ptr<T>`, defined in `src/Core/Pointer.h`) is the standard smart pointer; use `make_ref<T>(...)` to construct.

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

Slang files live under `src/Scene/*.slang`, `src/Scene/Material/*.slang`, `src/RenderPasses/*/*.slang`, and `src/Utils/**/*.slang`. Path tracing follows a layered shading design:

- `VertexData.slang` — gathers raw geometric hit data via triangle interpolation
- `ShadingPrep.slang` — builds geometry-only `ShadingData` from `VertexData` (no normal map, no back-face flip — those are material decisions)
- `ShadingData.slang` — shading-ready surface, keeping immutable geometric references (`faceN`, `tangentW`) separate from the mutable shading frame (`T/B/N`)
- `Scene.slang` — GPU `Scene` struct exposed as `ParameterBlock<Scene> gScene` with `StructuredBuffer<>` for mesh/material data
- `Material/Material.slang` — `IMaterial` interface; bindless `gMaterialTextures` via `ParameterBlock<MaterialTextures>`
- `Material/GLTFMaterial.slang` — concrete `GLTFMaterial : IMaterial`; owns `prepareShadingFrame()` (normal mapping, back-face handling) and `scatter()` (BSDF sampling)
- `Material/GLTFBSDFs.slang` — specular/diffuse/BTDF implementations
- `Material/GGXMicrofacet.slang` — GGX microfacet distribution and masking helpers
- `Material/BSDFTypes.slang` — `BSDFSample`, event types, `isValidScatter()` for geometric-side validation
- `Material/TextureSampler.slang` — texture sampling helpers

`PathTracing.slang` should stay a thin orchestrator: fetch hit data, call `prepareShadingData()`, let the material refine the frame, validate the sample, and spawn the next ray. Do not substitute `faceN` for shading normal `N`: `faceN` is for sidedness/ray offsets, `N` is for BSDF evaluation.

**Slang idioms:** `import` for module dependencies; `ParameterBlock<>` for GPU globals; `StructuredBuffer<>` for geometry arrays; `#include` only where needed before `import` (e.g., `GLTFMaterial.slang`).

### Utils (`src/Utils/`)

GUI helpers (`GUI`, `GUIWrapper`, ImGui theming via `imgui_spectrum`), logging (`Logger`), image I/O (`ExrUtils`, `ResourceIO`), math primitives (`Math/` — `Math.h`, `MathConstants.slangh`, `Ray.slang`), and sampling utilities (`Sampling/` — `SampleGenerator` with Slang interface).

## Naming Conventions (Falcor-style)

- Member variables: `m` + PascalCase (`mEnabled`, `mDirty`)
- Pointer members: `mp` + PascalCase (`mpDevice`, `mpScene`)
- Static members: `s` + PascalCase (`sRegistry`)
- Constants: `k` + PascalCase (`kDefaultFlags`, `kMaxCount`)
- Function pointer params: `p` prefix (`pProgram`, `pState`)
- File/class/public method names: PascalCase
- Private variables/methods: camelCase

## Patching External Dependencies

Local patches live in `patches/` and are applied to submodules by CMake with sentinel files (e.g., `external/nvrhi/.patched-coopvec`) to avoid re-applying:
- `nvrhi-disable-coopvec.patch` — applied to `external/nvrhi`
- `tinyusdz-raw-colorspace-passthrough.patch` — applied to `external/tinyusdz`

To modify a submodule: edit in-place under `external/`, then `git diff > patches/<name>.patch` from within the submodule directory. Patches must be reapplied after `git submodule update`.

## Key Dependencies

Vendored under `external/` in three categories:

**Git submodules** (in `.gitmodules`):
- **NVRHI**: D3D12-only (Vulkan/DX11 disabled). Has local patch disabling CoopVec.
- **ImGui**: Immediate-mode GUI (DX12 + Win32 backends, built via `cmake/FindImgui.cmake`).
- **TinyUSDZ**: USD scene loading. Has local patch for raw colorspace passthrough.
- **Assimp**: GLTF/OBJ import.
- **DirectXTex**: DDS texture loading.
- **GLM**: Math library (vectors, matrices). Experimental features and force-radians enabled.
- **spdlog**: Logging backend (static build, bundled fmt).
- **GoogleTest**: Unit testing framework.

**Vendored in-tree** (not submodules):
- **imgui-node-editor**: Visual node editor for the render graph UI.
- **TinyEXR** (with miniz): EXR image I/O.

**Downloaded by `setup.ps1`** (not in git):
- **Slang** (v2026.3.1): Shader language; DLLs copied post-build.
- **DXC** (v1.8.2505.1): DirectX shader compiler; DLLs copied post-build.

## Build System Notes

Single root `CMakeLists.txt` (no nested CMakeLists under `src/` or `tests/`). Sources are collected via `file(GLOB_RECURSE)`. The `007Core` static library contains all `src/` code except `main.cpp`; the `007Renderer` executable links `007Core`; `007Tests` links `007Core` + GoogleTest.

**Note:** `PROJECT_SHADER_DIR` is defined as `${CMAKE_SOURCE_DIR}/shaders` but no `shaders/` directory exists — shader files live under `src/` and are found via `PROJECT_SRC_DIR` search paths in `Program`.

## Testing

Tests live in `tests/`. The shared test environment is set up in `tests/Environment.cpp` (device, logger, ImGui context initialization shared across test cases).

**Test inventory** (11 total, 9 non-Full):
- `PathTracerTest`: Basic (4 spp smoke), Full (4096 spp + error threshold, writes `output.exr`), WhiteFurnaceFull (1024 spp × roughness sweep)
- `SlangTest`: Basic (Slang reflection + ComputePass verification)
- `ComputeShaderTest`: Basic (buffer add via ComputePass)
- `RenderGraphTest`: 6 tests (graph build, ordering, duplicate detection, missing/optional inputs, cycles, bad connections)

The CI subset (`--gtest_filter="-*Full"`) runs 9 tests, excluding `PathTracerTest.Full` and `PathTracerTest.WhiteFurnaceFull`.