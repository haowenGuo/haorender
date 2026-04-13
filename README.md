# haorender

haorender is a semi-industrial CPU renderer and desktop rendering workstation written in C++.

It is no longer just a teaching demo of the graphics pipeline. The project now combines a substantial software rasterizer, configurable shading system, shadow pipeline, hybrid Embree-assisted path, material inspection tools, frame profiling, preset management, and a Qt desktop front end that is practical for renderer development and asset look-dev on Windows.

## Languages

- [简体中文](README.zh-CN.md)
- [日本語](README.ja.md)

## Positioning

- Main application entry: [`qt_main.cpp`](qt_main.cpp)
- Legacy OpenCV prototype retained for comparison and stripped-down reference: [`main.cpp`](main.cpp)
- Main desktop target: `myrender`
- Primary platform: Windows desktop

haorender should be understood as a CPU rendering workbench with real engineering goals:

- reproducible asset loading
- controllable shading workflows
- inspectable renderer state
- measurable frame-stage performance
- distributable desktop usage beyond a source-only demo

## What haorender Delivers

### Core Renderer

- CPU rasterization pipeline with model/view/projection, viewport transform, clipping, back-face culling, and z-buffering
- near-camera frustum clipping and tile binning to prevent giant screen-space triangle blowups
- multi-threaded rendering work with OpenMP where available
- raster shadow maps with near/far layered cascades
- optional hybrid `Raster + Embree` shadow mode for CPU ray-occlusion queries
- tuned Phong path for stylized characters and controlled art-direction
- PBR path with IBL, channel remapping, tone mapping, and linear/sRGB conversion
- programmable fragment-style shader DSL for look-dev and debugging

### Desktop Workstation

- Qt desktop UI with dedicated tabs for Workspace, Scene, Shading, Lights, Materials, and Inspect
- English / Simplified Chinese language switching
- theme switching and persistent session restore
- model import, environment import, preset save/load, and material overview
- render background color/image switching
- integrated profiler readback for clear, shadow near/far, vertex, clip/bin, raster/shade, and total frame time

## Major Feature Set

### 1. Shading System

haorender provides three distinct shading workflows:

- `Realistic PBR`
  - image-based lighting
  - metallic / roughness / AO / emissive channel remapping
  - tone-mapped output
- `Stylized Phong`
  - hard / soft specular response
  - toon-band diffuse option
  - art-directed ambient / secondary light balance
- `Programmable Shader`
  - built-in expression DSL
  - editable from the desktop UI
  - compile feedback, guide text, example presets, and fallback protection

### 2. Lighting and Shadows

- up to three editable directional lights
- per-light yaw, pitch, intensity, and RGB control
- higher-resolution shadow maps
- cascade split / blend controls
- near and far shadow extents / depth ranges
- optional Embree-assisted path while preserving rasterization as the main renderer

### 3. Asset and Material Workflow

- Assimp-based asset loading for FBX / OBJ-like content
- per-mesh material inspection
- texture path visibility in the desktop UI
- PBR channel map debugging
- background image / environment setup for look-dev

### 4. Debugging and Profiling

- frame-stage profiler directly visible in the UI
- material panel and render-state inspection
- programmable shader debug presets for normal / shadow / rim inspection
- session restore for continuing tuning work without rebuilding state each launch

## Direct Download and Use

For users who do not want to build from source, the recommended distribution format is a Windows portable package:

1. Download the GitHub Release asset `haorender-windows-portable.zip`
2. Extract it to any writable folder
3. Keep the packaged `Resources` folder next to `myrender.exe`
4. Run `myrender.exe`

Example startup:

```powershell
.\myrender.exe
.\myrender.exe .\Resources\MAIFU\IF.fbx
.\myrender.exe .\Resources\MAIFU\IF.fbx --shadow-technique=embree
```

The repository includes a packaging helper for producing that release asset:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows_portable.ps1 -BuildDir .\build-nonhalf\Release -Zip
```

It collects the executable, runtime DLLs, Qt deployment folders, `Resources`, and bilingual README files into a portable package directory and zip.

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

If your environment does not auto-discover dependencies:

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

- `HAO_RENDER_ENABLE_EMBREE`: enables the optional Embree-assisted features
- `HAO_RENDER_DEPTH_HALF`: stores z-buffer and shadow depth in half precision
- `HAO_RENDER_VERTEX_HALF`: stores loaded vertex attributes in half precision

## Running

```powershell
.\build-nonhalf\Release\myrender.exe
.\build-nonhalf\Release\myrender.exe .\Resources\MAIFU\IF.fbx
.\build-nonhalf\Release\myrender.exe .\Resources\MAIFU\IF.fbx --shadow-technique=embree
```

If no model path is provided, the application attempts to load built-in default resources from the repository.

## Desktop Workflow

### Scene

- field of view
- exposure
- normal strength
- internal render resolution presets
- back-face culling
- shadow resolution / extent / depth / cascade tuning

### Shading

- mode switch between PBR / Stylized Phong / Programmable Shader
- dedicated control surface for each shading mode

### Lights

- up to three directional lights
- per-light yaw, pitch, intensity, and RGB color

### Materials

- per-mesh material overview
- texture binding inspection

### Inspect

- mesh / triangle / vertex statistics
- current render resolution
- Embree availability
- camera readback
- frame profiler breakdown

## Viewport Interaction

- Left mouse drag: orbit camera
- Right mouse drag or middle mouse drag: pan
- Mouse wheel: zoom

## Repository Layout

```text
include/                    Core headers
Resources/                  Example models, textures, and environment maps
qt_main.cpp                 Current Qt desktop application and UI
main.cpp                    Legacy OpenCV viewer path
model.cpp                   Model, material, and texture loading
render.cpp                  Frame orchestration, shadow passes, binning, and renderer state
shader.cpp                  Rasterization, clipping, shading, shadows, PBR / Phong logic
programmable_shader.cpp     Programmable shader compiler / interpreter
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

- haorender is now positioned as a semi-industrial CPU renderer and renderer workstation, not merely a pipeline-learning toy.
- The Qt desktop application is the primary user experience.
- The OpenCV path is retained because it is still useful for narrow experiments and low-overhead comparison.
- An explicit open-source license has not yet been added.

