# Developer Guide

[中文](DEVELOPMENT.md)

## Adding New Camera Support

This project uses the **Factory + Interface** pattern for camera abstraction. Adding a new camera takes 5 steps.

### Architecture Overview

```
ICameraDevice (pure virtual interface)
    ├── VirtualCameraDevice    — Virtual test camera
    ├── SaperaCameraDevice     — CameraLink (Sapera SDK)
    ├── GigECameraDevice       — GigE Vision (Hikvision MVS SDK)
    └── YourCameraDevice       — Your new camera

CameraFactory::createCamera(CameraType) → ICameraDevice*
```

The main window interacts with all cameras through `ICameraDevice`, with no knowledge of specific implementations:
```
MainWindowRefactored
    └── cameraDevice_ : ICameraDevice*
            ├── openDevice() / closeDevice()
            ├── startGrabbing() / stopGrabbing()
            ├── frameReady(ImageFrameData)  ← signal, emitted per frame
            └── getDisplayImage() / getRawData()
```

### Step 1: Create the Device Class

Create header file `my_camera_device.h`:

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

    // === Required interface methods ===

    // Open device
    // deviceId: device identifier (serial number, IP, device index, etc.) — pass "" if not needed
    // configPath: SDK config file path (e.g. Sapera .ccf file) — pass "" if not needed
    // If your camera doesn't need these, just ignore them (see VirtualCameraDevice)
    bool openDevice(const QString& deviceId, const QString& configPath) override;
    void closeDevice() override;
    bool isOpened() const override;

    bool startGrabbing() override;
    bool stopGrabbing() override;
    bool grabOne() override;

    void setBitShift(int shift) override { m_bitShift = shift; }
    int getBitShift() const override { return m_bitShift; }

    QImage getDisplayImage(int bufferIndex = 0) override;
    QVector<uint16_t> getRawData(int bufferIndex, int& width, int& height, int& bitDepth) override;
    bool saveImage(int bufferIndex, const QString& filePath) override;

    int getBufferCount() const override { return m_bufferCount; }
    void setBufferCount(int count) override;

private:
    int m_bitShift{6};
    int m_bufferCount{10};
    bool m_opened{false};
    mutable QMutex m_mutex;

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

### Step 2: Implement the Device Class

Create `my_camera_device.cpp`. The core is **frame data filling** and **signal emission**:

```cpp
#include "my_camera_device.h"
#include "image_depth_converter.h"
#include <QImageWriter>
#include <QFileInfo>
#include <QMutexLocker>
#include <cstring>

MyCameraDevice::MyCameraDevice(QObject* parent)
    : ICameraDevice(parent)
{
    m_buffers.resize(m_bufferCount);
}

MyCameraDevice::~MyCameraDevice() { closeDevice(); }

bool MyCameraDevice::openDevice(const QString& deviceId, const QString& configPath)
{
    // Initialize your SDK and connect to hardware
    //
    // Parameters:
    //   deviceId   — Device identifier, can be:
    //                 - Serial number (e.g. "DEV_001")
    //                 - IP address (e.g. "192.168.1.100")
    //                 - Device index (e.g. "0")
    //                 - Empty string "" (use default device)
    //   configPath — SDK config file path (e.g. Sapera .ccf file)
    //                 Most cameras don't need a config file, just pass ""
    //
    // If your camera doesn't need these parameters, just ignore them:
    //   Q_UNUSED(deviceId)
    //   Q_UNUSED(configPath)

    // Example 1: USB camera, only needs device index
    // int index = deviceId.isEmpty() ? 0 : deviceId.toInt();
    // if (!mySDK_Open(index)) return false;

    // Example 2: Network camera, needs IP address
    // if (!mySDK_Connect(deviceId.toStdString().c_str())) return false;

    // Example 3: Needs config file
    // if (!mySDK_LoadConfig(configPath.toStdString().c_str())) return false;

    m_opened = true;
    return true;
}

void MyCameraDevice::closeDevice()
{
    // mySDK_Close();
    m_opened = false;
    QMutexLocker locker(&m_mutex);
    for (auto& f : m_buffers) f.empty = true;
}

bool MyCameraDevice::isOpened() const { return m_opened; }

bool MyCameraDevice::startGrabbing()
{
    if (!m_opened) return false;
    // mySDK_StartGrabbing();
    // Register SDK callback → call onFrameReceived()
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

// === Key: frame processing and signal emission ===
// Call this from your SDK callback
void MyCameraDevice::onFrameReceived(/* SDK frame data */)
{
    ImageFrameData frame;
    frame.width = /* frame width */;
    frame.height = /* frame height */;
    frame.bitDepth = /* e.g. 8, 12, 16 */;
    frame.channels = 1;  // grayscale=1, RGB=3

    // Deep copy — SDK buffer may be reclaimed after callback returns
    int pixelCount = frame.width * frame.height;
    auto vec = QSharedPointer<QVector<uint16_t>>::create(pixelCount);
    std::memcpy(vec->data(), /* SDK data pointer */, pixelCount * sizeof(uint16_t));
    frame.rawData16 = vec;

    // Generate 8-bit display image
    frame.displayImage = ImageDepthConverter::bitExtract(
        vec->constData(), frame.width, frame.height,
        frame.width * (int)sizeof(uint16_t),
        frame.bitDepth, m_bitShift);

    // Store in ring buffer
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

    // Emit signal — main window will receive and display automatically
    emit frameReady(frame);
}

QImage MyCameraDevice::getDisplayImage(int bufferIndex)
{
    QMutexLocker locker(&m_mutex);
    if (bufferIndex < 0 || bufferIndex >= m_buffers.size()) return {};
    const auto& sf = m_buffers[bufferIndex];
    if (sf.empty) return {};
    if (!sf.displayImage.isNull()) return sf.displayImage;
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
        width = height = bitDepth = 0; return {};
    }
    const auto& sf = m_buffers[bufferIndex];
    if (sf.empty) { width = height = bitDepth = 0; return {}; }
    width = sf.width; height = sf.height; bitDepth = sf.bitDepth;
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

### Step 3: Register with the Factory

Edit `CameraFactory.h` — add enum value:

```cpp
enum class CameraType {
    CameraLink_Sapera,
    GigE_Hikvision,
    Virtual_Test,
    MyCamera          // ← new
};
```

Edit `CameraFactory.cpp` — add creation logic:

```cpp
#include "my_camera_device.h"  // optionally guarded by #ifdef

