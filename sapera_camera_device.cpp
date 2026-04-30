#include "sapera_camera_device.h"
#include "image_depth_converter.h"
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QThread>
#include <QImageWriter>
#include <QFile>
#include <QDataStream>
#include <cstring>

const QString SaperaCameraDevice::NoServerFoundMessage = QStringLiteral("No Acquisition Server Found");
const QString SaperaCameraDevice::NoConfigFoundMessage = QStringLiteral("No Configuration Files Found");

void SaperaCameraDevice::sapXferCallback(SapXferCallbackInfo* pInfo)
{
    auto* self = static_cast<SaperaCameraDevice*>(pInfo->GetContext());
    if (self) {
        self->processNewImage();
    }
}

SaperaCameraDevice::SaperaCameraDevice(QObject* parent)
    : ICameraDevice(parent)
{
}

SaperaCameraDevice::~SaperaCameraDevice()
{
    closeDevice();
}

// --- Static utilities ---

QStringList SaperaCameraDevice::getAvailableServers()
{
    QStringList servers;
    int count = SapManager::GetServerCount(SapManager::ResourceAcq);
    for (int i = 0; i < count; ++i) {
        char name[CORSERVER_MAX_STRLEN];
        if (SapManager::GetServerName(i, SapManager::ResourceAcq, name)) {
            servers.append(QString::fromLatin1(name));
        }
    }
    if (servers.isEmpty())
        servers.append(NoServerFoundMessage);
    return servers;
}

QStringList SaperaCameraDevice::getConfigurationFiles(const QString& directory)
{
    QStringList files;
    QDir dir(directory);
    if (dir.exists()) {
        QStringList filters{"*.ccf", "*.CCF"};
        dir.setNameFilters(filters);
        const auto list = dir.entryInfoList(QDir::Files | QDir::Readable);
        for (const auto& fi : list)
            files.append(fi.absoluteFilePath());
    }
    if (files.isEmpty())
        files.append(NoConfigFoundMessage);
    return files;
}

// --- ICameraDevice implementation ---

bool SaperaCameraDevice::openDevice(const QString& deviceId, const QString& configPath)
{
    m_selectedServer = deviceId;
    m_configPath = configPath;

    if (m_selectedServer.isEmpty() || m_selectedServer == NoServerFoundMessage) {
        emit deviceError(tr("No acquisition server available"));
        return false;
    }
    if (m_configPath.isEmpty() || m_configPath == NoConfigFoundMessage) {
        emit deviceError(tr("No configuration file selected"));
        return false;
    }

    m_serverNameBuffer = m_selectedServer.toLatin1();
    m_serverName = m_serverNameBuffer.data();

    QByteArray serverBytes = m_selectedServer.toLatin1();
    QByteArray ccfBytes = m_configPath.toLatin1();
    SapLocation loc(serverBytes.constData(), 0);

    int resCount = SapManager::GetResourceCount(serverBytes.constData(), SapManager::ResourceAcq);
    if (resCount <= 0) {
        emit deviceError(tr("No acquisition resources available"));
        return false;
    }

    m_acq = new SapAcquisition(loc, ccfBytes.constData());
    createBufferAndXfer();

    if (m_acq && !*m_acq && !m_acq->Create()) {
        emit deviceError(tr("Failed to create acquisition object"));
        closeDevice();
        return false;
    }
    if (m_buffers && !*m_buffers) {
        if (!m_buffers->Create()) {
            emit deviceError(tr("Failed to create buffer object"));
            closeDevice();
            return false;
        }
        m_buffers->Clear();
    }
    if (m_xfer && m_xfer->GetPair(0)) {
        m_xfer->GetPair(0)->SetCycleMode(SapXferPair::CycleNextWithTrash);
    }
    if (m_xfer && !*m_xfer && !m_xfer->Create()) {
        emit deviceError(tr("Failed to create transfer object"));
        closeDevice();
        return false;
    }

    QThread::msleep(150);
    return true;
}

