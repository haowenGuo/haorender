# haorender

haorender 已经不再只是一个学习级 CPU 软渲染器，而是一套半工业级 CPU 渲染器与桌面渲染工作台。

它在保留软渲染器可读性、可调试性和可改造性的同时，已经形成了比较完整的工程化形态：具备较成熟的光栅化主干、可切换着色体系、阴影系统、Embree 混合路径、材质检查、性能分析、预设管理，以及可直接分发给用户使用的 Qt 桌面应用。

## 语言

- [English](README.md)
- [日本語](README.ja.md)

## 项目定位

- 当前主程序入口：[`qt_main.cpp`](qt_main.cpp)
- 旧版 OpenCV 原型仍保留用于对照和简化实验：[`main.cpp`](main.cpp)
- 主桌面目标程序：`myrender`
- 当前重点平台：Windows 桌面

haorender 的定位已经更接近“CPU 渲染器工作台”而不是课堂练习项目，核心目标包括：

- 稳定可复现的模型与材质加载
- 可控的着色与光照工作流
- 可观察的渲染器内部状态
- 可量化的分阶段性能分析
- 可直接打包分发的桌面使用体验

## haorender 已实现的能力

### 渲染器核心能力

- CPU 光栅化主干：MVP、视口变换、裁剪、背面剔除、ZBuffer
- 近景视锥裁剪与 tile 分桶，避免镜头贴近模型时三角形投影炸裂
- 在可用环境下支持 OpenMP 多线程
- 近远分层阴影贴图与级联参数控制
- 可选 `Raster + Embree` 混合阴影模式
- 一套经过调校的风格化 Phong 渲染路径
- 一套支持 IBL、通道映射、tone mapping、线性/sRGB 转换的 PBR 渲染路径
- 一套可编程片元着色 DSL，用于 look-dev 和诊断调试

### Qt 桌面工作台能力

- Workspace、Scene、Shading、Lights、Materials、Inspect 多工作页
- 中英双语界面切换
- 主题切换、会话恢复
- 模型导入、环境图导入、预设保存 / 读取
- 背景颜色 / 背景图片切换
- Profiler 面板显示 clear、shadow near/far、vertex、clip/bin、raster/shade、总帧耗时

## 主要功能模块

### 1. 着色系统

haorender 当前提供三条独立着色工作流：

- `写实 PBR`
  - IBL 漫反射 / 高光控制
  - metallic / roughness / AO / emissive 通道映射
  - tone mapping 输出链路
- `风格化 Phong`
  - 硬边 / 柔边高光
  - 卡通分段漫反射
  - 环境光 / 辅光 / 主光比例调校
- `可编程着色器`
  - 内置轻量表达式 DSL
  - 直接在桌面 UI 中编辑
  - 带有编译状态、示例模板、说明文档与失败回退保护

### 2. 灯光与阴影

- 最多三盏可编辑方向光
- 每盏灯支持 yaw、pitch、强度、RGB 颜色
- 更高分辨率 shadow map
- 近远分层阴影范围与深度控制
- cascade split / blend 控制
- 在保持光栅化主渲染的前提下，可切换 Embree 辅助阴影路径

### 3. 资产与材质工作流

- 基于 Assimp 的 FBX / OBJ 类资产加载
- 每个 mesh 的材质概览
- 桌面界面内可见纹理路径与绑定信息
- PBR 通道调试
- 用于 look-dev 的背景图与环境图工作流

### 4. 调试与性能分析

- UI 直接显示分阶段帧分析
- 材质与渲染状态检查
- 可编程着色器支持法线 / 阴影 / rim 等调试模板
- 会话恢复，便于连续调参和回看结果

## 直接下载使用

对于不想自己编译的用户，推荐直接下载 GitHub Release 中的便携包：

1. 下载 `haorender-windows-portable.zip`
2. 解压到任意可写目录
3. 保持 `Resources` 文件夹与 `myrender.exe` 同级
4. 直接运行 `myrender.exe`

示例启动方式：

```powershell
.\myrender.exe
.\myrender.exe .\Resources\MAIFU\IF.fbx
.\myrender.exe .\Resources\MAIFU\IF.fbx --shadow-technique=embree
```

仓库中已经提供 Windows 便携包打包脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows_portable.ps1 -BuildDir .\build-nonhalf\Release -Zip
```

该脚本会把可执行文件、运行时 DLL、Qt 部署目录、`Resources`、中英 README 一起整理为便携包目录，并输出 zip。

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

如果依赖无法自动找到，可以显式传入路径：

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

- `HAO_RENDER_ENABLE_EMBREE`：启用 Embree 混合路径
- `HAO_RENDER_DEPTH_HALF`：使用半精度深度缓冲 / 阴影深度
- `HAO_RENDER_VERTEX_HALF`：使用半精度存储加载后的顶点属性

## 运行

```powershell
.\build-nonhalf\Release\myrender.exe
.\build-nonhalf\Release\myrender.exe .\Resources\MAIFU\IF.fbx
.\build-nonhalf\Release\myrender.exe .\Resources\MAIFU\IF.fbx --shadow-technique=embree
```

如果不提供模型路径，程序会尝试从仓库内置候选路径中加载默认资源。

## 桌面工作流

### Scene

- FOV
- 曝光
- 法线强度
- 内部渲染分辨率预设
- 背面剔除
- 阴影分辨率 / 范围 / 深度 / 级联参数

### Shading

- PBR / 风格化 Phong / 可编程着色三模式切换
- 每种模式都有独立控制面板

### Lights

- 最多三盏方向光
- 每盏灯支持 yaw、pitch、强度、RGB

### Materials

- 每个 mesh 的材质概览
- 纹理绑定与路径检查

### Inspect

- mesh / triangle / vertex 数量统计
- 当前渲染分辨率
- Embree 可用性
- 相机状态回读
- 帧分阶段 Profiler

## 视口交互

- 鼠标左键拖动：环绕观察
- 鼠标右键或中键拖动：平移
- 鼠标滚轮：缩放

## 仓库结构

```text
include/                    核心头文件
Resources/                  示例模型、贴图、环境图
qt_main.cpp                 当前 Qt 桌面主程序与 UI
main.cpp                    旧版 OpenCV 原型路径
model.cpp                   模型、材质、贴图加载
render.cpp                  渲染调度、阴影 pass、分桶与状态控制
shader.cpp                  光栅化、裁剪、着色、阴影、PBR / Phong 逻辑
programmable_shader.cpp     可编程着色器编译 / 解释执行器
raytrace_backend.cpp        Embree 辅助路径
scripts/                    打包与辅助脚本
```

## 推荐发布目录

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

- haorender 当前定位已经是“半工业级 CPU 渲染器与渲染工作台”，不再只是学习性质的小型练习项目。
- Qt 桌面应用已经是主入口和主体验。
- OpenCV 路径仍然保留，适合做轻量实验和低开销对照。
- 当前仓库尚未加入明确开源许可证。

