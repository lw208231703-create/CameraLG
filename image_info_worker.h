#ifndef IMAGE_INFO_WORKER_H
#define IMAGE_INFO_WORKER_H

#include <QObject>
#include <QImage>
#include <QPoint>
#include <QSize>
#include <QVector>
#include <QMutex>
#include <atomic>

struct ImageInfo {
    int firstPixelValue = 0;
    int mouseX = -1;
    int mouseY = -1;
    int pixelValue = -1;
    double fps = 0.0;
    int imageWidth = 0;
    int imageHeight = 0;
    QString colorType;
    int bitDepth = 0;
    bool valid = false;
};

int getGrayscale16Pixel(const QImage &image, int x, int y);

class ImageInfoWorker : public QObject
{
    Q_OBJECT
public:
    explicit ImageInfoWorker(QObject *parent = nullptr);
    
public slots:
    void updateImageInfo(const QImage &image, const QVector<uint16_t> &rawData, int rawWidth, int rawHeight, int bitDepth, const QPoint &mousePos, const QSize &displaySize, double fps, bool isImageCoords = false);
    void stop();
    
signals:
    void infoUpdated(const ImageInfo &info);
    
private:
    std::atomic<bool> shouldStop_{false};
    QMutex mutex_;
};

#endif
