#include "image_algorithm_manager.h"
#include "thread_manager.h"
#include "image_algorithms_filtering.h"
#include "image_algorithms_edge.h"
#include "image_algorithms_morphology.h"
#include "image_algorithms_threshold.h"
#include "image_algorithms_enhance.h"
#include "image_algorithms_frequency.h"
#include "image_algorithms_geometry.h"
#include "image_algorithms_segment.h"

#include "build_config.h"

#include <cmath>
#include <functional>


ImageAlgorithmManager::ImageAlgorithmManager(ThreadManager *threadManager, QObject *parent)
    : QObject(parent)
    , m_threadManager(threadManager)
{
    m_throttleClock.start();
}

void ImageAlgorithmManager::setCurrentBitDepth(int bitDepth)
{
    if (bitDepth <= 0) {
        return;
    }
    m_currentBitDepthHint = bitDepth;
}

void ImageAlgorithmManager::setInputScaleMode(InputScaleMode mode)
{
    m_inputScaleMode = mode;

    const auto algorithmIds = m_activeAlgorithms.keys();
    for (const auto &algorithmId : algorithmIds) {
        auto it = m_activeAlgorithms.find(algorithmId);
        if (it == m_activeAlgorithms.end()) continue;
        if (!it->worker) continue;

        QMetaObject::invokeMethod(it->worker, "setInputScaleMode", Qt::QueuedConnection,
                                 Q_ARG(int, mode == InputScaleMode::Native ? 1 : 0));
    }
}

ImageAlgorithmManager::~ImageAlgorithmManager()
{
    // Clean up all active algorithms
    for (auto it = m_activeAlgorithms.begin(); it != m_activeAlgorithms.end(); ++it) {
        AlgorithmInstance &instance = it.value();
        
        if (instance.worker) {
            instance.worker->stop();
        }
        
        if (instance.thread) {
            instance.thread->quit();
            instance.thread->wait(1000);
        }
        
        if (instance.display) {
            instance.display->close();
        }
    }
    
    m_activeAlgorithms.clear();
}

QVariantMap ImageAlgorithmManager::defaultUiParamsForInfo(const AlgorithmInfo &info)
{
    QVariantMap defaults;
    for (const auto &param : info.parameters) {
        defaults[param.name] = param.defaultValue;
    }
    return defaults;
}

QVariantMap ImageAlgorithmManager::getCachedOrDefaultUiParameters(const QString &algorithmId) const
{
    auto it = m_cachedUiParams.find(algorithmId);
    if (it != m_cachedUiParams.end()) {
        return it.value();
    }
    const AlgorithmInfo info = getAlgorithmInfo(algorithmId);
    if (info.id.isEmpty()) {
        return QVariantMap();
    }
    return defaultUiParamsForInfo(info);
}

void ImageAlgorithmManager::registerAllAlgorithms()
{
    if (m_algorithmsRegistered) {
        return;
    }

    // Register all algorithm categories
    registerFilteringAlgorithms();
    registerEdgeAlgorithms();
    registerMorphologyAlgorithms();
    registerThresholdAlgorithms();
    registerEnhanceAlgorithms();
    registerFrequencyAlgorithms();
    registerGeometryAlgorithms();
    registerSegmentAlgorithms();

    m_algorithmsRegistered = true;
}

QStringList ImageAlgorithmManager::getCategories() const
{
    return ImageAlgorithmFactory::instance().categories();
}

QVector<AlgorithmInfo> ImageAlgorithmManager::getAlgorithmsInCategory(const QString &category) const
{
    return ImageAlgorithmFactory::instance().algorithmInfosByCategory(category);
}

