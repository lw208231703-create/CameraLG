#pragma once

#include "ICameraDevice.h"
#include <SapClassBasic.h>
#include <sapBuffer.h>
#include <QByteArray>
#include <QMutex>
#include <atomic>

class SaperaCameraDevice : public ICameraDevice {
    Q_OBJECT
public:
    static const QString NoServerFoundMessage;
    static const QString NoConfigFoundMessage;

    static QStringList getAvailableServers();
    static QStringList getConfigurationFiles(const QString& directory);

    explicit SaperaCameraDevice(QObject* parent = nullptr);
    ~SaperaCameraDevice() override;

    // ICameraDevice implementation
    bool openDevice(const QString& deviceId, const QString& configPath) override;
    void closeDevice() override;
    bool isOpened() const override;
    bool startGrabbing() override;
    bool stopGrabbing() override;
    bool grabOne() override;

    void setBitShift(int shift) override;
    int getBitShift() const override;
    QImage getDisplayImage(int bufferIndex) override;
    QVector<uint16_t> getRawData(int bufferIndex, int& width, int& height, int& bitDepth) override;
    bool saveImage(int bufferIndex, const QString& filePath) override;
    int getBufferCount() const override;
    void setBufferCount(int count) override;

    // Sapera-specific
    bool setImageGeometry(int width, int height, int x, int y);
    int getWidth() const;
    int getHeight() const;
    int getX() const;
    int getY() const;
    bool isGrabbing() const;

private:
    void processNewImage();
    static void sapXferCallback(SapXferCallbackInfo* pInfo);
    void rebuildBufferAndXfer();
    void createBufferAndXfer();
    void destroyXferAndBuffer();

    QByteArray m_serverNameBuffer;
    char* m_serverName{nullptr};
    SapAcquisition* m_acq{nullptr};
    SapBufferWithTrash* m_buffers{nullptr};
    SapTransfer* m_xfer{nullptr};

    QString m_selectedServer;
    QString m_configPath;
    int m_bufferCount{10};
    int m_currentBufferIndex{0};
    int m_bitShift{6};
    std::atomic<int> m_frameCount{0};
    mutable QMutex m_bufferMutex;
};
