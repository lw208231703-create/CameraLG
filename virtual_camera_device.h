#pragma once

#include "ICameraDevice.h"
#include <QTimer>
#include <QMutex>
#include <QSharedPointer>
#include <QVector>

class VirtualCameraDevice : public ICameraDevice {
    Q_OBJECT
public:
    explicit VirtualCameraDevice(QObject* parent = nullptr);
    ~VirtualCameraDevice() override;

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

    void setResolution(int w, int h) { m_width = w; m_height = h; }

private slots:
    void generateFrame();

private:
    void fillTestPattern(QVector<uint16_t>& buf, int w, int h);

    QTimer* m_timer = nullptr;
    int m_width{640};
    int m_height{512};
    int m_bitShift{6};
    int m_bufferCount{10};
    int m_frameIndex{0};
    bool m_opened{false};

    struct SoftFrame {
        QSharedPointer<QVector<uint16_t>> raw16;
        QImage displayImage;
        int width{0}, height{0}, bitDepth{12};
        bool empty{true};
    };
    QVector<SoftFrame> m_buffers;
    int m_currentIndex{0};
    mutable QMutex m_mutex;
};