ICameraDevice* CameraFactory::createCamera(CameraType type, QObject* parent)
{
    switch (type) {
    // ... existing cases ...
    case CameraType::MyCamera:
        return new MyCameraDevice(parent);
    }
    return nullptr;
}
```

### Step 4: Update CMakeLists.txt

```cmake
# 1. Optional SDK detection
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

# 2. Conditional compilation
if(USE_MY_CAMERA)
    list(APPEND PROJECT_SOURCES
        my_camera_device.h
        my_camera_device.cpp
    )
    add_compile_definitions(ENABLE_MY_CAMERA)
endif()

# 3. Include dirs and link libraries
if(USE_MY_CAMERA)
    list(APPEND CAMERUI_INCLUDE_DIRS "${MY_SDK_ROOT}/include")
endif()

if(WIN32 AND USE_MY_CAMERA)
    target_link_directories(camerui PRIVATE "${MY_SDK_ROOT}/lib")
    target_link_libraries(camerui PRIVATE MySDK)
endif()
```

### Step 5: Use in Main Window

Edit `mainwindow_refactored.cpp`:

```cpp
#ifdef ENABLE_MY_CAMERA
#include "my_camera_device.h"
#endif

// In constructor — add as fallback
cameraDevice_ = CameraFactory::createCamera(CameraFactory::CameraType::CameraLink_Sapera, this);
if (!cameraDevice_) {
    cameraDevice_ = CameraFactory::createCamera(CameraFactory::CameraType::MyCamera, this);
}
if (!cameraDevice_) {
    cameraDevice_ = CameraFactory::createCamera(CameraFactory::CameraType::Virtual_Test, this);
}
```

### Key Notes

#### Thread Safety
- SDK callbacks typically fire on **SDK internal threads**
- `emit frameReady()` is a cross-thread signal — Qt automatically dispatches it to the UI thread via the event queue
- `m_buffers` read/write must be protected with `QMutex`

#### Memory Management
- `ImageFrameData::rawData16` uses `QSharedPointer` for shared ownership
- SDK frame buffers may be reclaimed after callback returns — **you must deep copy**
- `SoftFrame::raw16` shares the same allocation as `ImageFrameData::raw16` (zero-copy)

#### Bit Depth Conversion
- `ImageDepthConverter::bitExtract()` extracts 8-bit display image from 16-bit data
- `m_bitShift` controls which 8 bits to extract (default: bit 6-13)
- Users can adjust in real-time via the `BitDepthSlider` UI widget

#### Signal Timing
```
SDK Callback Thread              UI Thread
    │                              │
    ├── onFrameReceived()          │
    │   ├── Deep copy frame data   │
    │   ├── Generate displayImage  │
    │   ├── Store in m_buffers     │
    │   └── emit frameReady()  ────┼──→ Qt event queue dispatch
    │                              │    to UI thread
    │                              ├── MainWindow receives frameReady
    │                              ├── displayDock_->displayImage()
    │                              └── Update histogram/statistics
```