void SaperaCameraDevice::closeDevice()
{
    m_serverNameBuffer.clear();
    m_serverName = nullptr;

    if (m_xfer && m_xfer->IsGrabbing())
        stopGrabbing();

    destroyXferAndBuffer();

    if (m_acq) {
        m_acq->Destroy();
        delete m_acq;
        m_acq = nullptr;
    }
}

bool SaperaCameraDevice::isOpened() const
{
    return m_acq != nullptr && m_buffers != nullptr;
}

bool SaperaCameraDevice::startGrabbing()
{
    if (!m_xfer) {
        emit deviceError(tr("Transfer object not initialized"));
        return false;
    }
    m_frameCount = 0;
    if (!m_xfer->IsGrabbing()) {
        if (m_xfer->Grab()) {
            emit grabbingStarted();
            return true;
        }
        emit deviceError(tr("Failed to start continuous grab"));
        return false;
    }
    return true;
}

bool SaperaCameraDevice::stopGrabbing()
{
    if (!m_xfer) return false;
    if (m_xfer->IsGrabbing()) {
        if (m_xfer->Freeze()) {
            if (!m_xfer->Wait(5000))
                m_xfer->Abort();
            emit grabbingStopped();
            return true;
        }
    }
    return !m_xfer->IsGrabbing();
}

bool SaperaCameraDevice::grabOne()
{
    if (!m_xfer) {
        emit deviceError(tr("Transfer object not initialized"));
        return false;
    }
    m_frameCount = 0;
    if (m_xfer->IsGrabbing()) {
        m_xfer->Freeze();
        if (!m_xfer->Wait(1000))
            m_xfer->Abort();
        return false;
    }
    if (m_xfer->Snap(1)) {
        if (m_xfer->Wait(1000))
            return true;
    }
    emit deviceError(tr("Failed to capture single frame"));
    return false;
}

void SaperaCameraDevice::processNewImage()
{
    if (!m_buffers) return;

    int bufferIndex = m_buffers->GetIndex();
    m_currentBufferIndex = bufferIndex % m_bufferCount;

    // Build ImageFrameData from Sapera buffer
    m_buffers->SetIndex(bufferIndex);
    int w = m_buffers->GetWidth();
    int h = m_buffers->GetHeight();
    int pd = m_buffers->GetPixelDepth();
    int pitch = m_buffers->GetPitch();

    void* pData = nullptr;
    if (!m_buffers->GetAddress(&pData) || !pData) return;

    int pixelCount = w * h;

    ImageFrameData frame;
    frame.width = w;
    frame.height = h;
    frame.bitDepth = pd;
    frame.channels = 1;
    frame.frameIndex = m_frameCount.load();

    if (pd <= 8) {
        auto vec = QSharedPointer<QVector<uint8_t>>::create(pixelCount);
        const char* byteData = static_cast<const char*>(pData);
        for (int y = 0; y < h; ++y)
            std::memcpy(vec->data() + y * w, byteData + y * pitch, w);
        frame.rawData8 = vec;
    } else {
        auto vec = QSharedPointer<QVector<uint16_t>>::create(pixelCount);
        const char* byteData = static_cast<const char*>(pData);
        for (int y = 0; y < h; ++y)
            std::memcpy(vec->data() + y * w, byteData + y * pitch, w * sizeof(uint16_t));
        frame.rawData16 = vec;
    }

    m_buffers->ReleaseAddress(pData);

    emit frameReady(frame);
    m_frameCount++;
}

// --- Bit shift ---

int SaperaCameraDevice::getBitShift() const { return m_bitShift; }

void SaperaCameraDevice::setBitShift(int shift)
{
    if (shift < 0) shift = 0;
    if (shift > 8) shift = 8;
    m_bitShift = shift;
}

// --- Image access ---

