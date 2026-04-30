#ifndef IMAGE_ALGORITHM_BASE_H
#define IMAGE_ALGORITHM_BASE_H

#include <QObject>
#include <QImage>
#include <QVector>
#include <QVariantMap>
#include <QMutex>
#include <QString>
#include <atomic>
#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>
#ifdef CAMERUI_ENABLE_CUDA
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/cudafilters.hpp>
#endif

/**
 * @brief Algorithm parameter definition
 */
struct AlgorithmParameter {
    QString name;           // Parameter internal name
    QString displayName;    // Parameter display name (localized)
    QString type;           // Parameter type: "bool", "int", "double", "enum"
    QVariant defaultValue;  // Default value
    QVariant minValue;      // Minimum value (for int/double)
    QVariant maxValue;      // Maximum value (for int/double)
    QVariant step;          // Step value (for int/double spinboxes)
    QStringList enumValues; // Enum options (for enum type)
    QString tooltip;        // Parameter tooltip/description
    
    AlgorithmParameter() = default;
    
    AlgorithmParameter(const QString &n, const QString &dn, const QString &t,
                       const QVariant &def, const QVariant &min = QVariant(),
                       const QVariant &max = QVariant(), const QVariant &s = QVariant(),
                       const QStringList &enums = QStringList(), const QString &tip = QString())
        : name(n), displayName(dn), type(t), defaultValue(def),
          minValue(min), maxValue(max), step(s), enumValues(enums), tooltip(tip) {}
};

/**
 * @brief Algorithm category/group information
 */
struct AlgorithmInfo {
    QString id;                             // Unique algorithm identifier
    QString name;                           // Algorithm display name (localized)
    QString category;                       // Algorithm category
    QString description;                    // Algorithm description
    QVector<AlgorithmParameter> parameters; // Algorithm parameters
    
    AlgorithmInfo() = default;
};

/**
 * @brief Base class for all image processing algorithms
 * 
 * Provides common functionality for GPU-accelerated image processing
 * using OpenCV's UMat (Unified Memory) for OpenCL support.
 */
class ImageAlgorithmBase : public QObject
{
    Q_OBJECT
    
public:
    explicit ImageAlgorithmBase(QObject *parent = nullptr);
    virtual ~ImageAlgorithmBase();

    enum class InputScaleMode {
        ScaleTo255 = 0, // 默认：raw -> float32 映射到 0..255
        Native = 1      // 高精度：raw -> float32 保留 0..maxVal
    };

    /**
     * @brief Whether this algorithm supports CV_32F input in processImpl()
     *
     * When processing raw images, the base class can convert raw->CV_32F (scaled to 0..255)
     * to preserve more precision for algorithms that benefit from it. Algorithms that
     * require CV_8U input (e.g. Canny, adaptiveThreshold, LUT-based ops) should override
     * this to return false so the base class will directly use the 8-bit fallback path.
     */
    virtual bool supportsFloat32Input() const { return true; }
    
    /**
     * @brief Check if the algorithm supports CUDA acceleration
     * @return true if CUDA implementation is available
     */
    virtual bool supportsCuda() const { return false; }

#ifdef CAMERUI_ENABLE_CUDA
    /**
     * @brief Process the image using CUDA
     * @param input Input image (GpuMat)
     * @param stream CUDA stream for asynchronous processing
     * @return Processed image (GpuMat)
     */
    virtual cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) {
        Q_UNUSED(input);
        Q_UNUSED(stream);
        return cv::cuda::GpuMat();
    }
#endif

    /**
     * @brief Get algorithm information
     */
    virtual AlgorithmInfo algorithmInfo() const = 0;
    
    /**
     * @brief Get algorithm ID
     */
    QString algorithmId() const { return algorithmInfo().id; }
    
    /**
     * @brief Get algorithm name
     */
    QString algorithmName() const { return algorithmInfo().name; }
    
public slots:
    /**
     * @brief Process raw 16-bit image data
     * @param rawData Raw 16-bit image data
     * @param width Image width
     * @param height Image height
     * @param bitDepth Image bit depth
     */
    void processRawImage(const QVector<uint16_t> &rawData, int width, int height, int bitDepth);
    
    /**
     * @brief Process 8-bit image
     * @param image Input image
     */
    void processImage(const QImage &image);
    
    /**
     * @brief Update algorithm parameters
     * @param params Parameter map (name -> value)
     */
    void updateParameters(const QVariantMap &params);
    
    /**
     * @brief Enable or disable processing
     * @param enabled Whether processing is enabled
     */
    void setEnabled(bool enabled);

    // mode: 0=ScaleTo255, 1=Native
    void setInputScaleMode(int mode);
    
    /**
     * @brief Check if processing is enabled
     */
    bool isEnabled() const { return m_enabled.load(); }
    
    /**
     * @brief Stop processing
     */
    void stop();

    /**
     * @brief Execute the algorithm synchronously (CPU/OpenCL)
     * @param input Input image
     * @return Processed image
     */
    cv::UMat execute(const cv::UMat &input);

