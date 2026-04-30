# 开发者指南

[English](DEVELOPMENT_EN.md)

## 添加新相机支持

本项目使用 **Factory + Interface** 模式抽象相机设备。添加新相机只需 5 步。

### 架构概览

```
ICameraDevice (纯虚接口)
    ├── VirtualCameraDevice    — 虚拟测试相机
    ├── SaperaCameraDevice     — CameraLink (Sapera SDK)
    ├── GigECameraDevice       — GigE Vision (海康 MVS SDK)
    └── YourCameraDevice       — 你的新相机

CameraFactory::createCamera(CameraType) → ICameraDevice*
```

主窗口通过 `ICameraDevice` 接口与所有相机交互，不关心具体实现：
```
MainWindowRefactored
    └── cameraDevice_ : ICameraDevice*
            ├── openDevice() / closeDevice()
            ├── startGrabbing() / stopGrabbing()
            ├── frameReady(ImageFrameData)  ← 信号，每帧触发
            └── getDisplayImage() / getRawData()
```

### 第 1 步：创建设备类

创建头文件 `my_camera_device.h`：

```cpp
#pragma once

#include "ICameraDevice.h"
#include <QMutex>
#include <QSharedPointer>
#include <QVector>

class MyCameraDevice : public ICameraDevice {
    Q_OBJECT
public:
    explicit MyCameraDevice(QObject* parent = nullptr);
    ~MyCameraDevice() override;

    // === 必须实现的接口 ===

    // 打开设备
    // deviceId: 设备标识符（序列号、IP 地址、设备索引等，无则传 ""）
    // configPath: SDK 配置文件路径（如 Sapera .ccf 文件），无则传 ""
    // 如果你的相机不需要这些参数，直接忽略即可（参考 VirtualCameraDevice）
    bool openDevice(const QString& deviceId, const QString& configPath) override;

    // 关闭设备，释放所有资源
    void closeDevice() override;

    // 返回设备是否已打开
    bool isOpened() const override;

    // 开始连续采集。成功返回 true，失败返回 false
    bool startGrabbing() override;

    // 停止连续采集
    bool stopGrabbing() override;

    // 单帧采集。成功返回 true
    bool grabOne() override;

    // 设置/获取位移量（用于 16-bit → 8-bit 显示时选择有效位段）
    void setBitShift(int shift) override { m_bitShift = shift; }
    int getBitShift() const override { return m_bitShift; }

    // 获取指定缓冲区的显示图像（8-bit QImage，用于 UI 显示）
    QImage getDisplayImage(int bufferIndex = 0) override;

    // 获取指定缓冲区的原始数据（16-bit，用于分析和保存）
    QVector<uint16_t> getRawData(int bufferIndex, int& width, int& height, int& bitDepth) override;

    // 保存图像到文件
    bool saveImage(int bufferIndex, const QString& filePath) override;

    // 缓冲区管理
    int getBufferCount() const override { return m_bufferCount; }
    void setBufferCount(int count) override;

private:
    // 你的 SDK 相关成员
    int m_bitShift{6};
    int m_bufferCount{10};
    bool m_opened{false};
    mutable QMutex m_mutex;

    // 帧缓冲区（环形缓冲区）
    struct SoftFrame {
        QSharedPointer<QVector<uint16_t>> raw16;
        QImage displayImage;
        int width{0}, height{0}, bitDepth{0};
        bool empty{true};
    };
    QVector<SoftFrame> m_buffers;
    int m_currentIndex{0};
};
```

### 第 2 步：实现设备类

创建 `my_camera_device.cpp`，核心是 **帧数据填充** 和 **信号发射**：

