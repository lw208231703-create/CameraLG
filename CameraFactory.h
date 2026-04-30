#pragma once

#include "ICameraDevice.h"

class CameraFactory {
public:
    enum class CameraType {
        CameraLink_Sapera,
        GigE_Hikvision,
        Virtual_Test
    };

    static ICameraDevice* createCamera(CameraType type, QObject* parent = nullptr);

    // Register ImageFrameData metatype — call once at startup
    static void registerMetaTypes();

private:
    CameraFactory() = default;
};
