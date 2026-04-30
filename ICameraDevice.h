#pragma once

#include <QObject>
#include <QString>
#include "ImageFrameData.h"

class ICameraDevice : public QObject {
    Q_OBJECT
public:
    explicit ICameraDevice(QObject* parent = nullptr) : QObject(parent) {}
    ~ICameraDevice() override = default;

    virtual bool openDevice(const QString& deviceId, const QString& configPath) = 0;
    virtual void closeDevice() = 0;
    virtual bool isOpened() const = 0;

    virtual bool startGrabbing() = 0;
    virtual bool stopGrabbing() = 0;
    virtual bool grabOne() = 0;

    virtual void setBitShift(int shift) = 0;
    virtual int getBitShift() const = 0;

    virtual QImage getDisplayImage(int bufferIndex = 0) = 0;
    virtual QVector<uint16_t> getRawData(int bufferIndex, int& width, int& height, int& bitDepth) = 0;

    virtual bool saveImage(int bufferIndex, const QString& filePath) = 0;
    virtual int getBufferCount() const = 0;
    virtual void setBufferCount(int count) = 0;

signals:
    void frameReady(const ImageFrameData& frame);
    void deviceError(const QString& errorMessage);
    void grabbingStarted();
    void grabbingStopped();
};