QImage SaperaCameraDevice::getDisplayImage(int bufferIndex)
{
    QMutexLocker locker(&m_bufferMutex);
    if (!m_buffers) return {};

    m_buffers->SetIndex(bufferIndex);
    int w = m_buffers->GetWidth();
    int h = m_buffers->GetHeight();
    int pitch = m_buffers->GetPitch();
    int pd = m_buffers->GetPixelDepth();

    void* pData = nullptr;
    if (!m_buffers->GetAddress(&pData) || !pData) return {};

    QImage result;
    if (pd == 8) {
        QImage tmp(static_cast<const uchar*>(pData), w, h, pitch, QImage::Format_Grayscale8);
        result = tmp.copy();
    } else if (pd > 8 && pd <= 16) {
        result = ImageDepthConverter::bitExtract(
            static_cast<const uint16_t*>(pData), w, h, pitch, pd, m_bitShift);
    } else if (pd == 24 || pd == 32) {
        QImage::Format fmt = (pd == 24) ? QImage::Format_RGB888 : QImage::Format_ARGB32;
        QImage tmp(static_cast<const uchar*>(pData), w, h, pitch, fmt);
        result = tmp.copy();
    }

    m_buffers->ReleaseAddress(pData);
    return result;
}

QVector<uint16_t> SaperaCameraDevice::getRawData(int bufferIndex, int& width, int& height, int& bitDepth)
{
    QMutexLocker locker(&m_bufferMutex);
    if (!m_buffers) {
        width = height = bitDepth = 0;
        return {};
    }

    m_buffers->SetIndex(bufferIndex);
    width = m_buffers->GetWidth();
    height = m_buffers->GetHeight();
    bitDepth = m_buffers->GetPixelDepth();

    QVector<uint16_t> raw(width * height);

    if (bitDepth > 8 && bitDepth <= 16) {
        if (m_buffers->ReadRect(0, 0, width, height, raw.data()))
            return raw;

        void* pData = nullptr;
        if (m_buffers->GetAddress(&pData) && pData) {
            int pitch = m_buffers->GetPitch();
            const char* byteData = static_cast<const char*>(pData);
            for (int y = 0; y < height; ++y) {
                const auto* src = reinterpret_cast<const uint16_t*>(byteData + y * pitch);
                std::copy(src, src + width, raw.begin() + y * width);
            }
            m_buffers->ReleaseAddress(pData);
        }
    } else if (bitDepth == 8) {
        void* pData = nullptr;
        if (m_buffers->GetAddress(&pData) && pData) {
            int pitch = m_buffers->GetPitch();
            // Fill 16-bit vector from 8-bit source
            const char* byteData = static_cast<const char*>(pData);
            for (int y = 0; y < height; ++y) {
                const auto* src = reinterpret_cast<const uint8_t*>(byteData + y * pitch);
                for (int x = 0; x < width; ++x)
                    raw[y * width + x] = static_cast<uint16_t>(src[x]);
            }
            m_buffers->ReleaseAddress(pData);
        }
    }
    return raw;
}

