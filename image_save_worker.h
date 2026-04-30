#ifndef IMAGE_SAVE_WORKER_H
#define IMAGE_SAVE_WORKER_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

#include <QImage>

struct SaveTask {
    QVector<uint16_t> data;
    QImage image; // 新增：支持直接保存QImage
    int width;
    int height;
    int bitDepth;
    QString filePath;
    QString format;
    bool useQImage; // 标记是否使用QImage
};

class ImageSaveWorker : public QObject
{
    Q_OBJECT
public:
    explicit ImageSaveWorker(QObject *parent = nullptr);
    ~ImageSaveWorker();

public slots:
    void saveImage(const QVector<uint16_t> &data, int width, int height, int bitDepth, const QString &filePath, const QString &format);
    void saveQImage(const QImage &image, const QString &filePath, const QString &format); // 新增接口
    void stop();

signals:
    void saveFinished(const QString &filePath, bool success);
    void allTasksFinished();
    void workRequested(); // 内部信号，用于触发处理

private slots:
    void processTasks();

private:
    QQueue<SaveTask> taskQueue_;
    QMutex mutex_;
    QWaitCondition condition_;
    bool stopRequested_;
};

#endif // IMAGE_SAVE_WORKER_H
