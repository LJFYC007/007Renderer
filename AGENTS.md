## Project Overview

007Renderer is a real-time GPU path tracer built on Windows with C++17, DirectX 12, NVRHI, Slang shading language, and ImGui. It follows a Falcor-style architecture and targets research-level use in computer graphics.

## Build Commands

Prerequisites: run `.\setup.ps1` (PowerShell) to download Slang and DXC binaries into `external/`.

The PowerShell environment for this project is expected to already be initialized with the Visual Studio toolchain. Opening PowerShell in this workspace should automatically provide `cmake`, `Ninja`, and MSVC. So do not re-run `vcvarsall.bat` or manually reconfigure the compiler environment unless a command shows that the setup is actually missing.

```bash
# Configure (Ninja generator, MSVC x64 toolchain)
cmake -S . -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/RelWithDebInfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Build
cmake --build build/Debug --parallel
cmake --build build/RelWithDebInfo --parallel

# Run
build/Debug/bin/Debug/007Renderer.exe
build/RelWithDebInfo/bin/RelWithDebInfo/007Renderer.exe

# Run all tests
cmake --build build/Debug --target run_tests

# Run the 9-test GitHub Actions subset (non-Full tests)
cmake --build build/Debug --target run_tests_ci

# Run a single test by filter
build/Debug/bin/Debug/007Tests.exe --gtest_filter="TestSuiteName.TestName"
```

## Architecture

**Core layer** (`src/Core/`): `Device` wraps D3D12 + NVRHI initialization. `Program` (`src/Core/Program/`) handles Slang shader compilation and reflection-based binding via `BindingSetManager`. `ref<T>` (alias for `shared_ptr<T>`) is the standard smart pointer; use `make_ref<T>(...)` to construct.

**Render graph** (`src/RenderPasses/`): A DAG of `RenderPass` nodes connected by named typed ports (`RenderPassInput`/`RenderPassOutput`). `RenderGraph` performs topological sort and executes passes in order, passing `RenderData` (map of named `nvrhi::ResourceHandle`) between them. `RenderGraphEditor` provides an ImGui node-editor UI for rewiring the graph at runtime. New passes self-register via `RenderPassRegistry`.

**Shader passes** (`src/ShaderPasses/`): `Pass` base class owns a `BindingSetManager` and supports `pass["resourceName"] = handle;` syntax for binding. `ComputePass` and `RayTracingPass` derive from it.

**Scene** (`src/Scene/`): `Scene` holds geometry, materials, acceleration structures, and camera. Importers (`AssimpImporter`, `UsdImporter`) load from GLTF/OBJ/USD. `TextureManager` handles texture loading (PNG, JPG, EXR, DDS). 

**Slang** (`src/Scene/*.slang`, `src/RenderPasses/*/*.slang`): Path tracing follows the layered shading design described in `docs/coordinate-system-design.md`:
- `VertexData.slang` gathers raw geometric hit data
- `ShadingData.slang` converts that into a shading-ready surface, keeping immutable geometric references (`faceN`, `tangentW`) separate from the mutable shading frame (`T/B/N`)
- `Material/Material.slang` and `Material/GLTFMaterial.slang` own `prepareShadingFrame()` and `scatter()`, including normal mapping and BSDF sampling
- `Material/BSDFTypes.slang` defines `BSDFSample`, event types, and `isValidScatter()` for geometric-side validation

`PathTracing.slang` should stay a thin orchestrator: fetch hit data, call `prepareShadingData()`, let the material refine the frame, validate the sample, and spawn the next ray. Do not substitute `faceN` for shading normal `N`: `faceN` is for sidedness/ray offsets, `N` is for BSDF evaluation.

**Utils** (`src/Utils/`): GUI helpers (`GUI`, `GUIWrapper`, ImGui theming via `imgui_spectrum`), logging (`Logger`), image I/O (`ExrUtils`, `ResourceIO`), math primitives (`Math/`), and sampling utilities (`Sampling/` — `SampleGenerator` with Slang interface).

**UI**: ImGui provides the immediate-mode GUI. `imgui-node-editor` powers the visual render-graph editor (`RenderGraphEditor`). Both are vendored under `external/`.

**Render passes** live alongside the graph in `src/RenderPasses/`: `PathTracingPass` (ray tracing), `AccumulatePass` (temporal accumulation), `ErrorMeasure` (image quality metrics). Each pass has colocated `.slang` shaders. Shared Slang headers use the `.slangh` extension.

## Naming Conventions (Falcor-style)

- Member variables: `m` + PascalCase (`mEnabled`, `mDirty`)
- Pointer members: `mp` + PascalCase (`mpDevice`, `mpScene`)
- Static members: `s` + PascalCase (`sRegistry`)
- Constants: `k` + PascalCase (`kDefaultFlags`, `kMaxCount`)
- Function pointer params: `p` prefix (`pProgram`, `pState`)

## Patching External Dependencies

Local patches live in `patches/` and are applied to submodules:
- `nvrhi-disable-coopvec.patch` — applied to `external/nvrhi`
- `tinyusdz-raw-colorspace-passthrough.patch` — applied to `external/tinyusdz`

To modify a submodule: edit in-place under `external/`, then `git diff > patches/<name>.patch` from within the submodule directory. Patches must be reapplied after `git submodule update`.

## Key Dependencies

All vendored under `external/` as submodules (except Slang/DXC which are downloaded by `setup.ps1`):
- **NVRHI**: D3D12-only (Vulkan/DX11 disabled). Has local patch disabling CoopVec (`patches/nvrhi-disable-coopvec.patch`).
- **Slang**: Shader language; DLLs copied post-build.
- **TinyUSDZ**: USD scene loading. Has local patch for raw colorspace passthrough.
- **Assimp**: GLTF/OBJ import.
- **DirectXTex**: DDS texture loading.
- **TinyEXR** (with miniz): EXR image I/O.
- **GoogleTest**: Unit testing framework.

## Testing

Tests live in `tests/`. The GitHub Actions test set consists of 9 non-`Full` tests. This subset is intended for the Actions environment, which does not provide a GPU. `Full` tests are intended for local execution and are used for correctness validation with a 4096 spp path tracing run. The shared test environment is set up in `tests/Environment.cpp` (device initialization shared across test cases).
