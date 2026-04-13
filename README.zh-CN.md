# haorender

haorender 是一个以学习和研究为目标的 C++ CPU 软渲染器，并配套了一套 Qt 桌面工作台，用于场景查看、着色调试、参数迭代和性能分析。

它保留了“可读、可改、可调试”的软渲染主干，同时补上了真正能日常使用的桌面工具层：模型导入、材质检查、阴影控制、着色模式切换、Profiler 回读、预设保存/读取、会话恢复，以及可选的 Embree 混合阴影路径。

## 语言

- [English](README.md)
- [日本語](README.ja.md)

## 当前状态

- 当前主程序入口：[`qt_main.cpp`](qt_main.cpp)
- 旧版 OpenCV 原型保留用于参考：[`main.cpp`](main.cpp)
- 主目标程序：`myrender`
- 当前重点平台：Windows 桌面

## 核心亮点

### 渲染器本体

- CPU 光栅化主干：透视投影、视口变换、裁剪、背面剔除、ZBuffer
- 已加入视锥裁剪与 tile 分桶，避免近景三角形炸成巨大屏幕包围盒
- 在可用环境下支持 OpenMP 多线程
- 可选 `Raster + Embree` 混合阴影模式，不改变主渲染仍以光栅化为核心的架构
- 一套更适合风格化调试的 Phong 路径
- 一套包含 IBL、通道映射、tone mapping、线性/sRGB 转换的 PBR 路径
- 一套轻量可编程片元着色 DSL，用于 look-dev 和调试

### Qt 桌面工作台

- 提供 Workspace、Scene、Shading、Lights、Materials、Inspect 多页签工作流
- 支持中英双语界面切换
- 支持主题切换、会话恢复
- 支持渲染背景颜色 / 图片切换
- 支持分辨率预设、阴影参数、灯光参数、着色参数统一面板调节
- 支持材质概览和运行时性能分析面板
- 支持预设保存 / 读取

## 着色模式

### 写实 PBR

- IBL 漫反射 / 高光强度控制
- 天空光控制
- metallic / roughness / AO / emissive 通道映射
- tone mapping 输出链路

### 风格化 Phong

- 硬边 / 柔边高光
- 卡通分段漫反射
- tone mapping 开关
- 更强调主光风格化表现

### 可编程着色器

- 轻量表达式式片元着色 DSL
- 直接在 Qt 界面内编辑
- 内置默认光照、卡通、描边、法线调试、阴影调试模板
- 带有编译状态、使用说明，以及“编译失败保留上一版成功程序”的安全回退

## 直接下载使用

对于不想自己编译的用户，推荐发布一个 Windows 便携包：

1. 下载 release zip，例如 `haorender-windows-portable.zip`
2. 解压到任意可写目录
3. 保持 `Resources` 文件夹与可执行文件同级
4. 直接运行 `myrender.exe`

可选启动方式：

```powershell
.\myrender.exe
.\myrender.exe .\Resources\MAIFU\IF.fbx
.\myrender.exe .\Resources\MAIFU\IF.fbx --shadow-technique=embree
```

仓库已经附带 Windows 便携包打包脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows_portable.ps1 -BuildDir .\build-nonhalf\Release -Zip
```

这个脚本会把 Release 目录中的可执行文件、运行时 DLL、Qt 部署目录、`Resources`、中英 README 一起打成便携包目录，并可选输出 zip。

## 从源码构建

### 依赖

- CMake 3.10+
- C++17 编译器
- Qt 5 Widgets
- OpenCV
- Assimp
- Eigen
- 推荐支持 OpenMP 的编译器
- Embree 4 可选

### 构建命令

```powershell
cmake -S . -B build-nonhalf -DCMAKE_BUILD_TYPE=Release
cmake --build build-nonhalf --config Release
```

如果 CMake 无法自动找到依赖，可以显式传入路径：

```powershell
cmake -S . -B build-nonhalf `
  -DEIGEN3_INCLUDE_DIR="path\to\eigen3" `
  -DASSIMP_INCLUDE_DIR="path\to\assimp\include" `
  -DEMBREE_ROOT_DIR="path\to\embree"
cmake --build build-nonhalf --config Release
```

### 常用选项

```powershell
-DHAO_RENDER_ENABLE_EMBREE=ON
-DHAO_RENDER_DEPTH_HALF=ON
-DHAO_RENDER_VERTEX_HALF=ON
```

- `HAO_RENDER_ENABLE_EMBREE`：在 Embree 可用时启用混合路径
- `HAO_RENDER_DEPTH_HALF`：使用半精度深度缓冲 / 阴影深度
- `HAO_RENDER_VERTEX_HALF`：使用半精度存储加载后的顶点属性

## 运行

默认运行方式：

```powershell
.\build-nonhalf\Release\myrender.exe
```

也可以在命令行传入模型路径和阴影后端：

```powershell
.\build-nonhalf\Release\myrender.exe .\Resources\MAIFU\IF.fbx
.\build-nonhalf\Release\myrender.exe .\Resources\MAIFU\IF.fbx --shadow-technique=embree
```

如果不传模型路径，程序会尝试从仓库内置候选路径中加载默认资源。

## 桌面工作流

### Scene

- 相机 FOV
- 曝光
- 法线强度
- 内部渲染分辨率预设
- 背面剔除
- 阴影分辨率 / 范围 / 深度 / 级联参数

### Shading

- PBR / 风格化 Phong / 可编程着色 三种模式切换
- 每种模式都有对应独立参数面板

### Lights

- 最多三盏方向光
- 每盏灯支持 yaw、pitch、强度、RGB 颜色调节

### Materials

- 每个 mesh 的材质概览
- 纹理路径检查

### Inspect

- mesh / triangle / vertex 数统计
- 当前渲染分辨率
- Embree 可用性
- 相机参数回读
- 帧分阶段 Profiler

## 视口交互

- 鼠标左键拖动：环绕观察
- 鼠标右键或中键拖动：平移
- 鼠标滚轮：缩放
- 工具栏和面板：用于加载模型、环境图和调节渲染参数

## 仓库结构

```text
include/                    核心头文件
Resources/                  示例模型、贴图、环境图
qt_main.cpp                 当前 Qt 桌面主程序与 UI
main.cpp                    旧版 OpenCV 原型路径
model.cpp                   模型、材质、贴图加载
render.cpp                  渲染调度、阴影 pass、分桶与状态控制
shader.cpp                  光栅化、裁剪、着色、阴影、PBR / Phong 逻辑
programmable_shader.cpp     小型可编程着色器编译/解释执行器
raytrace_backend.cpp        Embree 辅助路径
scripts/                    打包与辅助脚本
```

## 推荐发布目录结构

```text
haorender-windows-portable/
  myrender.exe
  assimp-vc143-mtd.dll
  opencv_world*.dll
  Qt5*.dll
  embree4.dll                （若可用）
  tbb12.dll                  （若可用）
  platforms/
  imageformats/
  styles/
  iconengines/
  Resources/
  README.md
  README.zh-CN.md
```

## 说明

- 这个仓库目前仍然是“学习级渲染器”，不是生产引擎。
- 现在 Qt 桌面工作台已经是主体验入口。
- 旧 OpenCV 原型仍然保留，适合作为简化参考实现。
- 当前尚未加入明确开源许可证。

