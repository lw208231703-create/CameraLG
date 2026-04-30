# CameraLG - Industrial Camera Image Acquisition & Analysis Platform

[中文](README.md)

A C++17 / Qt 6 industrial machine vision application for image acquisition, real-time display, noise analysis, and 30+ image processing algorithms with optional CUDA GPU acceleration.

## Features

- **Multi-camera support**: CameraLink (Sapera SDK), GigE Vision (Hikvision MVS SDK), Virtual test camera
- **Real-time image display**: Zoom, pan, ROI selection, pixel info tracking, bad pixel markers
- **Image processing algorithms** (30+):
  - Filtering: Gaussian, Median, Bilateral, Box
  - Edge detection: Canny, Sobel, Laplacian, Scharr
  - Morphology: Erosion, Dilation, Opening, Closing, etc.
  - Thresholding: Binary, Adaptive, Otsu
  - Enhancement: Gamma correction, Histogram equalization, CLAHE, Sharpening
  - Frequency domain: Low-pass/High-pass (Ideal, Gaussian, Butterworth)
  - Geometry: Rotate, Translate, Scale, Flip
  - Segmentation: K-Means, Contour detection, Watershed
- **Noise analysis**: Multi-gain noise testing, 3D surface visualization
- **Spot detection**: Kalman filter tracking, stability analysis
- **Pipeline processing**: Drag-and-drop algorithm composition, 8-bit / 16-bit precision toggle
- **Dark theme**: Visual Studio-style dark UI

## Screenshots

<!-- TODO: Add screenshots or GIF demos -->
<!-- ![Main Interface](docs/screenshot_main.png) -->
<!-- ![Image Processing Pipeline](docs/screenshot_pipeline.png) -->

## Requirements

| Dependency | Version | Required |
|-----------|---------|----------|
| CMake | >= 3.16 | Yes |
| Qt 6 | >= 6.5 (Widgets, OpenGL, OpenGLWidgets, Concurrent) | Yes |
| OpenCV | >= 4.x (CUDA build recommended) | Yes |
| QXlsx | Latest | Yes |
| CUDA Toolkit | >= 11.0 (optional, GPU acceleration) | No |
| Sapera SDK | — (optional, CameraLink cameras) | No |
| Hikvision MVS SDK | — (optional, GigE cameras) | No |
| MSVC | 2022 (Windows) | Yes (Windows) |

## Quick Start

### 1. Clone the project

```bash
git clone https://github.com/lw208231703-create/camera_prj_release.git
cd camera_prj_release
```

### 2. Prepare dependencies

**Option A: Sibling directory layout (default)**

Place dependencies in the parent directory:
```
workspace/
├── camerui/              # This project
├── opencv-cuda-build/    # OpenCV build directory
├── QXlsx/QXlsx/          # QXlsx library
├── sapera/               # Sapera SDK (optional)
├── MVS/                  # Hikvision MVS SDK (optional)
├── CUDA/                 # CUDA Toolkit (optional)
└── CUDNN/                # cuDNN (optional)
```

**Option B: Custom paths**

Specify via CMake variables:
```bash
cmake -B build \
  -DOpenCV_DIR=/path/to/opencv \
  -DQXlsx_DIR=/path/to/QXlsx/QXlsx \
  -DSAPERA_SDK_DIR=/path/to/sapera \
  -DMVS_ROOT=/path/to/MVS \
  -DCUDAToolkit_ROOT=/path/to/cuda
```

### 3. Build

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Or use CMake Presets (requires Qt 6.10+):
```bash
cmake --preset default
cmake --build --preset release
```

### 4. Run

The build output is at `build/AppDist/Release/CameraLinkFrame.exe`.

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `ANALYZE_RAW_DATA` | ON | Analyze raw 16-bit acquisition data |
| `DISPLAYDOCK_ENABLE_RENDER_THROTTLE` | OFF | Enable display render throttle |
| `DISPLAYDOCK_ENABLE_PARTIAL_PAINT` | ON | Enable partial paint render path |
| `FORCE_2D_MODE` | ON | Force 2D mode in 3D widgets |
| `NOISE3D_STACK_VOXEL_MODE` | ON | Voxel stack mode for noise 3D view |
| `PIXEL_VALUE_3D_RENDER_STYLE` | 0 | 3D render style (0=surface, 1=scatter, 2=bars) |

## Project Architecture

```
camerui/
├── ICameraDevice.h          # Camera device abstract interface
├── CameraFactory.h/cpp      # Camera factory (creates device by type)
├── ImageFrameData.h         # Image frame data structure (shared ownership)
├── virtual_camera_device.*  # Virtual test camera
├── sapera_camera_device.*   # CameraLink camera implementation
├── gige_camera_device.*     # GigE camera implementation
├── cameramanager.*          # Hikvision MVS SDK wrapper
├── imagegrabber.*           # Ring buffer image acquisition
├── mainwindow_refactored.*  # Main window (split into multiple .cpp)
├── display_dock.*           # Image display panel
├── image_data_dock.*        # Histogram & statistics panel
├── image_algorithm_dock.*   # Algorithm selection panel
├── image_algorithm_base.*   # Algorithm base class & factory
├── image_algorithms_*.h     # Algorithm implementations (filter/edge/morph/...)
├── thread_manager.*         # Centralized thread management
├── mixed_processing_panel.* # Pipeline processing panel
└── dark_theme.qss           # Dark theme stylesheet
```

### Design Patterns

- **Factory + Interface**: `ICameraDevice` abstract interface + `CameraFactory`
- **Template Method**: `ImageAlgorithmBase::processImpl()` overridden by each algorithm
- **Producer-Consumer**: Worker threads with task queues
- **Facade**: `ThreadManager` as single point of thread lifecycle management

## Developer Guide

Need to add new camera support? See [DEVELOPMENT_EN.md](DEVELOPMENT_EN.md) ([中文](DEVELOPMENT.md)) for a complete 5-step extension tutorial with code templates.

## Acknowledgements

- Icons: [iconfont.cn](https://www.iconfont.cn/) — please check individual icon author licenses
- Image processing: [OpenCV](https://opencv.org/)
- UI framework: [Qt 6](https://www.qt.io/)
- Excel export: [QXlsx](https://github.com/j2doll/QXlsx)

## License

This project is licensed under the [MIT License](LICENSE).