bool SaperaCameraDevice::saveImage(int bufferIndex, const QString& filePath)
{
    QMutexLocker locker(&m_bufferMutex);
    if (!m_buffers) return false;

    QString ext = QFileInfo(filePath).suffix().toLower();
    QString formatUpper = ext.toUpper();

    // Try Sapera native save first
    QMap<QString, QString> saperaFormats = {
        {"bmp", "-format bmp"}, {"tif", "-format tiff"}, {"tiff", "-format tiff"},
        {"jpg", "-format jpeg"}, {"jpeg", "-format jpeg"}, {"raw", "-format raw"},
        {"crc", "-format crc"}, {"avi", "-format avi"}
    };

    if (saperaFormats.contains(ext)) {
        m_buffers->SetIndex(bufferIndex);
        QByteArray pathBytes = filePath.toLatin1();
        QByteArray optBytes = saperaFormats[ext].toLatin1();
        if (m_buffers->Save(pathBytes.constData(), optBytes.constData()))
            return true;
    }

    // Fallback via QImage
    QList<QByteArray> supported = QImageWriter::supportedImageFormats();
    bool qtOk = false;
    for (const auto& f : supported) {
        if (QString::fromLatin1(f).toLower() == ext) { qtOk = true; break; }
    }
    if (!qtOk) return false;

    int w = 0, h = 0, bd = 0;
    QVector<uint16_t> raw = getRawData(bufferIndex, w, h, bd);
    QImage image;

    if (!raw.isEmpty() && bd > 8) {
        QImage img16(w, h, QImage::Format_Grayscale16);
        for (int y = 0; y < h; ++y)
            std::memcpy(img16.scanLine(y), raw.constData() + y * w, w * sizeof(uint16_t));
        if (img16.save(filePath, formatUpper.toLatin1().constData()))
            return true;
    }

    image = getDisplayImage(bufferIndex);
    return !image.isNull() && image.save(filePath, formatUpper.toLatin1().constData());
}

int SaperaCameraDevice::getBufferCount() const { return m_bufferCount; }

void SaperaCameraDevice::setBufferCount(int count)
{
    if (count < 1 || count > 100 || m_bufferCount == count) return;

    bool wasGrabbing = isGrabbing();

    destroyXferAndBuffer();
    m_bufferCount = count;
    createBufferAndXfer();

    if (wasGrabbing)
        startGrabbing();
}

// --- Sapera-specific methods ---

bool SaperaCameraDevice::setImageGeometry(int width, int height, int x, int y)
{
    if (!m_acq) return false;

    bool wasGrabbing = isGrabbing();
    if (wasGrabbing) stopGrabbing();

    destroyXferAndBuffer();

    bool ok = true;
    ok &= m_acq->SetParameter(CORACQ_PRM_CROP_WIDTH, width);
    ok &= m_acq->SetParameter(CORACQ_PRM_CROP_HEIGHT, height);
    ok &= m_acq->SetParameter(CORACQ_PRM_CROP_LEFT, x);
    ok &= m_acq->SetParameter(CORACQ_PRM_CROP_TOP, y);

    if (!ok) {
        emit deviceError(tr("Failed to set image geometry"));
        return false;
    }

    createBufferAndXfer();

    if (wasGrabbing) startGrabbing();
    return true;
}

int SaperaCameraDevice::getWidth() const
{
    if (!m_acq) return 0;
    int v = 0;
    m_acq->GetParameter(CORACQ_PRM_CROP_WIDTH, &v);
    return v;
}

int SaperaCameraDevice::getHeight() const
{
    if (!m_acq) return 0;
    int v = 0;
    m_acq->GetParameter(CORACQ_PRM_CROP_HEIGHT, &v);
    return v;
}

int SaperaCameraDevice::getX() const
{
    if (!m_acq) return 0;
    int v = 0;
    m_acq->GetParameter(CORACQ_PRM_CROP_LEFT, &v);
    return v;
}

int SaperaCameraDevice::getY() const
{
    if (!m_acq) return 0;
    int v = 0;
    m_acq->GetParameter(CORACQ_PRM_CROP_TOP, &v);
    return v;
}

bool SaperaCameraDevice::isGrabbing() const
{
    return m_xfer && m_xfer->IsGrabbing();
}

// --- Private helpers ---

void SaperaCameraDevice::createBufferAndXfer()
{
    if (!m_acq) return;
    m_buffers = new SapBufferWithTrash(m_bufferCount, m_acq);
    m_xfer = new SapAcqToBuf(m_acq, m_buffers, sapXferCallback, this);
}

void SaperaCameraDevice::destroyXferAndBuffer()
{
    if (m_xfer) {
        m_xfer->Destroy();
        delete m_xfer;
        m_xfer = nullptr;
    }
    if (m_buffers) {
        m_buffers->Destroy();
        delete m_buffers;
        m_buffers = nullptr;
    }
}
