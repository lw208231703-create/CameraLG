#include "image_algorithm_base.h"
#include <QElapsedTimer>
#include <QDateTime>
#include <QSet>
#include <cmath>
#include <algorithm>

ImageAlgorithmBase::ImageAlgorithmBase(QObject *parent)
    : QObject(parent)
{
    m_lastStatisticsTime = QDateTime::currentMSecsSinceEpoch();
    
    // Enable OpenCV optimizations and multi-threading
    cv::setUseOptimized(true);
    cv::setNumThreads(0); // 0 = use all available threads
    
    // Check for CUDA availability first
#ifdef CAMERUI_ENABLE_CUDA
    try {
        int deviceCount = cv::cuda::getCudaEnabledDeviceCount();
        m_cudaAvailable = (deviceCount > 0);
    } catch (...) {
        m_cudaAvailable = false;
    }

    if (m_cudaAvailable) {
        try {
            m_cudaStream = cv::makePtr<cv::cuda::Stream>();
        } catch (...) {
            m_cudaAvailable = false;
            m_cudaStream.release();
        }
    }
#else
    m_cudaAvailable = false;
#endif
    
    // OpenCL setup: ALWAYS test OpenCL regardless of CUDA availability
    // Some algorithms may not support CUDA but can use OpenCL
    m_openclReliable = false;
    try {
        if (cv::ocl::haveOpenCL()) {
            cv::ocl::setUseOpenCL(true);
            
            // Perform a simple OpenCL test to verify it works
            // Some Intel iGPU drivers crash on certain operations
            cv::Mat testMat = cv::Mat::ones(64, 64, CV_8UC1) * 128;
            cv::UMat testUMat;
            testMat.copyTo(testUMat);
            cv::UMat resultUMat;
            cv::GaussianBlur(testUMat, resultUMat, cv::Size(3, 3), 1.0);
            cv::Mat resultMat;
            resultUMat.copyTo(resultMat);
            
            // If we got here without crashing, OpenCL seems reliable
            if (!resultMat.empty() && resultMat.rows == 64 && resultMat.cols == 64) {
                m_openclReliable = true;
            }
        }
    } catch (const std::exception &e) {
        // OpenCL test failed, disable it
        m_openclReliable = false;
        cv::ocl::setUseOpenCL(false);
    } catch (...) {
        // OpenCL test failed, disable it
        m_openclReliable = false;
        cv::ocl::setUseOpenCL(false);
    }
    
    // If OpenCL is not reliable, disable it globally for this instance
    if (!m_openclReliable) {
        cv::ocl::setUseOpenCL(false);
    }
}

ImageAlgorithmBase::~ImageAlgorithmBase()
{
    stop();
}

