#include "gige_camera_device.h"
#include "cameramanager.h"
#include "imagegrabber.h"
#include "image_depth_converter.h"
#include <QImageWriter>
#include <QFile>
#include <QDataStream>
#include <QFileInfo>
#include <QMutexLocker>
#include <opencv2/opencv.hpp>
#include <cstring>

GigECameraDevice::GigECameraDevice(QObject* parent)
    : ICameraDevice(parent)
    , m_cameraManager(new CameraManager(this))
    , m_imageGrabber(new ImageGrabber(this))
{
    connect(m_imageGrabber, &ImageGrabber::frameReceived,
            this, &GigECameraDevice::onFrameReceived);
    connect(m_cameraManager, &CameraManager::deviceConnected,
            this, &GigECameraDevice::onManagerDeviceConnected);
    connect(m_cameraManager, &CameraManager::deviceDisconnected,
            this, &GigECameraDevice::onManagerDeviceDisconnected);
    connect(m_cameraManager, &CameraManager::errorOccurred,
            this, &GigECameraDevice::onManagerError);

    m_softBuffers.resize(m_bufferCount);
}

GigECameraDevice::~GigECameraDevice()
{
    closeDevice();
}

bool GigECameraDevice::initializeSDK()
{
    return m_cameraManager->initialize();
}

void GigECameraDevice::finalizeSDK()
{
    m_cameraManager->finalize();
}

QList<GigECameraDevice::DeviceEntry> GigECameraDevice::enumerateDevices()
{
    QList<DeviceEntry> entries;
    auto devices = m_cameraManager->enumDevices();
    for (auto* info : devices) {
        if (!info) continue;
        DeviceEntry e;
        e.displayName = CameraManager::formatDeviceInfo(*info);
        e.internalId = info;
        entries.append(e);
    }
    return entries;
}

bool GigECameraDevice::openDevice(const QString& deviceId, const QString& /*configPath*/)
{
    (void)deviceId; // For GigE, device selection is done via enumerate → connect
    emit deviceError(tr("Use CameraManager directly for GigE connection control. "
                        "Call enumerateDevices() then cameraManager()->connectDevice()."));
    return false;
}

void GigECameraDevice::closeDevice()
{
    if (m_imageGrabber && m_imageGrabber->isGrabbing())
        m_imageGrabber->stopGrabbing();
    if (m_cameraManager && m_cameraManager->isConnected())
        m_cameraManager->disconnectDevice();

    QMutexLocker locker(&m_bufferMutex);
    for (auto& f : m_softBuffers)
        f.empty = true;
    m_currentBufferIndex = 0;
}

bool GigECameraDevice::isOpened() const
{
    return m_cameraManager && m_cameraManager->isConnected();
}

bool GigECameraDevice::startGrabbing()
{
    if (!m_cameraManager || !m_cameraManager->isConnected()) {
        emit deviceError(tr("Camera not connected"));
        return false;
    }
    void* handle = m_cameraManager->getHandle();
    if (m_imageGrabber->startGrabbing(handle)) {
        emit grabbingStarted();
        return true;
    }
    emit deviceError(tr("Failed to start GigE grabbing"));
    return false;
}

bool GigECameraDevice::stopGrabbing()
{
    if (m_imageGrabber && m_imageGrabber->isGrabbing()) {
        m_imageGrabber->stopGrabbing();
        emit grabbingStopped();
        return true;
    }
    return false;
}

bool GigECameraDevice::grabOne()
{
    // GigE: single frame via continuous mode with manual stop
    if (!m_cameraManager || !m_cameraManager->isConnected()) {
        emit deviceError(tr("Camera not connected"));
        return false;
    }
    // For single frame, we start and stop quickly
    return startGrabbing();
}

// --- Frame handling ---

void GigECameraDevice::onFrameReceived(const ImageData& image)
{
    ImageFrameData frame = convertFrame(image);

    {
        QMutexLocker locker(&m_bufferMutex);
        m_currentBufferIndex = (m_currentBufferIndex + 1) % m_bufferCount;

        SoftFrame& sf = m_softBuffers[m_currentBufferIndex];
        sf.raw16 = frame.rawData16;
        sf.raw8 = frame.rawData8;
        sf.displayImage = frame.displayImage;
        sf.width = frame.width;
        sf.height = frame.height;
        sf.bitDepth = frame.bitDepth;
        sf.empty = false;
    }

    emit frameReady(frame);
}