```cpp
#include "my_camera_device.h"
#include "image_depth_converter.h"
#include <QImageWriter>
#include <QFileInfo>
#include <QMutexLocker>

MyCameraDevice::MyCameraDevice(QObject* parent)
    : ICameraDevice(parent)
{
    m_buffers.resize(m_bufferCount);
}

MyCameraDevice::~MyCameraDevice() {
    closeDevice();
}

bool MyCameraDevice::openDevice(const QString& deviceId, const QString& configPath)
{
    // 在这里初始化你的 SDK，连接硬件
    //
    // 参数说明：
    //   deviceId   — 设备标识符，可以是：
    //                 - 序列号 (如 "DEV_001")
    //                 - IP 地址 (如 "192.168.1.100")
    //                 - 设备索引 (如 "0")
    //                 - 空字符串 ""（表示使用默认设备）
    //   configPath — SDK 配置文件路径（如 Sapera 的 .ccf 文件）
    //                 大多数相机不需要配置文件，传 "" 即可
    //
    // 如果你的相机不需要这些参数，直接忽略：
    //   Q_UNUSED(deviceId)
    //   Q_UNUSED(configPath)

    // 示例 1：USB 相机，只需设备索引
    // int index = deviceId.isEmpty() ? 0 : deviceId.toInt();
    // if (!mySDK_Open(index)) return false;

    // 示例 2：网络相机，需要 IP 地址
    // if (!mySDK_Connect(deviceId.toStdString().c_str())) return false;

    // 示例 3：需要配置文件
    // if (!mySDK_LoadConfig(configPath.toStdString().c_str())) return false;

    m_opened = true;
    return true;
}

void MyCameraDevice::closeDevice()
{
    // 释放 SDK 资源
    // mySDK_Close();

    m_opened = false;
    QMutexLocker locker(&m_mutex);
    for (auto& f : m_buffers)
        f.empty = true;
}

bool MyCameraDevice::isOpened() const { return m_opened; }

bool MyCameraDevice::startGrabbing()
{
    if (!m_opened) return false;

    // 启动 SDK 采集
    // mySDK_StartGrabbing();

    // 注册回调函数，在回调中调用 frameReady()
    // mySDK_RegisterCallback([](void* ctx, MyFrame* frame) {
    //     auto* self = static_cast<MyCameraDevice*>(ctx);
    //     self->onFrameReceived(frame);
    // }, this);

    emit grabbingStarted();
    return true;
}

bool MyCameraDevice::stopGrabbing()
{
    // mySDK_StopGrabbing();
    emit grabbingStopped();
    return true;
}

bool MyCameraDevice::grabOne()
{
    if (!m_opened) return false;
    // mySDK_GrabOne();
    return true;
}

// === 关键：帧数据处理和信号发射 ===
// 在 SDK 回调中调用此方法
void MyCameraDevice::onFrameReceived(/* SDK 帧数据 */)
{
    // 1. 填充 ImageFrameData
    ImageFrameData frame;
    frame.width = /* 帧宽度 */;
    frame.height = /* 帧高度 */;
    frame.bitDepth = /* 位深，如 8, 12, 16 */;
    frame.channels = 1;  // 灰度=1，RGB=3

    // 2. 复制原始数据（必须深拷贝，因为 SDK 缓冲区会被回收）
    int pixelCount = frame.width * frame.height;
    auto vec = QSharedPointer<QVector<uint16_t>>::create(pixelCount);
    std::memcpy(vec->data(), /* SDK 数据指针 */, pixelCount * sizeof(uint16_t));
    frame.rawData16 = vec;

    // 3. 生成 8-bit 显示图像
    frame.displayImage = ImageDepthConverter::bitExtract(
        vec->constData(), frame.width, frame.height,
        frame.width * (int)sizeof(uint16_t),
        frame.bitDepth, m_bitShift);

    // 4. 存入环形缓冲区
    {
        QMutexLocker locker(&m_mutex);
        m_currentIndex = (m_currentIndex + 1) % m_bufferCount;
        SoftFrame& sf = m_buffers[m_currentIndex];
        sf.raw16 = vec;
        sf.displayImage = frame.displayImage;
        sf.width = frame.width;
        sf.height = frame.height;
        sf.bitDepth = frame.bitDepth;
        sf.empty = false;
    }

    // 5. 发射信号 — 主窗口会自动接收并显示
    emit frameReady(frame);
}

// 获取显示图像（与 VirtualCameraDevice 相同模式）
QImage MyCameraDevice::getDisplayImage(int bufferIndex)
{
    QMutexLocker locker(&m_mutex);
    if (bufferIndex < 0 || bufferIndex >= m_buffers.size()) return {};
    const auto& sf = m_buffers[bufferIndex];
    if (sf.empty) return {};
    if (!sf.displayImage.isNull()) return sf.displayImage;

    // 懒生成：如果之前没生成过，现在生成
    if (sf.raw16) {
        return ImageDepthConverter::bitExtract(
            sf.raw16->constData(), sf.width, sf.height,
            sf.width * (int)sizeof(uint16_t), sf.bitDepth, m_bitShift);
    }
    return {};
}

QVector<uint16_t> MyCameraDevice::getRawData(int bufferIndex, int& width, int& height, int& bitDepth)
{
    QMutexLocker locker(&m_mutex);
    if (bufferIndex < 0 || bufferIndex >= m_buffers.size()) {
        width = height = bitDepth = 0;
        return {};
    }
    const auto& sf = m_buffers[bufferIndex];
    if (sf.empty) {
        width = height = bitDepth = 0;
        return {};
    }
    width = sf.width;
    height = sf.height;
    bitDepth = sf.bitDepth;
    return sf.raw16 ? *sf.raw16 : QVector<uint16_t>();
}

bool MyCameraDevice::saveImage(int bufferIndex, const QString& filePath)
{
    QImage img = getDisplayImage(bufferIndex);
    if (img.isNull()) return false;
    QString ext = QFileInfo(filePath).suffix().toUpper();
    return img.save(filePath, ext.toLatin1().constData());
}

void MyCameraDevice::setBufferCount(int count)
{
    if (count < 1 || count > 100) return;
    QMutexLocker locker(&m_mutex);
    m_bufferCount = count;
    m_buffers.resize(m_bufferCount);
}
```

