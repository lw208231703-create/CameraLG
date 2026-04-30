#pragma once

#include "ICameraDevice.h"
#include <QList>
#include <QImage>
#include <QVector>
#include <QMutex>
#include <QSharedPointer>

class CameraManager;
class ImageGrabber;
struct ImageData;

class GigECameraDevice : public ICameraDevice {
    Q_OBJECT
public:
    struct DeviceEntry {
        QString displayName;
        void* internalId;  // MV_CC_DEVICE_INFO*
    };

    explicit GigECameraDevice(QObject* parent = nullptr);
    ~GigECameraDevice() override;

    bool initializeSDK();
    void finalizeSDK();
    QList<DeviceEntry> enumerateDevices();

    // ICameraDevice
    bool openDevice(const QString& deviceId, const QString& configPath) override;
    void closeDevice() override;
    bool isOpened() const override;
    bool startGrabbing() override;
    bool stopGrabbing() override;
    bool grabOne() override;

    void setBitShift(int shift) override { m_bitShift = shift; }
    int getBitShift() const override { return m_bitShift; }
    QImage getDisplayImage(int bufferIndex) override;
    QVector<uint16_t> getRawData(int bufferIndex, int& width, int& height, int& bitDepth) override;
    bool saveImage(int bufferIndex, const QString& filePath) override;
    int getBufferCount() const override { return m_bufferCount; }
    void setBufferCount(int count) override;

    CameraManager* cameraManager() const { return m_cameraManager; }
    ImageGrabber* imageGrabber() const { return m_imageGrabber; }

private slots:
    void onFrameReceived(const ImageData& image);
    void onManagerDeviceConnected();
    void onManagerDeviceDisconnected();
    void onManagerError(const QString& error);

signals:
    void deviceConnected();
    void deviceDisconnected();

private:
    ImageFrameData convertFrame(const ImageData& image);
    int getPixelBitDepth(int pixelFormat);

    CameraManager* m_cameraManager = nullptr;
    ImageGrabber* m_imageGrabber = nullptr;

    int m_bitShift{6};
    int m_bufferCount{10};
    mutable QMutex m_bufferMutex;

    struct SoftFrame {
        QSharedPointer<QVector<uint16_t>> raw16;
        QSharedPointer<QVector<uint8_t>> raw8;
        QImage displayImage;
        int width{0}, height{0}, bitDepth{0};
        bool empty{true};
    };
    QVector<SoftFrame> m_softBuffers;
    int m_currentBufferIndex{0};
    int m_frameIndex{0};
};
