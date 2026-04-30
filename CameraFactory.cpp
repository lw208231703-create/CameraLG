#include "CameraFactory.h"
#include "ImageFrameData.h"
#include "virtual_camera_device.h"
#ifdef ENABLE_SAPERA_CAMERA
#include "sapera_camera_device.h"
#endif
#ifdef ENABLE_GIGE_CAMERA
#include "gige_camera_device.h"
#endif
#include <cstring>

// ImageFrameData static helpers

ImageFrameData ImageFrameData::fromRaw16(const uint16_t* src, int w, int h, int bd, int ch)
{
    ImageFrameData frame;
    frame.width = w;
    frame.height = h;
    frame.bitDepth = bd;
    frame.channels = ch;

    int count = w * h * ch;
    auto vec = QSharedPointer<QVector<uint16_t>>::create(count);
    std::memcpy(vec->data(), src, count * sizeof(uint16_t));
    frame.rawData16 = vec;
    return frame;
}

ImageFrameData ImageFrameData::fromRaw8(const uint8_t* src, int w, int h, int bd, int ch)
{
    ImageFrameData frame;
    frame.width = w;
    frame.height = h;
    frame.bitDepth = bd;
    frame.channels = ch;

    int count = w * h * ch;
    auto vec = QSharedPointer<QVector<uint8_t>>::create(count);
    std::memcpy(vec->data(), src, count * sizeof(uint8_t));
    frame.rawData8 = vec;
    return frame;
}

// CameraFactory

void CameraFactory::registerMetaTypes()
{
    qRegisterMetaType<ImageFrameData>("ImageFrameData");
}

ICameraDevice* CameraFactory::createCamera(CameraType type, QObject* parent)
{
    switch (type) {
    case CameraType::CameraLink_Sapera:
#ifdef ENABLE_SAPERA_CAMERA
        return new SaperaCameraDevice(parent);
#else
        return nullptr;
#endif

    case CameraType::GigE_Hikvision:
#ifdef ENABLE_GIGE_CAMERA
        return new GigECameraDevice(parent);
#else
        return nullptr;
#endif

    case CameraType::Virtual_Test:
        return new VirtualCameraDevice(parent);
    }

    return nullptr;
}