ImageFrameData GigECameraDevice::convertFrame(const ImageData& image)
{
    ImageFrameData frame;
    frame.width = image.frameInfo.nWidth;
    frame.height = image.frameInfo.nHeight;
    frame.frameIndex = m_frameIndex++;
    frame.timestamp = image.timestamp;

    int fmt = image.frameInfo.enPixelType;
    frame.bitDepth = getPixelBitDepth(fmt);

    const auto* data = reinterpret_cast<const unsigned char*>(image.data.constData());
    int pixelCount = frame.width * frame.height;

    if (frame.bitDepth <= 8) {
        auto vec = QSharedPointer<QVector<uint8_t>>::create(pixelCount);
        std::memcpy(vec->data(), data, pixelCount);
        frame.rawData8 = vec;
    } else {
        auto vec = QSharedPointer<QVector<uint16_t>>::create(pixelCount);
        std::memcpy(vec->data(), data, pixelCount * sizeof(uint16_t));
        frame.rawData16 = vec;
    }

    // Generate display image
    if (frame.bitDepth <= 8) {
        frame.displayImage = QImage(data, frame.width, frame.height,
                                    frame.width, QImage::Format_Grayscale8).copy();
    } else if (frame.bitDepth <= 16) {
        frame.displayImage = ImageDepthConverter::bitExtract(
            reinterpret_cast<const uint16_t*>(data),
            frame.width, frame.height,
            frame.width * (int)sizeof(uint16_t),
            frame.bitDepth, m_bitShift);
    }

    return frame;
}

int GigECameraDevice::getPixelBitDepth(int pixelFormat)
{
    // Mirror Hikvision pixel type constants
    switch (pixelFormat) {
    case 0x01080001: return 8;   // Mono8
    case 0x01080005: return 8;   // BayerRG8
    case 0x01080007: return 8;   // BayerGB8
    case 0x01080009: return 8;   // BayerGR8
    case 0x0108000B: return 8;   // BayerBG8
    case 0x02200001: return 8;   // RGB8_Packed
    case 0x01100003: return 10;  // Mono10
    case 0x01100005: return 12;  // Mono12
    case 0x01100007: return 14;  // Mono14
    case 0x01100009: return 16;  // Mono16
    default: return 8;
    }
}

// --- Image access ---

QImage GigECameraDevice::getDisplayImage(int bufferIndex)
{
    QMutexLocker locker(&m_bufferMutex);
    if (bufferIndex < 0 || bufferIndex >= m_softBuffers.size()) return {};
    const auto& sf = m_softBuffers[bufferIndex];
    if (sf.empty) return {};
    if (!sf.displayImage.isNull()) return sf.displayImage;

    // Reconstruct display image from raw data
    if (sf.bitDepth <= 8 && sf.raw8) {
        return QImage(sf.raw8->constData(), sf.width, sf.height,
                      sf.width, QImage::Format_Grayscale8).copy();
    }
    if (sf.raw16) {
        return ImageDepthConverter::bitExtract(
            sf.raw16->constData(), sf.width, sf.height,
            sf.width * (int)sizeof(uint16_t), sf.bitDepth, m_bitShift);
    }
    return {};
}

QVector<uint16_t> GigECameraDevice::getRawData(int bufferIndex, int& width, int& height, int& bitDepth)
{
    QMutexLocker locker(&m_bufferMutex);
    if (bufferIndex < 0 || bufferIndex >= m_softBuffers.size()) {
        width = height = bitDepth = 0;
        return {};
    }
    const auto& sf = m_softBuffers[bufferIndex];
    if (sf.empty) {
        width = height = bitDepth = 0;
        return {};
    }
    width = sf.width;
    height = sf.height;
    bitDepth = sf.bitDepth;

    if (sf.raw16) {
        return *sf.raw16;
    }
    if (sf.raw8) {
        QVector<uint16_t> converted(sf.raw8->size());
        for (int i = 0; i < sf.raw8->size(); ++i)
            converted[i] = static_cast<uint16_t>((*sf.raw8)[i]);
        return converted;
    }
    return {};
}

bool GigECameraDevice::saveImage(int bufferIndex, const QString& filePath)
{
    QMutexLocker locker(&m_bufferMutex);
    if (bufferIndex < 0 || bufferIndex >= m_softBuffers.size()) return false;
    const auto& sf = m_softBuffers[bufferIndex];
    if (sf.empty) return false;

    QString ext = QFileInfo(filePath).suffix().toLower();
    QString formatUpper = ext.toUpper();

    if (ext == "raw") {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) return false;
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << static_cast<qint32>(sf.width) << static_cast<qint32>(sf.height);
        if (sf.raw16) {
            stream.writeRawData(reinterpret_cast<const char*>(sf.raw16->constData()),
                                sf.raw16->size() * (int)sizeof(uint16_t));
        } else if (sf.raw8) {
            stream.writeRawData(reinterpret_cast<const char*>(sf.raw8->constData()),
                                sf.raw8->size());
        }
        file.close();
        return true;
    }

    QImage img = getDisplayImage(bufferIndex);
    if (!img.isNull())
        return img.save(filePath, formatUpper.toLatin1().constData());

    return false;
}

void GigECameraDevice::setBufferCount(int count)
{
    if (count < 1 || count > 100 || m_bufferCount == count) return;
    QMutexLocker locker(&m_bufferMutex);
    m_bufferCount = count;
    m_softBuffers.resize(m_bufferCount);
}

// --- Manager signal relays ---

void GigECameraDevice::onManagerDeviceConnected()
{
    emit deviceConnected();
}

void GigECameraDevice::onManagerDeviceDisconnected()
{
    emit deviceDisconnected();
}

void GigECameraDevice::onManagerError(const QString& error)
{
    emit deviceError(error);
}
