# CameraLG - 工业相机图像采集与分析平台

[English](README_EN.md)

基于 C++17 / Qt 6 的工业机器视觉相机应用，支持图像采集、实时显示、噪声分析和 30+ 种图像处理算法，部分算法支持 CUDA GPU 加速。

## 功能特性

- **多相机支持**：CameraLink (Sapera SDK)、GigE Vision (海康 MVS SDK)、虚拟测试相机
- **实时图像显示**：缩放、平移、ROI 选区、像素信息追踪、坏点标记
- **图像处理算法**（30+ 种）：
  - 滤波：高斯、中值、双边、均值
  - 边缘检测：Canny、Sobel、Laplacian、Scharr
  - 形态学：腐蚀、膨胀、开运算、闭运算等
  - 阈值分割：二值化、自适应、Otsu
  - 增强：伽马校正、直方图均衡、CLAHE、锐化
  - 频域：低通/高通滤波（理想、高斯、巴特沃斯）
  - 几何变换：旋转、平移、缩放、翻转
  - 分割：K-Means、轮廓检测、分水岭
- **噪声分析**：多增益噪声测试、3D 表面可视化
- **光斑检测**：卡尔曼滤波追踪、稳定性分析
- **流水线处理**：拖拽式算法组合，支持 8-bit / 16-bit 精度切换
- **深色主题**：Visual Studio 风格暗色 UI

## 界面预览

<!-- TODO: 添加截图或 GIF 演示 -->
<!-- ![主界面](docs/screenshot_main.png) -->
<!-- ![图像处理流水线](docs/screenshot_pipeline.png) -->

## 系统要求

| 依赖 | 版本要求 | 必需 |
|------|---------|------|
| CMake | >= 3.16 | 是 |
| Qt 6 | >= 6.5 (Widgets, OpenGL, OpenGLWidgets, Concurrent) | 是 |
| OpenCV | >= 4.x (推荐 CUDA 版本) | 是 |
| QXlsx | 最新版 | 是 |
| CUDA Toolkit | >= 11.0 (可选，GPU 加速) | 否 |
| Sapera SDK | — (可选，CameraLink 相机) | 否 |
| 海康 MVS SDK | — (可选，GigE 相机) | 否 |
| MSVC | 2022 (Windows) | 是 (Windows) |

## 快速开始

### 1. 克隆项目

```bash
git clone https://github.com/lw208231703-create/camera_prj_release.git
cd camera_prj_release
```

### 2. 准备依赖

**方式一：同级目录布局（默认）**

将依赖放在项目上级目录：
```
workspace/
├── camerui/              # 本项目
├── opencv-cuda-build/    # OpenCV 构建目录
├── QXlsx/QXlsx/          # QXlsx 库
├── sapera/               # Sapera SDK (可选)
├── MVS/                  # 海康 MVS SDK (可选)
├── CUDA/                 # CUDA Toolkit (可选)
└── CUDNN/                # cuDNN (可选)
```

**方式二：自定义路径**

通过 CMake 变量指定：
```bash
cmake -B build \
  -DOpenCV_DIR=/path/to/opencv \
  -DQXlsx_DIR=/path/to/QXlsx/QXlsx \
  -DSAPERA_SDK_DIR=/path/to/sapera \
  -DMVS_ROOT=/path/to/MVS \
  -DCUDAToolkit_ROOT=/path/to/cuda
```

### 3. 构建

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

或使用 CMake Presets（需要 Qt 6.10+）：
```bash
cmake --preset default
cmake --build --preset release
```

### 4. 运行

构建产物位于 `build/AppDist/Release/CameraLinkFrame.exe`。

## CMake 配置选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `ANALYZE_RAW_DATA` | ON | 分析原始 16-bit 采集数据 |
| `DISPLAYDOCK_ENABLE_RENDER_THROTTLE` | OFF | 启用显示渲染节流 |
| `DISPLAYDOCK_ENABLE_PARTIAL_PAINT` | ON | 启用局部绘制渲染路径 |
| `FORCE_2D_MODE` | ON | 在 3D 控件中强制 2D 模式 |
| `NOISE3D_STACK_VOXEL_MODE` | ON | 噪声 3D 视图的体素堆叠模式 |
| `PIXEL_VALUE_3D_RENDER_STYLE` | 0 | 3D 渲染样式 (0=曲面, 1=散点, 2=柱状) |

## 项目架构

```
camerui/
├── ICameraDevice.h          # 相机设备抽象接口
├── CameraFactory.h/cpp      # 相机工厂（按类型创建设备实例）
├── ImageFrameData.h         # 图像帧数据结构（共享所有权）
├── virtual_camera_device.*  # 虚拟测试相机
├── sapera_camera_device.*   # CameraLink 相机实现
├── gige_camera_device.*     # GigE 相机实现
├── cameramanager.*          # 海康 MVS SDK 封装
├── imagegrabber.*           # 环形缓冲区图像采集
├── mainwindow_refactored.*  # 主窗口（拆分为多个 .cpp）
├── display_dock.*           # 图像显示面板
├── image_data_dock.*        # 直方图与统计面板
├── image_algorithm_dock.*   # 算法选择面板
├── image_algorithm_base.*   # 算法基类与工厂
├── image_algorithms_*.h     # 算法实现（滤波/边缘/形态学/...）
├── thread_manager.*         # 集中式线程管理
├── mixed_processing_panel.* # 流水线处理面板
└── dark_theme.qss           # 深色主题样式
```

### 设计模式

- **Factory + Interface**：`ICameraDevice` 抽象接口 + `CameraFactory` 工厂创建
- **Template Method**：`ImageAlgorithmBase::processImpl()` 由各算法子类实现
- **Producer-Consumer**：Worker 线程 + 任务队列
- **Facade**：`ThreadManager` 统一管理线程生命周期

## 开发者指南

需要添加新相机支持？请参考 [DEVELOPMENT.md](DEVELOPMENT.md)（[English](DEVELOPMENT_EN.md)），包含完整的 5 步扩展教程和代码模板。

## 致谢

- 图标来源：[iconfont.cn](https://www.iconfont.cn/) — 请参阅各图标作者的许可条款
- 图像处理：[OpenCV](https://opencv.org/)
- UI 框架：[Qt 6](https://www.qt.io/)
- Excel 导出：[QXlsx](https://github.com/j2doll/QXlsx)

## 开源许可

本项目基于 [MIT 许可证](LICENSE) 开源。
