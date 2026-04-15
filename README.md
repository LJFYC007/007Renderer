# 007Renderer

![Build](https://github.com/LJFYC007/007Renderer/actions/workflows/windows-test.yml/badge.svg)

A real-time GPU path tracer built on Windows with C++17, DirectX 12, NVRHI, Slang, and ImGui. Falcor-style architecture, research-oriented.

As a graphics PhD student, I built this as both a vehicle for deepening my real-time rendering knowledge and a personal sandbox for reproducing state-of-the-art graphics papers.

## Features

- **Path tracing** on the D3D12 ray tracing pipeline (DXR)
- **Next-Event Estimation + Multiple Importance Sampling** — balance heuristic, transmission-aware, area-weighted emissive-triangle CDF
- **GLTF 2.0 materials** — metallic/roughness, transmission, IOR, normal maps, emissive, per-texture UV transforms
- **GGX microfacet BSDF** — dielectric Fresnel, specular reflection & transmission, Lambertian diffuse
- **Scene formats** — USD (`.usd*` via TinyUSDZ), GLTF / OBJ (via Assimp)
- **Render graph** — DAG of passes (PathTracing → Accumulate → ToneMapping → ErrorMeasure) with an ImGui node-editor for runtime rewiring
- **Temporal accumulation** with automatic reset on camera / scene change
- **Image I/O** — EXR, PNG, JPG, HDR, DDS
- **Luminograph GUI theme** + shared widget library
- **11-case GoogleTest suite** — includes white-furnace and converged-reference regression checks

## Quick Start

```powershell
# 1. Fetch Slang + DXC binaries
.\setup.ps1

# 2. Configure + build (run from PowerShell — MSVC + Windows SDK env needed)
cmake -S . -B build/RelWithDebInfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/RelWithDebInfo --parallel

# 3. Run
build\RelWithDebInfo\bin\RelWithDebInfo\007Renderer.exe

# 4. Full test suite
cmake --build build/RelWithDebInfo --target run_tests
```

See [`AGENTS.md`](./AGENTS.md) for architecture deep-dive, naming conventions, Slang idioms, and submodule patches.

## Repository Layout

```
src/
├── Core/             # Device, Window, Program (Slang compile + reflection binding)
├── RenderPasses/     # Graph nodes: PathTracing, Accumulate, ErrorMeasure, ToneMapping
├── ShaderPasses/     # NVRHI dispatch wrappers: ComputePass, RayTracingPass
├── Scene/            # Scene, Camera, Importers (USD, Assimp), Material, BSDFs
└── Utils/            # GUI, logging, math, sampling, image I/O
tests/                # GoogleTest suites
media/                # Bundled Cornell Box + sphere scenes
external/             # Submodules + vendored deps
patches/              # Local patches applied to submodules at configure time
```

## Roadmap

### Materials
- [ ] **OpenPBR / Disney Principled BSDF** — only GLTF 2.0 metallic-roughness today
- [ ] Anisotropic GGX
- [ ] Clearcoat, sheen, iridescence lobes
- [ ] Subsurface scattering

### Lighting & Sampling
- [ ] **ReSTIR DI / GI** — baseline is NEE + MIS only
- [ ] Environment map / IBL (emissive triangles are currently the only light source)
- [ ] Analytic light types (point, directional, spot, rect area)
- [ ] Light BVH / hierarchical light sampling for large emissive sets

### Denoising & Post
- [ ] SVGF / À-Trous spatiotemporal denoiser
- [ ] OIDN or OptiX denoiser integration
- [ ] Bloom, auto-exposure, configurable tonemap operators

### Importers
- [ ] **Finish the Assimp importer** — currently unmaintained (`Scene/Importer/AssimpImporter.cpp`)
- [ ] USD: compute camera FOV from camera attributes (`UsdImporter.cpp:300`)
- [ ] MaterialX / UsdPreviewSurface material translation
- [ ] Animation and skinning

### Render Graph Editor
- [ ] **Graph serialization** — save / load graphs as JSON (no persistence today)
- [ ] Live pass hot-reload without full rebuild

### Engine Plumbing
- [ ] Constant-buffer lifetime audit — everything is marked volatile (`Core/Program/Program.cpp:617`)
- [ ] Multi-queue / async compute
- [ ] Vulkan backend (NVRHI supports it; currently disabled via patch)
- [ ] Scene path as CLI argument (hardcoded in `main.cpp` today)

### Quality of Life
- [ ] In-app screenshot / EXR dump hotkey
- [ ] Camera path recorder + playback

## License

See [`LICENSE`](./LICENSE).