### 第 3 步：注册到工厂

编辑 `CameraFactory.h`，添加枚举值：

```cpp
enum class CameraType {
    CameraLink_Sapera,
    GigE_Hikvision,
    Virtual_Test,
    MyCamera          // ← 新增
};
```

编辑 `CameraFactory.cpp`，添加创建逻辑：

```cpp
#include "my_camera_device.h"  // 按需条件编译

ICameraDevice* CameraFactory::createCamera(CameraType type, QObject* parent)
{
    switch (type) {
    // ... 已有 case ...
    case CameraType::MyCamera:
        return new MyCameraDevice(parent);
    }
    return nullptr;
}
```

### 第 4 步：更新 CMakeLists.txt

```cmake
# 1. 可选检测你的 SDK
if(NOT DEFINED MY_SDK_ROOT)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../my_sdk")
        set(MY_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../my_sdk")
    endif()
endif()

if(DEFINED MY_SDK_ROOT AND EXISTS "${MY_SDK_ROOT}/include")
    find_path(MY_SDK_INCLUDE NAMES MySDK.h PATHS "${MY_SDK_ROOT}/include")
    if(MY_SDK_INCLUDE AND EXISTS "${MY_SDK_ROOT}/lib/MySDK.lib")
        message(STATUS "Found My SDK. Enabling MyCamera support.")
        set(USE_MY_CAMERA ON)
    else()
        set(USE_MY_CAMERA OFF)
    endif()
endif()

# 2. 条件编译
if(USE_MY_CAMERA)
    list(APPEND PROJECT_SOURCES
        my_camera_device.h
        my_camera_device.cpp
    )
    add_compile_definitions(ENABLE_MY_CAMERA)
endif()

# 3. 头文件和链接库
if(USE_MY_CAMERA)
    list(APPEND CAMERUI_INCLUDE_DIRS "${MY_SDK_ROOT}/include")
endif()

if(WIN32 AND USE_MY_CAMERA)
    target_link_directories(camerui PRIVATE "${MY_SDK_ROOT}/lib")
    target_link_libraries(camerui PRIVATE MySDK)
endif()
```

### 第 5 步：在主窗口中使用

编辑 `mainwindow_refactored.cpp`：

```cpp
#ifdef ENABLE_MY_CAMERA
#include "my_camera_device.h"
#endif

// 在构造函数中创建设备（作为备选方案）
MainWindowRefactored::MainWindowRefactored(QWidget *parent)
    : QMainWindow(parent)
{
    // ... 已有代码 ...

    // 优先级链：Sapera → MyCamera → Virtual
    cameraDevice_ = CameraFactory::createCamera(CameraFactory::CameraType::CameraLink_Sapera, this);
    if (!cameraDevice_) {
        cameraDevice_ = CameraFactory::createCamera(CameraFactory::CameraType::MyCamera, this);
    }
    if (!cameraDevice_) {
        cameraDevice_ = CameraFactory::createCamera(CameraFactory::CameraType::Virtual_Test, this);
    }

    // ... 后续连接信号槽 ...
}
```

### 关键注意事项

#### 线程安全
- SDK 回调通常在 **SDK 内部线程** 中触发
- `emit frameReady()` 是跨线程信号，Qt 会自动通过事件队列传递到 UI 线程
- `m_buffers` 的读写必须用 `QMutex` 保护

#### 内存管理
- `ImageFrameData::rawData16` 使用 `QSharedPointer` 共享所有权
- SDK 的帧缓冲区在回调返回后可能被回收，**必须深拷贝**
- `SoftFrame` 中的 `raw16` 与 `ImageFrameData::raw16` 共享同一块内存（零拷贝）

#### 位深转换
- `ImageDepthConverter::bitExtract()` 将 16-bit 数据提取为 8-bit 显示图像
- `m_bitShift` 控制提取哪 8 位（默认从 bit 6 开始，即 bit 6-13）
- 用户可通过 UI 的 `BitDepthSlider` 实时调整

#### 信号时序
```
SDK 回调线程                    UI 线程
    │                              │
    ├── onFrameReceived()          │
    │   ├── 深拷贝帧数据           │
    │   ├── 生成 displayImage      │
    │   ├── 存入 m_buffers         │
    │   └── emit frameReady()  ────┼──→ 自动通过 Qt 事件队列
    │                              │    到达 UI 线程
    │                              ├── MainWindow 接收 frameReady
    │                              ├── displayDock_->displayImage()
    │                              └── 更新直方图/统计信息
```
