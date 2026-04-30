#pragma once

#include <QMetaType>
#include <QImage>
#include <QSharedPointer>
#include <QVector>
#include <cstdint>

struct ImageFrameData {
    int width = 0;
    int height = 0;
    int bitDepth = 0;
    int channels = 1;
    int64_t frameIndex = 0;
    int64_t timestamp = 0;

    // Shared ownership avoids deep copies — multiple receivers share the same allocation
    QSharedPointer<QVector<uint16_t>> rawData16;
    QSharedPointer<QVector<uint8_t>> rawData8;

    // QImage is implicitly shared (COW), cheap to pass by value
    QImage displayImage;

    bool isValid() const { return width > 0 && height > 0; }
    bool is16Bit() const { return bitDepth > 8; }

    const uint16_t* data16() const { return rawData16 ? rawData16->constData() : nullptr; }
    const uint8_t* data8() const { return rawData8 ? rawData8->constData() : nullptr; }
    int pixelCount() const { return width * height; }

    static ImageFrameData fromRaw16(const uint16_t* src, int w, int h, int bd, int ch = 1);
    static ImageFrameData fromRaw8(const uint8_t* src, int w, int h, int bd, int ch = 1);
};

Q_DECLARE_METATYPE(ImageFrameData)
