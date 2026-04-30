#include "image_save_worker.h"
#include <QImage>
#include <QFile>
#include <QDataStream>
#include <QFileInfo>
#include <QImageWriter>

ImageSaveWorker::ImageSaveWorker(QObject *parent)
    : QObject(parent)
    , stopRequested_(false)
{
    // 使用 QueuedConnection 确保 processTasks 在工作线程中执行
    connect(this, &ImageSaveWorker::workRequested, this, &ImageSaveWorker::processTasks, Qt::QueuedConnection);
}

ImageSaveWorker::~ImageSaveWorker()
{
    stop();
}

void ImageSaveWorker::saveImage(const QVector<uint16_t> &data, int width, int height, int bitDepth, const QString &filePath, const QString &format)
{
    QMutexLocker locker(&mutex_);
    taskQueue_.enqueue({data, QImage(), width, height, bitDepth, filePath, format, false});
    
    // 触发处理
    emit workRequested();
}

void ImageSaveWorker::saveQImage(const QImage &image, const QString &filePath, const QString &format)
{
    QMutexLocker locker(&mutex_);
    taskQueue_.enqueue({QVector<uint16_t>(), image, image.width(), image.height(), image.depth(), filePath, format, true});
    
    // 触发处理
    emit workRequested();
}

void ImageSaveWorker::stop()
{
    QMutexLocker locker(&mutex_);
    stopRequested_ = true;
    condition_.wakeAll();
}

void ImageSaveWorker::processTasks()
{
    SaveTask task;
    {
        QMutexLocker locker(&mutex_);
        if (taskQueue_.isEmpty()) {
            emit allTasksFinished();
            return;
        }
        task = taskQueue_.dequeue();
    }

    if (stopRequested_) return;

    bool success = false;
    QString extension = QFileInfo(task.filePath).suffix().toLower();

    if (extension == "raw") {
        QFile file(task.filePath);
        if (file.open(QIODevice::WriteOnly)) {
            QDataStream stream(&file);
            stream.setByteOrder(QDataStream::LittleEndian);
            stream << static_cast<qint32>(task.width);
            stream << static_cast<qint32>(task.height);
            stream << static_cast<qint32>(task.bitDepth);
            
            // 写入数据
            if (task.useQImage) {
                // 如果是QImage但要求保存为RAW，这通常不应该发生，或者需要转换
                // 这里简单处理：不支持从QImage保存为自定义RAW格式
                success = false;
            } else {
                for (uint16_t val : task.data) {
                    stream << val;
                }
                success = true;
            }
        }
    } else {
        if (task.useQImage) {
            // 直接保存QImage
            // 检查格式是否支持
            QString formatUpper = task.format.toUpper();
            QList<QByteArray> supportedFormats = QImageWriter::supportedImageFormats();
            bool formatSupported = false;
            for (const QByteArray &fmt : supportedFormats) {
                if (QString::fromLatin1(fmt).toUpper() == formatUpper) {
                    formatSupported = true;
                    break;
                }
            }
            
            if (!formatSupported) {
                qWarning() << "Image format not supported:" << task.format 
                           << "Supported formats:" << supportedFormats;
                success = false;
            } else {
                // 使用大写格式字符串确保兼容性
                success = task.image.save(task.filePath, formatUpper.toLatin1().constData());
                if (!success) {
                    qWarning() << "Failed to save image to:" << task.filePath 
                               << "format:" << formatUpper;
                }
            }
        } else {
            // 转换为QImage保存
            QImage image(task.width, task.height, QImage::Format_Grayscale16);
            if (!task.data.isEmpty() && task.data.size() >= task.width * task.height) {
                for (int y = 0; y < task.height; ++y) {
                    uint16_t *line = reinterpret_cast<uint16_t*>(image.scanLine(y));
                    const uint16_t *src = task.data.constData() + y * task.width;
                    memcpy(line, src, task.width * sizeof(uint16_t));
                }
                QString formatUpper = task.format.toUpper();
                success = image.save(task.filePath, formatUpper.toLatin1().constData());
                if (!success) {
                    qWarning() << "Failed to save converted image to:" << task.filePath 
                               << "format:" << formatUpper;
                }
            }
        }
    }

    emit saveFinished(task.filePath, success);

    // 继续处理下一个任务
    {
        QMutexLocker locker(&mutex_);
        if (!taskQueue_.isEmpty()) {
            emit workRequested();
        }
    }
}
