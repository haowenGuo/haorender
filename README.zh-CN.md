# haorender

haorender 是一个使用 C++ 编写的 CPU 软渲染器，用来实验和学习完整渲染管线：模型加载、相机控制、光栅化、深度缓冲、阴影贴图、Phong 着色，以及早期 PBR 材质支持。

这个项目更偏学习和研究，不是生产级引擎。当前目标是让渲染管线尽量清晰、可调试，同时保持足够的交互性能，可以在 OpenCV 窗口中查看真实模型。

## 其他语言

- [English](README.md)
- [日本語](README.ja.md)

## 功能特性

- 基于 Assimp 加载 FBX/OBJ 等模型资源
- 使用 OpenCV 窗口显示渲染结果，并支持鼠标相机控制
- 透视投影、视口变换、背面剔除和 ZBuffer
- 视锥裁剪，用于避免镜头靠近模型时三角形投影成巨大屏幕包围盒
- 基于 tile 的光栅化和 OpenMP 并行
- 支持 diffuse、normal、specular 以及部分 PBR 贴图读取
- 支持 metallic、roughness、AO、emissive 的 PBR 通道映射切换
- Phong 着色支持 sRGB/线性转换、色调映射、法线强度控制和更稳定的高光响应
- 阴影贴图支持更高分辨率、近远级联分层和渐变 PCF 软阴影
- 可选 `光栅化 + Embree` 阴影模式，主渲染仍然走原本光栅化，只把遮挡查询交给 Embree

## 环境要求

- C++17 编译器
- CMake 3.10+
- OpenCV
- Assimp
- Eigen
- 支持 OpenMP 的编译器，推荐开启
- Embree 4，可选，用于启用混合式光栅化 + 光追阴影路径

当前 Windows 环境下项目根目录包含一个本地 `assimp-vc143-mtd.dll`。如果你的 Assimp 安装路径不同，可以修改 `CMakeLists.txt`，或者在 CMake 配置时传入正确的 Assimp 路径。

## 构建

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

如果 CMake 找不到 Eigen 或 Assimp，可以显式传入路径：

```powershell
cmake -S . -B build -DEIGEN3_INCLUDE_DIR="path\to\eigen3" -DASSIMP_INCLUDE_DIR="path\to\assimp\include"
cmake --build build --config Release
```

如果想实验半精度深度缓冲，可以开启：

```powershell
cmake -S . -B build-half -DHAO_RENDER_DEPTH_HALF=ON
cmake --build build-half --config Release
```

半精度可以降低 ZBuffer 和 shadow map 的内存带宽，但也可能带来深度精度问题。默认构建使用 `float` 深度缓冲，作为更稳的性能基线。

如果想实验半精度顶点存储，可以开启：

```powershell
cmake -S . -B build-vertex-half -DHAO_RENDER_VERTEX_HALF=ON
cmake --build build-vertex-half --config Release
```

这个选项会把加载后的 position、normal、tangent、bitangent、UV 存成 `Eigen::half`，然后在 CPU MVP 和着色路径里转回 `float` 使用。在普通 CPU 上它主要降低顶点内存带宽，不保证矩阵乘法一定更快，除非硬件和编译器能高效执行半精度运算。

如果想启用可选的 Embree 后端，可以这样构建：

```powershell
cmake -S . -B build-embree -DHAO_RENDER_ENABLE_EMBREE=ON -DEMBREE_INCLUDE_DIR="path\to\embree\include" -DEMBREE_LIBRARY="path\to\embree4.lib"
cmake --build build-embree --config Release
```

如果没有找到 Embree，项目也仍然可以正常构建，并自动继续使用原来的 shadow map 路径。

## 运行

进入构建输出目录后运行：

```powershell
.\myrender.exe
```

也可以传入模型路径：

```powershell
.\myrender.exe ..\Resources\MAIFU\IF.fbx
```

也可以在启动时直接指定阴影后端：

```powershell
.\myrender.exe ..\Resources\MAIFU\IF.fbx --shadow-technique=embree
```

`main.cpp` 中的默认模型路径是：

```text
../Resources/MAIFU/IF.fbx
```

## 操作方式

- 鼠标左键拖动：环绕观察
- 鼠标右键拖动：平移相机
- 鼠标滚轮：缩放
- `r`：重置相机
- `w`、`a`、`s`、`d`：快速旋转模型
- `1`、`2`、`3`：切换 PBR 通道映射预设
- `4`：切换到原本的 shadow map 阴影路径
- `5`：在 Embree 可用时切换到 `光栅化 + Embree` 阴影模式
- `Esc`：退出

## 当前说明

- 渲染器目前是 CPU 软渲染实现，很多管线阶段都保留在项目代码中，便于学习和调试。
- PBR 路径已经有基础实现，但很多模型只提供 diffuse 贴图，因此 Phong 路径仍然很重要。
- 阴影已经加入近远分层和 PCF 优化，但 shadow map 生成仍然是主要性能开销之一。
- 后续很适合扩展一套 GPU 后端，例如 OpenGL Renderer，复用当前的模型和材质加载流程。

## 目录结构

```text
include/        模型、渲染器、着色器、绘制工具和图像工具头文件
Resources/      示例模型和贴图资源
main.cpp        OpenCV 主循环和相机控制
model.cpp       Assimp 模型、材质、贴图加载
render.cpp      渲染调度、阴影 pass、tile 分块和帧渲染
shader.cpp      光栅化、裁剪、片元着色、阴影和 PBR/Phong 逻辑
Drawer.cpp      绘制辅助函数
tgaimage.cpp    TGA 图像支持
```

## License

目前还没有添加明确开源许可证。公开分发或接受外部贡献前，建议补充 LICENSE 文件。
