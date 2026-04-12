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

## Requirements

- C++17 compiler
- CMake 3.10+
- OpenCV
- Assimp
- Eigen
- OpenMP-capable compiler, optional but recommended

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
