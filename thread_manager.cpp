#include "thread_manager.h"
#include <QDebug>
#include "image_save_worker.h"
#include "image_analysis_worker.h"
#include "noise_analysis_worker.h"
#include "spot_detection_worker.h"

ThreadManager::ThreadManager(QObject *parent)
    : QObject(parent)
    , imageSaveWorker_(nullptr)
    , imageAnalysisWorker_(nullptr)
    , noiseAnalysisWorker_(nullptr)
#if ENABLE_SPOT_DETECTION
    , spotDetectionWorker_(nullptr)
#endif
    , cameraSDKThread_(nullptr)
    , imageSaveThread_(nullptr)
    , imageAnalysisThread_(nullptr)
    , noiseAnalysisThread_(nullptr)
#if ENABLE_SPOT_DETECTION
    , spotDetectionThread_(nullptr)
#endif
    , threadCounter_(0)
{
    setupImageSaveWorker();
    setupImageAnalysisWorker();
    setupNoiseAnalysisWorker();
#if ENABLE_SPOT_DETECTION
    setupSpotDetectionWorker();
#endif

    cameraSDKThread_ = new QThread(this);
    cameraSDKThread_->setObjectName("CameraSDKThread");
    threads_["CameraSDK"] = cameraSDKThread_;
}

ThreadManager::~ThreadManager()
{
    stopAllThreads();
    cleanupThreads();
}

void ThreadManager::setupImageSaveWorker()
{
    imageSaveThread_ = new QThread(this);
    imageSaveThread_->setObjectName("ImageSaveThread");

    imageSaveWorker_ = new ImageSaveWorker();
    imageSaveWorker_->moveToThread(imageSaveThread_);

    connect(imageSaveThread_, &QThread::finished, imageSaveWorker_, &QObject::deleteLater);

    threads_["ImageSave"] = imageSaveThread_;
}

void ThreadManager::setupImageAnalysisWorker()
{
    imageAnalysisThread_ = new QThread(this);
    imageAnalysisThread_->setObjectName("ImageAnalysisThread");

    imageAnalysisWorker_ = new ImageAnalysisWorker();
    imageAnalysisWorker_->moveToThread(imageAnalysisThread_);

    connect(imageAnalysisThread_, &QThread::finished, imageAnalysisWorker_, &QObject::deleteLater);

    threads_["ImageAnalysis"] = imageAnalysisThread_;
}

void ThreadManager::setupNoiseAnalysisWorker()
{
    noiseAnalysisThread_ = new QThread(this);
    noiseAnalysisThread_->setObjectName("NoiseAnalysisThread");

    noiseAnalysisWorker_ = new NoiseAnalysisWorker();
    noiseAnalysisWorker_->moveToThread(noiseAnalysisThread_);

    connect(noiseAnalysisThread_, &QThread::finished, noiseAnalysisWorker_, &QObject::deleteLater);

    threads_["NoiseAnalysis"] = noiseAnalysisThread_;
}

#if ENABLE_SPOT_DETECTION
void ThreadManager::setupSpotDetectionWorker()
{
    spotDetectionThread_ = new QThread(this);
    spotDetectionThread_->setObjectName("SpotDetectionThread");

    spotDetectionWorker_ = new SpotDetectionWorker();
    spotDetectionWorker_->moveToThread(spotDetectionThread_);

    connect(spotDetectionThread_, &QThread::finished, spotDetectionWorker_, &QObject::deleteLater);

    threads_["SpotDetection"] = spotDetectionThread_;
}
#endif

void ThreadManager::startAllThreads()
{
    if (cameraSDKThread_ && !cameraSDKThread_->isRunning()) {
        cameraSDKThread_->start();
    }

    if (imageSaveThread_ && !imageSaveThread_->isRunning()) {
        imageSaveThread_->start();
    }

    if (imageAnalysisThread_ && !imageAnalysisThread_->isRunning()) {
        imageAnalysisThread_->start();
    }

    if (noiseAnalysisThread_ && !noiseAnalysisThread_->isRunning()) {
        noiseAnalysisThread_->start();
    }

#if ENABLE_SPOT_DETECTION
    if (spotDetectionThread_ && !spotDetectionThread_->isRunning()) {
        spotDetectionThread_->start();
    }
#endif

    emit allThreadsStarted();
}

QThread* ThreadManager::createAlgorithmThread(const QString &algorithmId)
{
    QString threadName = QString("Algorithm_%1").arg(algorithmId);
    return createWorkerThread(threadName);
}

QThread* ThreadManager::createWorkerThread(const QString &threadName)
{
    if (threads_.contains(threadName)) {
        qWarning() << "Thread already exists:" << threadName;
        return threads_[threadName];
    }

    QThread *thread = new QThread(this);
    thread->setObjectName(threadName);

    threads_[threadName] = thread;

    if (cameraSDKThread_ && cameraSDKThread_->isRunning()) {
        thread->start();
    }

    return thread;
}

void ThreadManager::unregisterThread(const QString &threadName)
{
    QThread *thread = threads_.value(threadName, nullptr);
    if (thread) {
        unregisterThread(thread);
    }
}

void ThreadManager::unregisterThread(QThread *thread)
{
    if (!thread) return;

    QString threadName;
    for (auto it = threads_.begin(); it != threads_.end(); ++it) {
        if (it.value() == thread) {
            threadName = it.key();
            threads_.erase(it);
            break;
        }
    }

    if (thread->isRunning()) {
        thread->quit();
        if (!thread->wait(2000)) {
            qWarning() << "Thread" << threadName << "did not finish in time";
            thread->terminate();
            thread->wait();
        }
    }

    if (thread->parent() != this) {
        thread->deleteLater();
    }
}

void ThreadManager::stopAllThreads()
{
#if ENABLE_SPOT_DETECTION
    if (spotDetectionWorker_) {
        spotDetectionWorker_->stop();
    }
#endif

    for (auto it = threads_.begin(); it != threads_.end(); ++it) {
        QThread *thread = it.value();
        if (thread && thread->isRunning()) {
            thread->quit();
        }
    }

    for (auto it = threads_.begin(); it != threads_.end(); ++it) {
        QThread *thread = it.value();
        if (thread && thread->isRunning()) {
            if (!thread->wait(5000)) {
                qWarning() << "Thread" << it.key() << "did not finish in time, forcing termination";
                thread->terminate();
                thread->wait();
            }
        }
    }

    emit allThreadsStopped();
}

bool ThreadManager::isThreadRunning(const QString &threadName) const
{
    QThread *thread = threads_.value(threadName, nullptr);
    return thread && thread->isRunning();
}

void ThreadManager::cleanupThreads()
{
    imageSaveWorker_ = nullptr;
    imageAnalysisWorker_ = nullptr;
    noiseAnalysisWorker_ = nullptr;
}