#ifdef CAMERUI_ENABLE_CUDA
    /**
     * @brief Execute the algorithm synchronously (CUDA)
     * @param input Input image
     * @param stream CUDA stream
     * @return Processed image
     */
    cv::cuda::GpuMat executeCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream);
#endif // CAMERUI_ENABLE_CUDA

signals:
    /**
     * @brief Emitted when image processing is complete
     * @param processedImage Processed image
     * @param originalImage Original image (for comparison)
     */
    void imageProcessed(const QImage &processedImage, const QImage &originalImage);
    
    /**
     * @brief Emitted with processing statistics
     * @param processingTime Processing time in milliseconds
     * @param fps Frames per second
     */
    void statisticsUpdated(double processingTime, double fps);
    
    /**
     * @brief Emitted when an error occurs
     * @param error Error message
     */
    void errorOccurred(const QString &error);
    
protected:
    /**
     * @brief Process image implementation (must be overridden by subclasses)
     * @param input Input image (8-bit grayscale UMat)
     * @return Processed image
     */
    virtual cv::UMat processImpl(const cv::UMat &input) = 0;
    
    /**
     * @brief Get current parameter value
     * @param name Parameter name
     * @return Parameter value
     */
    QVariant getParameter(const QString &name) const;
    
    /**
     * @brief Convert 16-bit raw data to 8-bit
     */
    cv::UMat convertRawTo8Bit(const QVector<uint16_t> &rawData, int width, int height, int bitDepth);
    
    bool m_cudaAvailable{false};
    bool m_openclReliable{false};  // Set to true only if OpenCL is tested and working
#ifdef CAMERUI_ENABLE_CUDA
    cv::Ptr<cv::cuda::Stream> m_cudaStream;
#endif
    
    QVariantMap m_parameters;
    mutable QRecursiveMutex m_paramsMutex;
    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_shouldStop{false};
    std::atomic<bool> m_isProcessing{false};
    
    // Performance statistics
    int m_frameCount{0};
    qint64 m_lastStatisticsTime{0};

    InputScaleMode m_inputScaleMode{InputScaleMode::ScaleTo255};
};

/**
 * @brief Factory for creating algorithm instances
 */
class ImageAlgorithmFactory
{
public:
    static ImageAlgorithmFactory& instance();
    
    /**
     * @brief Register an algorithm class
     */
    template<typename T>
    void registerAlgorithm() {
        AlgorithmInfo info = T().algorithmInfo();
        m_creators[info.id] = []() -> ImageAlgorithmBase* { return new T(); };
        m_algorithmInfos[info.id] = info;
    }
    
    /**
     * @brief Create algorithm instance by ID
     * @param id Algorithm ID
     * @return New algorithm instance (caller takes ownership)
     */
    ImageAlgorithmBase* createAlgorithm(const QString &id);
    
    /**
     * @brief Get all registered algorithm infos
     */
    QVector<AlgorithmInfo> allAlgorithmInfos() const;
    
    /**
     * @brief Get algorithm infos by category
     */
    QVector<AlgorithmInfo> algorithmInfosByCategory(const QString &category) const;
    
    /**
     * @brief Get all categories
     */
    QStringList categories() const;
    
private:
    ImageAlgorithmFactory() = default;
    
    QMap<QString, std::function<ImageAlgorithmBase*()>> m_creators;
    QMap<QString, AlgorithmInfo> m_algorithmInfos;
};

/**
 * @brief Utility functions for algorithm precision mode handling
 * 
 * These are shared between ImageProcessingPanel and MixedProcessingDialog
 * to ensure consistent behavior across the application.
 */
namespace AlgorithmPrecisionUtils {

/**
 * @brief Check if an algorithm should be hidden in high precision (16-bit) mode
 * 
 * Algorithms that are 8-bit-only (supportsFloat32Input() == false) should
 * be hidden when the system is operating in 16-bit high precision mode.
 * 
 * @param algorithmId Algorithm ID to check
 * @return true if algorithm is 8-bit only and should be hidden in 16-bit mode
 */
bool shouldHideInHighPrecision(const QString &algorithmId);

/**
 * @brief Map UI-range parameters to native range for 16-bit processing
 * 
 * When processing images in 16-bit mode, parameters that are specified in
 * UI range (typically 0-255 for 8-bit style parameters) need to be scaled
 * to the native range based on the effective bit depth.
 * 
 * @param uiParams Parameters in UI range (0-255 style)
 * @param bitDepth Effective bit depth of the input image
 * @return Parameters scaled to native range
 */
QVariantMap mapUiParamsToNativeRange(const QVariantMap &uiParams, int bitDepth);

} // namespace AlgorithmPrecisionUtils

#endif // IMAGE_ALGORITHM_BASE_H
