#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include <QObject>
#include <QThread>
#include <QHash>

class ImageSaveWorker;
class ImageAnalysisWorker;
class NoiseAnalysisWorker;
class HistogramEqualizationWorker;
class SpotDetectionWorker;

/**
 * @brief 线程管理器 - 集中管理应用程序线程架构
 *
 * @details 线程架构设计：
 * 1. UI线程（主线程）：处理用户界面事件
 * 2. 相机SDK线程：预留用于相机SDK图像采集集成
 * 3. 图像保存线程：异步保存图像到磁盘
 * 4. 图像分析线程：计算图像统计信息和直方图
 * 5. 噪声分析线程：执行噪声分析计算
 * 6. 算法处理线程池：动态管理图像处理算法线程
 *
 * @note 关键设计原则：
 * - 所有工作线程由ThreadManager统一创建和管理
 * - 工作对象使用moveToThread模式移动到对应线程
 * - 线程生命周期由ThreadManager统一控制
 * - 统一使用QThread-based线程，不使用std::thread或pthread
 * - UI组件可以通过ThreadManager获取工作对象引用进行信号连接
 *
 * @note 线程安全：
 * - ThreadManager本身运行在UI线程
 * - 所有public方法都是线程安全的
 * - 工作对象的方法通过Qt信号/槽机制在各自线程中执行
 */
class ThreadManager : public QObject
{
    Q_OBJECT

public:
    explicit ThreadManager(QObject *parent = nullptr);
    ~ThreadManager();

    QThread* cameraSDKThread() const { return cameraSDKThread_; }
    QThread* imageSaveThread() const { return imageSaveThread_; }
    QThread* imageAnalysisThread() const { return imageAnalysisThread_; }
    QThread* noiseAnalysisThread() const { return noiseAnalysisThread_; }
#if ENABLE_SPOT_DETECTION
    QThread* spotDetectionThread() const { return spotDetectionThread_; }
#endif

    ImageSaveWorker* imageSaveWorker() const { return imageSaveWorker_; }
    ImageAnalysisWorker* imageAnalysisWorker() const { return imageAnalysisWorker_; }
    NoiseAnalysisWorker* noiseAnalysisWorker() const { return noiseAnalysisWorker_; }
#if ENABLE_SPOT_DETECTION
    SpotDetectionWorker* spotDetectionWorker() const { return spotDetectionWorker_; }
#endif

    QThread* createAlgorithmThread(const QString &algorithmId);
    QThread* createWorkerThread(const QString &threadName);
    void unregisterThread(const QString &threadName);
    void unregisterThread(QThread *thread);

    void startAllThreads();
    void stopAllThreads();

    bool isThreadRunning(const QString &threadName) const;

signals:
    void allThreadsStarted();
    void allThreadsStopped();
    void threadError(const QString &threadName, const QString &error);

private:
    void setupImageSaveWorker();
    void setupImageAnalysisWorker();
    void setupNoiseAnalysisWorker();
#if ENABLE_SPOT_DETECTION
    void setupSpotDetectionWorker();
#endif
    void cleanupThreads();

    ImageSaveWorker *imageSaveWorker_;
    ImageAnalysisWorker *imageAnalysisWorker_;
    NoiseAnalysisWorker *noiseAnalysisWorker_;
#if ENABLE_SPOT_DETECTION
    SpotDetectionWorker *spotDetectionWorker_;
#endif

    QThread *cameraSDKThread_;
    QThread *imageSaveThread_;
    QThread *imageAnalysisThread_;
    QThread *noiseAnalysisThread_;
#if ENABLE_SPOT_DETECTION
    QThread *spotDetectionThread_;
#endif

    QHash<QString, QThread*> threads_;
    int threadCounter_;
};

#endif // THREAD_MANAGER_H
