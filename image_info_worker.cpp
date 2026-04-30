#include "image_info_worker.h"

int getGrayscale16Pixel(const QImage &image, int x, int y)
{
    if (image.format() != QImage::Format_Grayscale16) {
        return -1;
    }
    
    if (x < 0 || x >= image.width() || y < 0 || y >= image.height()) {
        return -1;
    }
    
    const uchar *scanLine = image.constScanLine(y);
    if (!scanLine) {
        return -1;
    }
    
    constexpr qsizetype bytesPerPixel = sizeof(uint16_t);
    qsizetype requiredBytes = static_cast<qsizetype>(x + 1) * bytesPerPixel;
    if (image.bytesPerLine() < requiredBytes) {
        return -1;
    }
    
    const uint16_t *data = reinterpret_cast<const uint16_t*>(scanLine);
    return static_cast<int>(data[x]);
}

ImageInfoWorker::ImageInfoWorker(QObject *parent)
    : QObject(parent)
{
}

void ImageInfoWorker::updateImageInfo(const QImage &image, const QVector<uint16_t> &rawData, int rawWidth, int rawHeight, int bitDepth, const QPoint &mousePos, const QSize &displaySize, double fps, bool isImageCoords)
{
    if (shouldStop_.load()) {
        return;
    }
    
    QMutexLocker locker(&mutex_);
    
    ImageInfo info;
    
    if (image.isNull() && rawData.isEmpty()) {
        info.valid = false;
        emit infoUpdated(info);
        return;
    }
    
    info.valid = true;
    info.fps = fps;
    
    bool hasRawData = !rawData.isEmpty() && rawWidth > 0 && rawHeight > 0;
    
    if (hasRawData) {
        info.imageWidth = rawWidth;
        info.imageHeight = rawHeight;
        info.bitDepth = bitDepth > 0 ? bitDepth : 14;
        info.colorType = "Grayscale (Raw)";
    } else {
        info.imageWidth = image.width();
        info.imageHeight = image.height();
        info.bitDepth = image.depth();
        
        switch (image.format()) {
            case QImage::Format_Grayscale8:
            case QImage::Format_Grayscale16:
                info.colorType = "Grayscale";
                break;
            case QImage::Format_RGB888:
                info.colorType = "RGB";
                break;
            case QImage::Format_ARGB32:
            case QImage::Format_ARGB32_Premultiplied:
                info.colorType = "ARGB";
                break;
            case QImage::Format_RGB32:
                info.colorType = "RGB32";
                break;
            default:
                info.colorType = QString("Format_%1").arg(static_cast<int>(image.format()));
                break;
        }
    }
    
    if (hasRawData) {
        if (rawData.size() > 0) {
            info.firstPixelValue = rawData[0];
        }
    } else if (image.width() > 0 && image.height() > 0) {
        if (image.format() == QImage::Format_Grayscale8) {
            info.firstPixelValue = qGray(image.pixel(0, 0));
        } else if (image.format() == QImage::Format_Grayscale16) {
            int pixelValue = getGrayscale16Pixel(image, 0, 0);
            if (pixelValue >= 0) {
                info.firstPixelValue = pixelValue;
            }
        } else {
            QRgb pixel = image.pixel(0, 0);
            info.firstPixelValue = qGray(pixel);
        }
    }
    
    if (mousePos.x() >= 0 && mousePos.y() >= 0) {
        int width = hasRawData ? rawWidth : image.width();
        int height = hasRawData ? rawHeight : image.height();
        
        int origX = -1;
        int origY = -1;

        if (isImageCoords) {
            origX = mousePos.x();
            origY = mousePos.y();
        } else if (displaySize.width() > 0 && displaySize.height() > 0) {
            double scaleX = static_cast<double>(width) / static_cast<double>(displaySize.width());
            double scaleY = static_cast<double>(height) / static_cast<double>(displaySize.height());

            origX = static_cast<int>(std::floor(mousePos.x() * scaleX));
            origY = static_cast<int>(std::floor(mousePos.y() * scaleY));
        }

        if (origX >= 0 && origX < width && origY >= 0 && origY < height) {
            info.mouseX = origX;
            info.mouseY = origY;
            
            if (hasRawData) {
                int index = origY * rawWidth + origX;
                if (index >= 0 && index < rawData.size()) {
                    info.pixelValue = rawData[index];
                }
            } else {
                if (image.format() == QImage::Format_Grayscale8) {
                    info.pixelValue = qGray(image.pixel(origX, origY));
                } else if (image.format() == QImage::Format_Grayscale16) {
                    int pixelValue = getGrayscale16Pixel(image, origX, origY);
                    if (pixelValue >= 0) {
                        info.pixelValue = pixelValue;
                    }
                } else {
                    QRgb pixel = image.pixel(origX, origY);
                    info.pixelValue = qGray(pixel);
                }
            }
        } else {
            info.mouseX = -1;
            info.mouseY = -1;
            info.pixelValue = -1;
        }
    } else {
        info.mouseX = -1;
        info.mouseY = -1;
        info.pixelValue = -1;
    }
    
    emit infoUpdated(info);
}

void ImageInfoWorker::stop()
{
    shouldStop_.store(true);
}
