# haorender

haorender is a learning-focused CPU software renderer written in C++ with a Qt desktop front end for scene inspection, shading iteration, profiling, and renderer debugging.

It keeps the raster pipeline visible and hackable while adding a practical workstation around it: model import, material inspection, shadow controls, shading mode switching, profiler readback, preset save/load, session restore, and an optional Embree-backed hybrid shadow path.

## Languages

- [简体中文](README.zh-CN.md)
- [日本語](README.ja.md)

## Project Status

- Current app entry: [`qt_main.cpp`](qt_main.cpp)
- Legacy OpenCV prototype retained for reference: [`main.cpp`](main.cpp)
- Main target: `myrender`
- Platform focus: Windows desktop

## Highlights

### Renderer

- CPU rasterization pipeline with perspective projection, viewport transform, clipping, back-face culling, and z-buffering
- Frustum clipping and tile binning to avoid near-camera triangle blowups
- Multi-threaded raster/shadow work with OpenMP where available
- Hybrid `Raster + Embree` shadow option without replacing the main raster renderer
- Phong path tuned for stylized look development
- PBR path with image-based lighting, channel remapping, tone mapping, and linear/sRGB conversion
- Optional programmable fragment-style shader DSL for look-dev and debugging

### Desktop UI

- Qt desktop workspace with separate tabs for Workspace, Scene, Shading, Lights, Materials, and Inspect
- English / Simplified Chinese UI language switching
- Theme switching and persistent session restore
- Render background color or image selection
- Resolution presets, shadow settings, light rig editing, and per-look shading controls
- Material overview panel and runtime profiler panel
- Preset save/load for renderer state and scene tuning

## Core Shading Modes

### Realistic PBR

- IBL diffuse/specular strength control
- sky light control
- metallic / roughness / AO / emissive channel mapping
- tone-mapped output path

### Stylized Phong

- hard / soft specular styling
- toon band diffuse option
- optional tone mapping
- primary-light-focused stylized tuning

### Programmable Shader

- lightweight expression-based fragment shader DSL
- editable in the Qt UI
- built-in examples for lit, toon, rim, normal-debug, and shadow-debug looks
- compile status, guide text, and safe fallback to the last successful program

## Direct Download Use

For non-developers, the recommended distribution format is a Windows portable package:

1. Download the release zip, for example `haorender-windows-portable.zip`
2. Extract it to any writable folder
3. Keep the packaged `Resources` directory next to the executable
4. Run `myrender.exe`

Optional startup:

```powershell
.\myrender.exe
.\myrender.exe .\Resources\MAIFU\IF.fbx
.\myrender.exe .\Resources\MAIFU\IF.fbx --shadow-technique=embree
```

The repository now includes a packaging helper:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows_portable.ps1 -BuildDir .\build-nonhalf\Release -Zip
```

That script collects the release executable, runtime DLLs, Qt deployment folders, `Resources`, and the bilingual README files into a portable package directory, and optionally creates a zip archive.

## Build From Source

### Requirements

- CMake 3.10+
- C++17 compiler
- Qt 5 Widgets
- OpenCV
- Assimp
- Eigen
- OpenMP-capable compiler recommended
- Embree 4 optional

### Configure and Build

```powershell
cmake -S . -B build-nonhalf -DCMAKE_BUILD_TYPE=Release
cmake --build build-nonhalf --config Release
```

If CMake cannot find dependencies, pass explicit paths:

```powershell
cmake -S . -B build-nonhalf `
  -DEIGEN3_INCLUDE_DIR="path\to\eigen3" `
  -DASSIMP_INCLUDE_DIR="path\to\assimp\include" `
  -DEMBREE_ROOT_DIR="path\to\embree"
cmake --build build-nonhalf --config Release
```

### Useful Build Options

```powershell
-DHAO_RENDER_ENABLE_EMBREE=ON
-DHAO_RENDER_DEPTH_HALF=ON
-DHAO_RENDER_VERTEX_HALF=ON
```

- `HAO_RENDER_ENABLE_EMBREE`: enables the optional Embree-assisted path when Embree is available
- `HAO_RENDER_DEPTH_HALF`: stores z-buffer / shadow depth in half precision
- `HAO_RENDER_VERTEX_HALF`: stores loaded vertex attributes in half precision

## Running the App

Default run:

```powershell
.\build-nonhalf\Release\myrender.exe
```

Model path and shadow mode can be provided on the command line:

```powershell
.\build-nonhalf\Release\myrender.exe .\Resources\MAIFU\IF.fbx
.\build-nonhalf\Release\myrender.exe .\Resources\MAIFU\IF.fbx --shadow-technique=embree
```

If no model path is provided, the app tries built-in default resource paths from the repository.

## Desktop Workflow

### Scene

- camera FOV
- exposure
- normal strength
- internal render resolution preset
- back-face culling
- shadow resolution / extent / depth / cascade settings

### Shading

- switch between PBR / Stylized Phong / Programmable Shader
- each shading look exposes its own dedicated control set

### Lights

- up to three directional lights
- per-light yaw, pitch, intensity, and RGB color

### Materials

- per-mesh material summary
- texture path inspection

### Inspect

- mesh / triangle / vertex counts
- current render resolution
- Embree availability
- camera readback
- frame profiler breakdown

## Controls in the Viewport

- Left mouse drag: orbit camera
- Right mouse drag or middle mouse drag: pan
- Mouse wheel: zoom
- Toolbar / panels: load model, load environment, change renderer settings

## Repository Layout

```text
include/                    Core headers
Resources/                  Example models, textures, and environment maps
qt_main.cpp                 Current Qt desktop application entry and UI
main.cpp                    Legacy OpenCV viewer / prototype path
model.cpp                   Model, texture, and material loading
render.cpp                  Frame orchestration, shadow passes, binning, and renderer state
shader.cpp                  Rasterization, clipping, shading, shadows, PBR / Phong logic
programmable_shader.cpp     Small programmable shader compiler/interpreter
raytrace_backend.cpp        Embree-backed helper path
scripts/                    Packaging helpers
```

## Recommended Release Layout

```text
haorender-windows-portable/
  myrender.exe
  assimp-vc143-mtd.dll
  opencv_world*.dll
  Qt5*.dll
  embree4.dll                (when available)
  tbb12.dll                  (when available)
  platforms/
  imageformats/
  styles/
  iconengines/
  Resources/
  README.md
  README.zh-CN.md
```

## Notes

- This repository is still a renderer-learning project, not a production engine.
- The Qt desktop app is now the primary experience.
- The OpenCV prototype remains useful as a stripped-down reference path.
- No explicit open-source license has been added yet.