void ImageAlgorithmBase::processRawImage(const QVector<uint16_t> &rawData, int width, int height, int bitDepth)
{
    if (m_shouldStop.load() || !m_enabled.load()) {
        return;
    }
    
    if (m_isProcessing.load()) {
        // Skip this frame to avoid queue buildup
        return;
    }
    
    m_isProcessing.store(true);
    
    QElapsedTimer timer;
    timer.start();
    
    try {
        // Declare UMats in outer scope to ensure they outlive the derived Mats
        // This prevents "UMat deallocation error: some derived Mat is still alive"
        cv::UMat inputUMat;
        cv::UMat input32fUMat;
        cv::UMat processedUMat;

        cv::Mat processedMat;
        cv::Mat originalMat;

        // Convert raw 16-bit data to 8-bit (for the left/original view, unchanged)
        inputUMat = convertRawTo8Bit(rawData, width, height, bitDepth);
        
        if (inputUMat.empty()) {
            // If conversion failed (e.g. empty data or OpenCL error), stop processing
            m_isProcessing.store(false);
            return;
        }

        bool useCuda = false;
        bool cudaFailed = false;
#ifdef CAMERUI_ENABLE_CUDA
        useCuda = m_cudaAvailable && supportsCuda() && !supportsFloat32Input();

        if (useCuda) {
            // CUDA path with automatic fallback on error
            try {
                cv::Mat rawMat(height, width, CV_16UC1, const_cast<uint16_t*>(rawData.constData()));

                cv::cuda::GpuMat gpuRaw;
                gpuRaw.upload(rawMat, *m_cudaStream);

                // Convert to 8-bit on GPU
                cv::cuda::GpuMat gpu8bit;
                double maxVal = (1 << bitDepth) - 1.0;
                gpuRaw.convertTo(gpu8bit, CV_8UC1, 255.0 / maxVal, 0, *m_cudaStream);

                cv::cuda::GpuMat gpuProcessed;
                {
                    QMutexLocker locker(&m_paramsMutex);
                    gpuProcessed = processImplCuda(gpu8bit, *m_cudaStream);
                }

                if (gpuProcessed.empty()) {
                    gpu8bit.download(processedMat, *m_cudaStream);
                } else {
                    gpuProcessed.download(processedMat, *m_cudaStream);
                }
                gpu8bit.download(originalMat, *m_cudaStream);
                m_cudaStream->waitForCompletion();
            } catch (const cv::Exception &e) {
                qWarning() << "CUDA processing failed for" << algorithmName() << ":" << e.what() << "- falling back to CPU/OpenCL";
                cudaFailed = true;
                processedMat = cv::Mat();
                originalMat = cv::Mat();
            }
        }
#endif // CAMERUI_ENABLE_CUDA

        // Fallback to CPU/OpenCL if CUDA failed or not used
        if (!useCuda || cudaFailed) {
            // CPU/OpenCL path
            if (supportsFloat32Input()) {
                // Convert raw to CV_32F
                cv::Mat rawMat(height, width, CV_16UC1, const_cast<uint16_t*>(rawData.constData()));
                // Use copyTo instead of getUMat for better OpenCL driver compatibility
                cv::UMat rawUMat;
                rawMat.copyTo(rawUMat);
                double maxVal = (1 << bitDepth) - 1.0;
                if (m_inputScaleMode == InputScaleMode::Native) {
                    rawUMat.convertTo(input32fUMat, CV_32F, 1.0);
                } else {
                    rawUMat.convertTo(input32fUMat, CV_32F, 255.0 / maxVal);
                }

                // Safety fallback for any algorithm that still rejects CV_32F
                bool float32Failed = false;
                try {
                    QMutexLocker locker(&m_paramsMutex);
                    processedUMat = processImpl(input32fUMat);
                } catch (const cv::Exception &e) {
                    qWarning() << "Float32 processing failed for" << algorithmName() << ":" << e.what() << "- falling back to 8-bit";
                    float32Failed = true;
                }
                
                // Fallback to 8-bit if float32 failed
                if (float32Failed) {
                    QMutexLocker locker(&m_paramsMutex);
                    processedUMat = processImpl(inputUMat);
                }
            } else {
                QMutexLocker locker(&m_paramsMutex);
                processedUMat = processImpl(inputUMat);
            }

            // Ensure output is 8-bit for display.
            // IMPORTANT: do NOT use per-frame NORM_MINMAX here for raw16 pipelines,
            // otherwise brightness/contrast shifts (additive offsets) get canceled visually.
            if (!processedUMat.empty() && processedUMat.type() != CV_8UC1) {
                const double maxVal = (1 << bitDepth) - 1.0;
                cv::UMat out8;

                if (processedUMat.type() == CV_32F) {
                    const double alpha = (m_inputScaleMode == InputScaleMode::Native)
                                             ? (255.0 / maxVal)   // native 0..maxVal -> 0..255
                                             : 1.0;               // already in 0..255 space
                    processedUMat.convertTo(out8, CV_8UC1, alpha, 0.0);
                    processedUMat = out8;
                } else if (processedUMat.type() == CV_16UC1) {
                    processedUMat.convertTo(out8, CV_8UC1, 255.0 / maxVal, 0.0);
                    processedUMat = out8;
                } else {
                    // Fallback for algorithms producing other intermediate types
                    cv::UMat normalized;
                    cv::normalize(processedUMat, normalized, 0, 255, cv::NORM_MINMAX);
                    normalized.convertTo(processedUMat, CV_8UC1);
                }
            }

            // IMPORTANT: Use copyTo instead of getMat to ensure data ownership.
            // getMat(ACCESS_READ) returns a reference that becomes invalid when UMat is destroyed.
            processedUMat.copyTo(processedMat);
            inputUMat.copyTo(originalMat);
        }
        
        // Create QImages
        QImage processedImage(processedMat.data, processedMat.cols, processedMat.rows,
                             processedMat.step, QImage::Format_Grayscale8);
        QImage processedCopy = processedImage.copy();
        
        QImage originalImage(originalMat.data, originalMat.cols, originalMat.rows,
                            originalMat.step, QImage::Format_Grayscale8);
        QImage originalCopy = originalImage.copy();
        
        emit imageProcessed(processedCopy, originalCopy);
        
        // Update statistics
        qint64 elapsed = timer.elapsed();
        m_frameCount++;
        
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - m_lastStatisticsTime >= 1000) {
            double fps = m_frameCount * 1000.0 / (currentTime - m_lastStatisticsTime);
            emit statisticsUpdated(elapsed, fps);
            
            m_frameCount = 0;
            m_lastStatisticsTime = currentTime;
        }
        
    } catch (const cv::Exception &e) {
        emit errorOccurred(QString("OpenCV exception: %1").arg(e.what()));
    } catch (const std::exception &e) {
        emit errorOccurred(QString("Standard exception: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("Unknown exception occurred");
    }
    
    m_isProcessing.store(false);
}

void ImageAlgorithmBase::processImage(const QImage &image)
{
    if (m_shouldStop.load() || !m_enabled.load()) {
        return;
    }
    
    if (m_isProcessing.load()) {
        return;
    }
    
    m_isProcessing.store(true);
    
    QElapsedTimer timer;
    timer.start();
    
    try {
        // Convert to grayscale if needed
        QImage grayImage = image;
        if (image.format() != QImage::Format_Grayscale8) {
            grayImage = image.convertToFormat(QImage::Format_Grayscale8);
        }
        
        if (grayImage.isNull()) {
            emit errorOccurred("Failed to convert image to grayscale");
            m_isProcessing.store(false);
            return;
        }
        
        // Create Mat wrapper from QImage data
        // Note: QImage may have padding (bytesPerLine != width), making it non-contiguous.
        cv::Mat matWrapper(grayImage.height(), grayImage.width(), CV_8UC1,
                   const_cast<uchar*>(grayImage.bits()), grayImage.bytesPerLine());
        
        cv::Mat processedMat;

        bool useCuda = false;
        bool cudaFailed = false;
#ifdef CAMERUI_ENABLE_CUDA
        useCuda = m_cudaAvailable && supportsCuda();

        if (useCuda) {
            // CUDA path with automatic fallback on error
            try {
                cv::Mat mat = matWrapper.isContinuous() ? matWrapper : matWrapper.clone();
                cv::cuda::GpuMat gpuInput;
                gpuInput.upload(mat, *m_cudaStream);

                cv::cuda::GpuMat gpuProcessed;
                {
                    QMutexLocker locker(&m_paramsMutex);
                    gpuProcessed = processImplCuda(gpuInput, *m_cudaStream);
                }

                if (gpuProcessed.empty()) {
                     gpuInput.download(processedMat, *m_cudaStream);
                } else {
                     gpuProcessed.download(processedMat, *m_cudaStream);
                }
                m_cudaStream->waitForCompletion();
            } catch (const cv::Exception &e) {
                qWarning() << "CUDA processing failed for" << algorithmName() << ":" << e.what() << "- falling back to CPU/OpenCL";
                cudaFailed = true;
                processedMat = cv::Mat(); // Clear any partial results
            }
        }
#endif // CAMERUI_ENABLE_CUDA

        // Fallback to OpenCL/CPU if CUDA failed or not available
        if (!useCuda || cudaFailed) {
            if (m_openclReliable) {
                // OpenCL path - only when OpenCL has been tested and works
                try {
                    cv::UMat uMat;
                    cv::UMat processed;
                    
                    // UMat conversion strategy:
                    // - For continuous Mat: use getUMat (zero-copy, fast)
                    // - For non-continuous Mat (QImage with padding): use copyTo (safe)
                    if (matWrapper.isContinuous()) {
                        uMat = matWrapper.getUMat(cv::ACCESS_READ);
                    } else {
                        matWrapper.copyTo(uMat);
                    }
                    
                    // Process the image
                    {
                        QMutexLocker locker(&m_paramsMutex);
                        processed = processImpl(uMat);
                    }
                    
                    // IMPORTANT: Copy result to Mat before UMat goes out of scope.
                    if (!processed.empty()) {
                        // Ensure output is 8-bit for display
                        if (processed.type() != CV_8UC1) {
                            cv::UMat out8;
                            if (processed.type() == CV_32F) {
                                processed.convertTo(out8, CV_8UC1, 1.0, 0.0);
                            } else {
                                cv::UMat normalized;
                                cv::normalize(processed, normalized, 0, 255, cv::NORM_MINMAX);
                                normalized.convertTo(out8, CV_8UC1);
                            }
                            out8.copyTo(processedMat);
                        } else {
                            processed.copyTo(processedMat);
                        }
                    }
                } catch (const cv::Exception &e) {
                    qWarning() << "OpenCL processing failed for" << algorithmName() << ":" << e.what() << "- falling back to pure CPU";
                    processedMat = cv::Mat(); // Clear any partial results
                }
            }
            
            // Pure CPU fallback if OpenCL failed or not available
            if (processedMat.empty()) {
                // Pure CPU path - safest fallback when no CUDA and OpenCL is unreliable
                // When OpenCL is disabled, UMat operations fall back to CPU automatically.
                // We still use UMat API but with OpenCL disabled at initialization.
                cv::Mat mat = matWrapper.isContinuous() ? matWrapper : matWrapper.clone();
                
                // Convert to UMat - since OpenCL is disabled, this will use CPU memory
                cv::UMat uMat;
                mat.copyTo(uMat);
                
                cv::UMat processed;
                {
                    QMutexLocker locker(&m_paramsMutex);
                    processed = processImpl(uMat);
                }
                
                // Copy result
                if (!processed.empty()) {
                    if (processed.type() != CV_8UC1) {
                        cv::UMat out8;
                        if (processed.type() == CV_32F) {
                            processed.convertTo(out8, CV_8UC1, 1.0, 0.0);
                        } else {
                            cv::UMat normalized;
                            cv::normalize(processed, normalized, 0, 255, cv::NORM_MINMAX);
                            normalized.convertTo(out8, CV_8UC1);
                        }
                        out8.copyTo(processedMat);
                    } else {
                        processed.copyTo(processedMat);
                    }
                }
            }
        }
        
        if (processedMat.empty()) {
            m_isProcessing.store(false);
            return;
        }
        
        QImage processedImage(processedMat.data, processedMat.cols, processedMat.rows,
                             processedMat.step, QImage::Format_Grayscale8);
        QImage processedCopy = processedImage.copy();
        
        emit imageProcessed(processedCopy, grayImage.copy());
        
        // Update statistics
        qint64 elapsed = timer.elapsed();
        m_frameCount++;
        
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - m_lastStatisticsTime >= 1000) {
            double fps = m_frameCount * 1000.0 / (currentTime - m_lastStatisticsTime);
            emit statisticsUpdated(elapsed, fps);
            
            m_frameCount = 0;
            m_lastStatisticsTime = currentTime;
        }
        
    } catch (const cv::Exception &e) {
        emit errorOccurred(QString("OpenCV exception: %1").arg(e.what()));
    } catch (const std::exception &e) {
        emit errorOccurred(QString("Standard exception: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("Unknown exception occurred");
    }
    
    m_isProcessing.store(false);
}

void ImageAlgorithmBase::updateParameters(const QVariantMap &params)
{
    QMutexLocker locker(&m_paramsMutex);
    for (auto it = params.begin(); it != params.end(); ++it) {
        m_parameters[it.key()] = it.value();
    }
}

void ImageAlgorithmBase::setEnabled(bool enabled)
{
    m_enabled.store(enabled);
    if (!enabled) {
        m_frameCount = 0;
        m_lastStatisticsTime = QDateTime::currentMSecsSinceEpoch();
    }
}

void ImageAlgorithmBase::setInputScaleMode(int mode)
{
    m_inputScaleMode = (mode == 1) ? InputScaleMode::Native : InputScaleMode::ScaleTo255;
}

void ImageAlgorithmBase::stop()
{
    m_shouldStop.store(true);
    m_enabled.store(false);
}

QVariant ImageAlgorithmBase::getParameter(const QString &name) const
{
    QMutexLocker locker(&m_paramsMutex);
    return m_parameters.value(name);
}

cv::UMat ImageAlgorithmBase::convertRawTo8Bit(const QVector<uint16_t> &rawData, int width, int height, int bitDepth)
{
    if (rawData.isEmpty() || width <= 0 || height <= 0 || rawData.size() < width * height) {
        return cv::UMat();
    }

    try {
        // Wrap raw data as OpenCV Mat
        cv::Mat rawMat(height, width, CV_16UC1, const_cast<uint16_t*>(rawData.constData()));
        
        // Convert to UMat for GPU acceleration
        // Use copyTo instead of getUMat for better OpenCL driver compatibility
        cv::UMat uRawMat;
        rawMat.copyTo(uRawMat);
        cv::UMat uMat8bit;
        
        double maxVal = (1 << bitDepth) - 1.0;
        uRawMat.convertTo(uMat8bit, CV_8UC1, 255.0 / maxVal);
        
        return uMat8bit;
    } catch (const cv::Exception &e) {
        qWarning() << "OpenCV error in convertRawTo8Bit:" << e.what();
        return cv::UMat();
    } catch (...) {
        qWarning() << "Unknown error in convertRawTo8Bit";
        return cv::UMat();
    }
}

cv::UMat ImageAlgorithmBase::execute(const cv::UMat &input)
{
    QMutexLocker locker(&m_paramsMutex);
    return processImpl(input);
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat ImageAlgorithmBase::executeCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    QMutexLocker locker(&m_paramsMutex);
    return processImplCuda(input, stream);
}
#endif // CAMERUI_ENABLE_CUDA

// Factory implementation
ImageAlgorithmFactory& ImageAlgorithmFactory::instance()
{
    static ImageAlgorithmFactory factory;
    return factory;
}

ImageAlgorithmBase* ImageAlgorithmFactory::createAlgorithm(const QString &id)
{
    auto it = m_creators.find(id);
    if (it != m_creators.end()) {
        return it.value()();
    }
    return nullptr;
}

QVector<AlgorithmInfo> ImageAlgorithmFactory::allAlgorithmInfos() const
{
    QVector<AlgorithmInfo> infos;
    for (const auto &info : m_algorithmInfos) {
        infos.append(info);
    }
    
    std::sort(infos.begin(), infos.end(), [](const AlgorithmInfo& a, const AlgorithmInfo& b) {
        if (a.category != b.category) {
            return a.category < b.category;
        }
        return a.name < b.name;
    });
    
    return infos;
}

QVector<AlgorithmInfo> ImageAlgorithmFactory::algorithmInfosByCategory(const QString &category) const
{
    QVector<AlgorithmInfo> infos;
    for (const auto &info : m_algorithmInfos) {
        if (info.category == category) {
            infos.append(info);
        }
    }
    
    std::sort(infos.begin(), infos.end(), [](const AlgorithmInfo& a, const AlgorithmInfo& b) {
        return a.name < b.name;
    });
    
    return infos;
}

QStringList ImageAlgorithmFactory::categories() const
{
    QSet<QString> cats;
    for (const auto &info : m_algorithmInfos) {
        cats.insert(info.category);
    }
    QStringList result = cats.values();
    std::sort(result.begin(), result.end());
    return result;
}

// AlgorithmPrecisionUtils namespace implementations
namespace AlgorithmPrecisionUtils {

bool shouldHideInHighPrecision(const QString &algorithmId)
{
    // These algorithms are implemented as 8-bit-only (or effectively identical to 8-bit)
    // in the current codebase (supportsFloat32Input() == false).
    // Hide them when running in high precision 16-bit mode.
    static const QSet<QString> kHiddenIds = {
        QStringLiteral("enhance.gamma"),
        QStringLiteral("enhance.histogram_eq"),
        QStringLiteral("enhance.clahe"),
        QStringLiteral("enhance.negative"),
        QStringLiteral("threshold.adaptive"),
        QStringLiteral("threshold.otsu"),
        QStringLiteral("edge.canny"),
    };

    return kHiddenIds.contains(algorithmId);
}

QVariantMap mapUiParamsToNativeRange(const QVariantMap &uiParams, int bitDepth)
{
    // No longer mapping 8-bit parameters to 16-bit range
    // Return parameters unchanged - user inputs are used directly
    Q_UNUSED(bitDepth);
    return uiParams;
}

} // namespace AlgorithmPrecisionUtils