bool ImageAlgorithmManager::enableAlgorithm(const QString &algorithmId, const QVariantMap &params)
{
    // Check if already enabled
    if (m_activeAlgorithms.contains(algorithmId)) {
        return true;
    }
    
    // Create algorithm instance
    ImageAlgorithmBase *worker = ImageAlgorithmFactory::instance().createAlgorithm(algorithmId);
    if (!worker) {
        qWarning() << "Failed to create algorithm:" << algorithmId;
        emit diagnosticMessage(QStringLiteral("Failed to create algorithm: %1").arg(algorithmId));
        return false;
    }
    
    AlgorithmInfo info = worker->algorithmInfo();
    
    // Create worker thread using ThreadManager
    QThread *thread = nullptr;
    if (m_threadManager) {
        thread = m_threadManager->createAlgorithmThread(algorithmId);
    } else {
        qWarning() << "ThreadManager is null, creating thread locally";
        thread = new QThread(this);
    }
    worker->moveToThread(thread);
    
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    if (!m_threadManager) {
        // Only delete locally created threads
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
    
    // Create display window
    ImageAlgorithmDisplay *display = new ImageAlgorithmDisplay(info.name);
    
    // Connect signals
    connect(worker, &ImageAlgorithmBase::imageProcessed,
            display, &ImageAlgorithmDisplay::updateImages, Qt::QueuedConnection);
    connect(worker, &ImageAlgorithmBase::statisticsUpdated,
            display, &ImageAlgorithmDisplay::updateStatistics, Qt::QueuedConnection);
    connect(worker, &ImageAlgorithmBase::errorOccurred,
            this, &ImageAlgorithmManager::onAlgorithmError, Qt::QueuedConnection);
    connect(display, &ImageAlgorithmDisplay::windowClosed,
            this, &ImageAlgorithmManager::onDisplayWindowClosed);

    // Frame rate throttling control (per-algorithm window)
    connect(display, &ImageAlgorithmDisplay::frameRateLimitChanged, this,
            [this, algorithmId](int fpsLimit) {
                auto it = m_activeAlgorithms.find(algorithmId);
                if (it == m_activeAlgorithms.end()) return;
                it->fpsLimit = fpsLimit;
                it->lastDeliveredNs = 0; // reset to apply immediately
            });
    
    // Store algorithm ID in display for later lookup
    display->setProperty("algorithmId", algorithmId);
    
    // Start thread
    thread->start();

    // Apply input scale mode to worker
    QMetaObject::invokeMethod(worker, "setInputScaleMode", Qt::QueuedConnection,
                         Q_ARG(int, m_inputScaleMode == InputScaleMode::Native ? 1 : 0));
    
    // Set initial parameters (with default values for parameters not specified).
    // If caller didn't provide a value, reuse cached UI params if present.
    const QVariantMap cached = getCachedOrDefaultUiParameters(algorithmId);
    QVariantMap allParams;
    for (const auto &param : info.parameters) {
        if (params.contains(param.name)) {
            allParams[param.name] = params[param.name];
        } else if (cached.contains(param.name)) {
            allParams[param.name] = cached.value(param.name);
        } else {
            allParams[param.name] = param.defaultValue;
        }
    }

    // Cache UI-range params so the panel can reflect actual values later.
    m_cachedUiParams[algorithmId] = allParams;
    
    QVariantMap sendParams = allParams;
    const int bitDepthForMapping = (m_lastBitDepth > 0 ? m_lastBitDepth : m_currentBitDepthHint);
    if (m_inputScaleMode == InputScaleMode::Native && bitDepthForMapping > 8) {
        sendParams = AlgorithmPrecisionUtils::mapUiParamsToNativeRange(allParams, bitDepthForMapping);
    }
    QMetaObject::invokeMethod(worker, "updateParameters", Qt::QueuedConnection,
                             Q_ARG(QVariantMap, sendParams));
    
    // Enable the algorithm
    QMetaObject::invokeMethod(worker, "setEnabled", Qt::QueuedConnection,
                             Q_ARG(bool, true));
    
    // Show display window
    display->show();
    display->raise();
    
    // Store instance
    AlgorithmInstance instance;
    instance.thread = thread;
    instance.worker = worker;
    instance.display = display;
    instance.algorithmId = algorithmId;
    instance.fpsLimit = 30;
    instance.lastDeliveredNs = 0;
    m_activeAlgorithms[algorithmId] = instance;
    
    emit algorithmEnabled(algorithmId);
    
    return true;
}

void ImageAlgorithmManager::disableAlgorithm(const QString &algorithmId)
{
    auto it = m_activeAlgorithms.find(algorithmId);
    if (it == m_activeAlgorithms.end()) {
        return;
    }

    // Take a copy and remove from active map first to stop delivering new frames immediately.
    AlgorithmInstance instance = it.value();
    m_activeAlgorithms.erase(it);
    
    // Disable and stop worker
    if (instance.worker) {
        // Set stop flags immediately (thread-safe atomics) so any queued frames will early-return.
        // Do NOT call setEnabled(false) directly here (it mutates non-atomic counters across threads).
        instance.worker->stop();
    }
    
    // Stop thread
    if (instance.thread) {
        instance.thread->requestInterruption();
        instance.thread->quit();
        
        // Unregister thread from ThreadManager
        if (m_threadManager) {
            m_threadManager->unregisterThread(instance.thread);
        } else {
            // Wait for thread to finish if not managed by ThreadManager
            instance.thread->wait(2000);
        }
    }
    
    // Close display window
    if (instance.display) {
        if (instance.worker) {
            QObject::disconnect(instance.worker, nullptr, instance.display, nullptr);
        }
        disconnect(instance.display, &ImageAlgorithmDisplay::windowClosed,
                   this, &ImageAlgorithmManager::onDisplayWindowClosed);
        // Avoid calling close() from within closeEvent re-entrantly.
        instance.display->hide();
        instance.display->deleteLater();
    }

    emit algorithmDisabled(algorithmId);
    
}

bool ImageAlgorithmManager::isAlgorithmEnabled(const QString &algorithmId) const
{
    return m_activeAlgorithms.contains(algorithmId);
}

void ImageAlgorithmManager::updateAlgorithmParameters(const QString &algorithmId, const QVariantMap &params)
{
    auto it = m_activeAlgorithms.find(algorithmId);
    if (it == m_activeAlgorithms.end()) {
        return;
    }

    // Update UI-range cache
    m_cachedUiParams[algorithmId] = params;
    
    if (it->worker) {
        QVariantMap sendParams = params;
        const int bitDepthForMapping = (m_lastBitDepth > 0 ? m_lastBitDepth : m_currentBitDepthHint);
        if (m_inputScaleMode == InputScaleMode::Native && bitDepthForMapping > 8) {
            sendParams = AlgorithmPrecisionUtils::mapUiParamsToNativeRange(params, bitDepthForMapping);
        }
        QMetaObject::invokeMethod(it->worker, "updateParameters", Qt::QueuedConnection,
                                 Q_ARG(QVariantMap, sendParams));
    }
}

AlgorithmInfo ImageAlgorithmManager::getAlgorithmInfo(const QString &algorithmId) const
{
    QVector<AlgorithmInfo> allInfos = ImageAlgorithmFactory::instance().allAlgorithmInfos();
    for (const auto &info : allInfos) {
        if (info.id == algorithmId) {
            return info;
        }
    }
    return AlgorithmInfo();
}

void ImageAlgorithmManager::processRawImage(const QVector<uint16_t> &rawData, int width, int height, int bitDepth)
{
    if (rawData.isEmpty() || width <= 0 || height <= 0 || rawData.size() < width * height) {
        return;
    }

    m_lastBitDepth = bitDepth;
    if (bitDepth > 0) {
        m_currentBitDepthHint = bitDepth;
    }

    // Distribute image to all active algorithms (with throttling to avoid queue buildup)
    const auto algorithmIds = m_activeAlgorithms.keys();
    const qint64 nowNs = m_throttleClock.nsecsElapsed();

    for (const auto &algorithmId : algorithmIds) {
        auto it = m_activeAlgorithms.find(algorithmId);
        if (it == m_activeAlgorithms.end()) continue;

        AlgorithmInstance &instance = it.value();
        if (!instance.worker) continue;

        if (instance.fpsLimit > 0) {
            const qint64 intervalNs = 1000000000LL / qMax(1, instance.fpsLimit);
            if (instance.lastDeliveredNs != 0 && (nowNs - instance.lastDeliveredNs) < intervalNs) {
                continue; // drop frame
            }
            instance.lastDeliveredNs = nowNs;
        }

        QMetaObject::invokeMethod(instance.worker, "processRawImage", Qt::QueuedConnection,
                                 Q_ARG(QVector<uint16_t>, rawData),
                                 Q_ARG(int, width),
                                 Q_ARG(int, height),
                                 Q_ARG(int, bitDepth));
    }
}

void ImageAlgorithmManager::processImage(const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    const auto algorithmIds = m_activeAlgorithms.keys();
    const qint64 nowNs = m_throttleClock.nsecsElapsed();

    for (const auto &algorithmId : algorithmIds) {
        auto it = m_activeAlgorithms.find(algorithmId);
        if (it == m_activeAlgorithms.end()) continue;

        AlgorithmInstance &instance = it.value();
        if (!instance.worker) continue;

        if (instance.fpsLimit > 0) {
            const qint64 intervalNs = 1000000000LL / qMax(1, instance.fpsLimit);
            if (instance.lastDeliveredNs != 0 && (nowNs - instance.lastDeliveredNs) < intervalNs) {
                continue;
            }
            instance.lastDeliveredNs = nowNs;
        }

        QMetaObject::invokeMethod(instance.worker, "processImage", Qt::QueuedConnection,
                                 Q_ARG(QImage, image));
    }
}

void ImageAlgorithmManager::onDisplayWindowClosed()
{
    QObject *sender = QObject::sender();
    if (!sender) return;
    
    QString algorithmId = sender->property("algorithmId").toString();
    if (!algorithmId.isEmpty()) {
        disableAlgorithm(algorithmId);
    }
}

void ImageAlgorithmManager::onAlgorithmError(const QString &error)
{
    // Find which algorithm sent this error
    ImageAlgorithmBase *worker = qobject_cast<ImageAlgorithmBase*>(QObject::sender());
    if (worker) {
        QString algorithmId = worker->algorithmId();
        emit algorithmError(algorithmId, error);
        
        // Update error in display
        auto it = m_activeAlgorithms.find(algorithmId);
        if (it != m_activeAlgorithms.end() && it->display) {
            it->display->showError(error);
        }
    }
}
