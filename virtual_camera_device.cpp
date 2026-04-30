#include "virtual_camera_device.h"
#include "image_depth_converter.h"
#include <QImageWriter>
#include <QFileInfo>
#include <QMutexLocker>
#include <cmath>
#include <cstring>

VirtualCameraDevice::VirtualCameraDevice(QObject* parent)
    : ICameraDevice(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &VirtualCameraDevice::generateFrame);
    m_buffers.resize(m_bufferCount);
}

VirtualCameraDevice::~VirtualCameraDevice()
{
    closeDevice();
}

bool VirtualCameraDevice::openDevice(const QString& /*deviceId*/, const QString& /*configPath*/)
{
    m_opened = true;
    return true;
}

void VirtualCameraDevice::closeDevice()
{
    m_timer->stop();
    m_opened = false;
    QMutexLocker locker(&m_mutex);
    for (auto& f : m_buffers)
        f.empty = true;
}

bool VirtualCameraDevice::isOpened() const { return m_opened; }

bool VirtualCameraDevice::startGrabbing()
{
    if (!m_opened) return false;
    m_timer->start(40); // ~25 fps
    emit grabbingStarted();
    return true;
}

bool VirtualCameraDevice::stopGrabbing()
{
    m_timer->stop();
    emit grabbingStopped();
    return true;
}

bool VirtualCameraDevice::grabOne()
{
    if (!m_opened) return false;
    generateFrame();
    return true;
}

void VirtualCameraDevice::generateFrame()
{
    int pixelCount = m_width * m_height;
    auto vec = QSharedPointer<QVector<uint16_t>>::create(pixelCount);
    fillTestPattern(*vec, m_width, m_height);

    ImageFrameData frame;
    frame.width = m_width;
    frame.height = m_height;
    frame.bitDepth = 12;
    frame.channels = 1;
    frame.frameIndex = m_frameIndex++;
    frame.rawData16 = vec;

    // Generate 8-bit display image
    frame.displayImage = ImageDepthConverter::bitExtract(
        vec->constData(), m_width, m_height,
        m_width * (int)sizeof(uint16_t), 12, m_bitShift);

    {
        QMutexLocker locker(&m_mutex);
        m_currentIndex = (m_currentIndex + 1) % m_bufferCount;
        SoftFrame& sf = m_buffers[m_currentIndex];
        sf.raw16 = vec;
        sf.displayImage = frame.displayImage;
        sf.width = m_width;
        sf.height = m_height;
        sf.bitDepth = 12;
        sf.empty = false;
    }

    emit frameReady(frame);
}

void VirtualCameraDevice::fillTestPattern(QVector<uint16_t>& buf, int w, int h)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Gradient + checkerboard pattern
            int gradient = (int)((double)x / w * 4095.0);
            int checker = ((x / 32 + y / 32) % 2) * 500;
            buf[y * w + x] = static_cast<uint16_t>(
                std::min(4095, std::max(0, gradient + checker)));
        }
    }
}

QImage VirtualCameraDevice::getDisplayImage(int bufferIndex)
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

QVector<uint16_t> VirtualCameraDevice::getRawData(int bufferIndex, int& width, int& height, int& bitDepth)
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

bool VirtualCameraDevice::saveImage(int bufferIndex, const QString& filePath)
{
    QImage img = getDisplayImage(bufferIndex);
    if (img.isNull()) return false;
    QString ext = QFileInfo(filePath).suffix().toUpper();
    return img.save(filePath, ext.toLatin1().constData());
}

void VirtualCameraDevice::setBufferCount(int count)
{
    if (count < 1 || count > 100) return;
    QMutexLocker locker(&m_mutex);
    m_bufferCount = count;
    m_buffers.resize(m_bufferCount);
}
