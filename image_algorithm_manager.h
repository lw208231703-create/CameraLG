#ifndef IMAGE_ALGORITHM_MANAGER_H
#define IMAGE_ALGORITHM_MANAGER_H

#include <QObject>
#include <QThread>
#include <QMap>
#include <QPointer>
#include <QElapsedTimer>
#include "image_algorithm_base.h"
#include "image_algorithm_display.h"
#include "thread_manager.h"

/**
 * @brief Manages image processing algorithms and their worker threads
 * 
 * Provides centralized management for algorithm instances, including:
 * - Algorithm creation and destruction
 * - Worker thread management (via ThreadManager)
 * - Display window management
 * - Image data distribution to active algorithms
 */
class ImageAlgorithmManager : public QObject
{
    Q_OBJECT
    
public:
    explicit ImageAlgorithmManager(ThreadManager *threadManager, QObject *parent = nullptr);
    ~ImageAlgorithmManager();
    
    /**
     * @brief Initialize and register all available algorithms
     */
    void registerAllAlgorithms();
    
    /**
     * @brief Get list of all algorithm categories
     */
    QStringList getCategories() const;
    
    /**
     * @brief Get algorithms in a category
     * @param category Category name
     * @return List of algorithm infos in the category
     */
    QVector<AlgorithmInfo> getAlgorithmsInCategory(const QString &category) const;
    
    /**
     * @brief Enable an algorithm
     * @param algorithmId Algorithm ID
     * @param params Initial parameters
     * @return true if successfully enabled
     */
    bool enableAlgorithm(const QString &algorithmId, const QVariantMap &params = QVariantMap());
    
    /**
     * @brief Disable an algorithm
     * @param algorithmId Algorithm ID
     */
    void disableAlgorithm(const QString &algorithmId);
    
    /**
     * @brief Check if an algorithm is enabled
     * @param algorithmId Algorithm ID
     */
    bool isAlgorithmEnabled(const QString &algorithmId) const;
    
    /**
     * @brief Update algorithm parameters
     * @param algorithmId Algorithm ID
     * @param params New parameters
     */
    void updateAlgorithmParameters(const QString &algorithmId, const QVariantMap &params);
    
    /**
     * @brief Get algorithm info by ID
     * @param algorithmId Algorithm ID
     */
    AlgorithmInfo getAlgorithmInfo(const QString &algorithmId) const;

    /**
     * @brief Get cached UI-range parameters if available, otherwise return defaults.
     *
     * Note: values are in UI range (typically 0..255 for 8-bit style params).
     */
    QVariantMap getCachedOrDefaultUiParameters(const QString &algorithmId) const;

    enum class InputScaleMode {
        ScaleTo255 = 0,
        Native = 1
    };

    void setInputScaleMode(InputScaleMode mode);

    // Provide a bit-depth hint as soon as the UI/dock knows it, so parameters can be
    // mapped correctly even before the first frame is processed.
    void setCurrentBitDepth(int bitDepth);

signals:
    /**
     * @brief Diagnostic message for logging (forward to UI OutputDock)
     */
    void diagnosticMessage(const QString &message);    
public slots:
    /**
     * @brief Process raw image data
     * @param rawData Raw 16-bit image data
     * @param width Image width
     * @param height Image height
     * @param bitDepth Image bit depth
     */
    void processRawImage(const QVector<uint16_t> &rawData, int width, int height, int bitDepth);

    /**
     * @brief Process already-converted 8-bit grayscale image (OpenCV standard 0..255 pipeline)
     */
    void processImage(const QImage &image);
    
signals:
    /**
     * @brief Emitted when an algorithm is enabled
     * @param algorithmId Algorithm ID
     */
    void algorithmEnabled(const QString &algorithmId);
    
    /**
     * @brief Emitted when an algorithm is disabled
     * @param algorithmId Algorithm ID
     */
    void algorithmDisabled(const QString &algorithmId);
    
    /**
     * @brief Emitted when an algorithm encounters an error
     * @param algorithmId Algorithm ID
     * @param error Error message
     */
    void algorithmError(const QString &algorithmId, const QString &error);
    
private slots:
    void onDisplayWindowClosed();
    void onAlgorithmError(const QString &error);
    
private:
    static QVariantMap defaultUiParamsForInfo(const AlgorithmInfo &info);

    struct AlgorithmInstance {
        QThread *thread{nullptr};
        ImageAlgorithmBase *worker{nullptr};
        QPointer<ImageAlgorithmDisplay> display;
        QString algorithmId;

        // Throttling
        int fpsLimit{30};          // 0 = unlimited
        qint64 lastDeliveredNs{0}; // timestamp from m_throttleClock
    };
    
    ThreadManager *m_threadManager{nullptr};  // Reference to thread manager
    QMap<QString, AlgorithmInstance> m_activeAlgorithms;
    bool m_algorithmsRegistered{false};

    QElapsedTimer m_throttleClock;

    InputScaleMode m_inputScaleMode{InputScaleMode::ScaleTo255};
    int m_lastBitDepth{0};
    int m_currentBitDepthHint{0};

    // Cache last UI-range parameters per algorithm so UI stays consistent across
    // mode switches / re-enables.
    QMap<QString, QVariantMap> m_cachedUiParams;
};

#endif // IMAGE_ALGORITHM_MANAGER_H
