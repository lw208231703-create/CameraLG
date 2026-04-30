#pragma once

#ifdef ENABLE_GIGE_CAMERA

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QImage>
#include <QVector>
#include <QByteArray>
#include "MvCameraControl.h"
#include <QMetaType>

struct ImageData {
    QByteArray data;
    MV_FRAME_OUT_INFO_EX frameInfo;
    int64_t timestamp;
    unsigned int frameNumber;
};
Q_DECLARE_METATYPE(ImageData)

class ImageGrabber : public QObject
{
    Q_OBJECT

public:
    explicit ImageGrabber(QObject *parent = nullptr);
    ~ImageGrabber();

    bool startGrabbing(void* handle);
    void stopGrabbing();
    bool isGrabbing() const;
    void updateHandle(void* handle);

    bool getNextImage(ImageData& image, int timeoutMs = 1000);
    void releaseImageData(const ImageData& image);
    int getQueueSize() const;
    void clearQueue();

    void setMaxQueueSize(int size);
    int getMaxQueueSize() const;

    unsigned long long getTotalFrames() const;
    unsigned long long getDroppedFrames() const;
    double getFrameRate() const;

signals:
    void frameReceived(const ImageData& image);
    void errorOccurred(const QString& error);

private:
    static void __stdcall imageCallback(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser);

    void processFrame(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo);
    void updateStatistics();

    void* m_handle;
    bool m_isGrabbing;

    QVector<ImageData> m_ringBuffer;
    int m_writeIndex;
    int m_readIndex;
    int m_bufferCount;
    mutable QMutex m_bufferMutex;
    QWaitCondition m_bufferCondition;
    int m_maxBufferSize;

    unsigned long long m_totalFrames;
    unsigned long long m_droppedFrames;
    qint64 m_startTime;
    qint64 m_lastFrameTime;
    int m_frameCountInSecond;
};

#endif // ENABLE_GIGE_CAMERA
