# haorender

haorender is a CPU software renderer written in C++ for experimenting with the full rendering pipeline: model loading, camera control, rasterization, depth buffering, shadow mapping, Phong shading, and early PBR material support.

This repository is a learning-oriented renderer rather than a production engine. The current focus is making the pipeline visible and hackable while keeping enough performance to interact with real models through an OpenCV window.

## README Languages

- [简体中文](README.zh-CN.md)
- [日本語](README.ja.md)

## Features

- Assimp-based model loading for FBX/OBJ-like assets
- OpenCV window output and mouse camera control
- Perspective projection, viewport transform, back-face culling, and z-buffering
- Frustum clipping to avoid huge screen-space triangles near the camera
- Tile-based rasterization with OpenMP parallelism
- Diffuse, normal, specular, and partial PBR texture loading
- Configurable PBR channel mapping for metallic, roughness, AO, and emissive channels
- Phong shading with sRGB/linear conversion, tone mapping, normal strength control, and adjusted specular response
- Shadow mapping with higher-resolution maps, cascaded near/far shadow layers, and gradient PCF softening
- Optional `Raster + Embree` shadow mode, keeping rasterization as the primary renderer while using Embree for CPU ray-occlusion queries

## Requirements

- C++17 compiler
- CMake 3.10+
- OpenCV
- Assimp
- Eigen
- OpenMP-capable compiler, optional but recommended
- Embree 4, optional when you want the hybrid raster + ray-traced shadow path

The project currently uses a local `assimp-vc143-mtd.dll` on Windows. If your Assimp installation differs, update `CMakeLists.txt` or pass the correct Assimp CMake/include path during configuration.

## Build

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

If CMake cannot find Eigen or Assimp, pass explicit paths:

```powershell
cmake -S . -B build -DEIGEN3_INCLUDE_DIR="path\to\eigen3" -DASSIMP_INCLUDE_DIR="path\to\assimp\include"
cmake --build build --config Release
```

To experiment with half-precision depth buffers for the z-buffer and shadow maps:

```powershell
cmake -S . -B build-half -DHAO_RENDER_DEPTH_HALF=ON
cmake --build build-half --config Release
```

Half precision can reduce memory bandwidth, but it may also increase depth artifacts. The default build uses `float` depth buffers as the safer performance baseline.

To experiment with half-precision vertex storage:

```powershell
cmake -S . -B build-vertex-half -DHAO_RENDER_VERTEX_HALF=ON
cmake --build build-vertex-half --config Release
```

This stores loaded mesh position, normal, tangent, bitangent, and UV data in `Eigen::half`, then converts them back to `float` for the CPU MVP and shading path. On CPUs this mainly reduces vertex memory bandwidth; it does not guarantee faster matrix multiplication unless the hardware/compiler can execute half arithmetic efficiently.

To enable the optional Embree backend:

```powershell
cmake -S . -B build-embree -DHAO_RENDER_ENABLE_EMBREE=ON -DEMBREE_INCLUDE_DIR="path\to\embree\include" -DEMBREE_LIBRARY="path\to\embree4.lib"
cmake --build build-embree --config Release
```

If Embree is not found, the project still builds and keeps using the original shadow-map path.

## Run

From the build output directory:

```powershell
.\myrender.exe
```

You can also pass a model path:

```powershell
.\myrender.exe ..\Resources\MAIFU\IF.fbx
```

The default model path in `main.cpp` is:

```text
../Resources/MAIFU/IF.fbx
```

## Controls

- Left mouse drag: orbit camera
- Right mouse drag: pan camera
- Mouse wheel: zoom
- `r`: reset camera
- `w`, `a`, `s`, `d`: rotate the model for quick inspection
- `1`, `2`, `3`: switch PBR channel mapping presets
- `4`: use the original shadow-map path
- `5`: use `Raster + Embree` shadow mode when Embree is available
- `Esc`: exit

## Current Notes

- The renderer is CPU-based and intentionally keeps most pipeline stages in project code for learning and debugging.
- The PBR path exists, but many models only provide diffuse textures, so the Phong path is still important.
- Shadow quality has been improved with cascaded near/far maps and PCF, but shadow map generation is still a major performance cost.
- A future GPU backend, such as an OpenGL renderer sharing the same model/material loading path, is a good next milestone.

## Repository Layout

```text
include/        Core headers for model, renderer, shader, drawer, and image helpers
Resources/      Example model and texture assets
main.cpp        OpenCV application loop and camera controls
model.cpp       Assimp model/material/texture loading
render.cpp      Render orchestration, shadow pass, tile binning, and frame rendering
shader.cpp      Rasterization, clipping, fragment shading, shadows, and PBR/Phong logic
Drawer.cpp      Drawing helpers
tgaimage.cpp    TGA image support
```

## License

No explicit license has been added yet. Add one before distributing or accepting external contributions.
