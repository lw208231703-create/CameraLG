#include "imagegrabber.h"

#ifdef ENABLE_GIGE_CAMERA

#include <QElapsedTimer>
#include <QCoreApplication>
#include <QDateTime>
#include <QMetaObject>
#include <cstring>

ImageGrabber::ImageGrabber(QObject *parent)
    : QObject(parent)
    , m_handle(nullptr)
    , m_isGrabbing(false)
    , m_writeIndex(0)
    , m_readIndex(0)
    , m_bufferCount(0)
    , m_maxBufferSize(30)
    , m_totalFrames(0)
    , m_droppedFrames(0)
    , m_startTime(0)
    , m_lastFrameTime(0)
    , m_frameCountInSecond(0)
{
    qRegisterMetaType<ImageData>("ImageData");
    m_ringBuffer.resize(m_maxBufferSize);
}

ImageGrabber::~ImageGrabber()
{
    stopGrabbing();
    clearQueue();
}

bool ImageGrabber::startGrabbing(void* handle)
{
    if (handle == nullptr) {
        emit errorOccurred("Invalid handle");
        return false;
    }

    if (m_isGrabbing) {
        emit errorOccurred("Already grabbing");
        return false;
    }

    m_handle = handle;
    clearQueue();

    int nRet = MV_CC_RegisterImageCallBackEx(m_handle, imageCallback, this);
    if (nRet != MV_OK) {
        emit errorOccurred(QString("Register callback failed, error code: %1").arg(nRet));
        return false;
    }

    nRet = MV_CC_StartGrabbing(m_handle);
    if (nRet != MV_OK) {
        MV_CC_RegisterImageCallBackEx(m_handle, nullptr, nullptr);
        emit errorOccurred(QString("Start grabbing failed, error code: %1").arg(nRet));
        return false;
    }

    m_isGrabbing = true;
    m_totalFrames = 0;
    m_droppedFrames = 0;
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_lastFrameTime = m_startTime;
    m_frameCountInSecond = 0;

    return true;
}

void ImageGrabber::stopGrabbing()
{
    if (!m_isGrabbing) {
        return;
    }

    m_isGrabbing = false;

    if (m_handle != nullptr) {
        MV_CC_StopGrabbing(m_handle);
        MV_CC_RegisterImageCallBackEx(m_handle, nullptr, nullptr);
    }

    m_bufferCondition.wakeAll();
}

bool ImageGrabber::isGrabbing() const
{
    return m_isGrabbing;
}

void ImageGrabber::updateHandle(void* handle)
{
    if (m_isGrabbing) {
        stopGrabbing();
    }
    m_handle = handle;
}

void __stdcall ImageGrabber::imageCallback(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{
    if (pUser == nullptr || pFrameInfo == nullptr) {
        return;
    }

    ImageGrabber* grabber = static_cast<ImageGrabber*>(pUser);
    grabber->processFrame(pData, pFrameInfo);
}

void ImageGrabber::processFrame(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo)
{
    QMutexLocker locker(&m_bufferMutex);

    if (!m_isGrabbing) {
        return;
    }

    ImageData& imageData = m_ringBuffer[m_writeIndex];

    unsigned int bufferSize = pFrameInfo->nFrameLen;
    imageData.data = QByteArray(reinterpret_cast<const char*>(pData), bufferSize);
    
    imageData.frameInfo = *pFrameInfo;
    imageData.timestamp = QDateTime::currentMSecsSinceEpoch();
    imageData.frameNumber = pFrameInfo->nFrameNum;

    int currentIndex = m_writeIndex;
    m_writeIndex = (m_writeIndex + 1) % m_maxBufferSize;

    if (m_bufferCount < m_maxBufferSize) {
        m_bufferCount++;
    } else {
        m_readIndex = (m_readIndex + 1) % m_maxBufferSize;
        m_droppedFrames++;
    }

    m_totalFrames++;
    m_frameCountInSecond++;

    updateStatistics();

    locker.unlock();

    emit frameReceived(m_ringBuffer[currentIndex]);
}

bool ImageGrabber::getNextImage(ImageData& image, int timeoutMs)
{
    QMutexLocker locker(&m_bufferMutex);

    if (m_bufferCount == 0) {
        if (!m_bufferCondition.wait(&m_bufferMutex, timeoutMs)) {
            return false;
        }
    }

    if (m_bufferCount == 0) {
        return false;
    }

    image = m_ringBuffer[m_readIndex];
    m_ringBuffer[m_readIndex].data.clear();
    
    m_readIndex = (m_readIndex + 1) % m_maxBufferSize;
    m_bufferCount--;

    return true;
}

void ImageGrabber::releaseImageData(const ImageData& image)
{
    Q_UNUSED(image);
}

int ImageGrabber::getQueueSize() const
{
    QMutexLocker locker(&m_bufferMutex);
    return m_bufferCount;
}

void ImageGrabber::clearQueue()
{
    QMutexLocker locker(&m_bufferMutex);
    
    for (int i = 0; i < m_maxBufferSize; i++) {
        m_ringBuffer[i].data.clear();
    }
    
    m_writeIndex = 0;
    m_readIndex = 0;
    m_bufferCount = 0;
}

void ImageGrabber::setMaxQueueSize(int size)
{
    QMutexLocker locker(&m_bufferMutex);
    
    int newSize = qMax(10, size);
    
    if (newSize == m_maxBufferSize) {
        return;
    }

    clearQueue();
    m_maxBufferSize = newSize;
    m_ringBuffer.resize(m_maxBufferSize);
}

int ImageGrabber::getMaxQueueSize() const
{
    QMutexLocker locker(&m_bufferMutex);
    return m_maxBufferSize;
}

unsigned long long ImageGrabber::getTotalFrames() const
{
    QMutexLocker locker(&m_bufferMutex);
    return m_totalFrames;
}

unsigned long long ImageGrabber::getDroppedFrames() const
{
    QMutexLocker locker(&m_bufferMutex);
    return m_droppedFrames;
}

double ImageGrabber::getFrameRate() const
{
    QMutexLocker locker(&m_bufferMutex);
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = currentTime - m_startTime;
    
    if (elapsed <= 0) {
        return 0.0;
    }
    
    return (m_totalFrames * 1000.0) / elapsed;
}

void ImageGrabber::updateStatistics()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = currentTime - m_lastFrameTime;
    
    if (elapsed >= 1000) {
        m_frameCountInSecond = 0;
        m_lastFrameTime = currentTime;
    }
}

#endif // ENABLE_GIGE_CAMERA