#include "histogram_equalization_worker.h"
#include <QElapsedTimer>
#include <QDateTime>

HistogramEqualizationWorker::HistogramEqualizationWorker(QObject *parent)
    : QObject(parent)
{
    m_lastStatisticsTime = QDateTime::currentMSecsSinceEpoch();
    
    // 启用OpenCV优化和多线程
    cv::setUseOptimized(true);
    cv::setNumThreads(0); // 0表示使用所有可用线程
    
    // 尝试启用OpenCL加速
    if (cv::ocl::haveOpenCL()) {
        cv::ocl::setUseOpenCL(true);
        if (cv::ocl::useOpenCL()) {
            cv::ocl::Context context = cv::ocl::Context::getDefault();
            if (!context.empty()) {
                
            }
        }
    } else {
        
    }
    
    // 输出优化状态
    
}

HistogramEqualizationWorker::~HistogramEqualizationWorker()
{
    stop();
}

void HistogramEqualizationWorker::processRawImage(const QVector<uint16_t> &rawData, int width, int height, int bitDepth)
{
    if (m_shouldStop.load() || !m_params.enabled) {
        return;
    }
    
    if (m_isProcessing.load()) {
        // 如果正在处理，跳过本帧以避免队列积压
        return;
    }
    
    m_isProcessing.store(true);
    
    QElapsedTimer timer;
    timer.start();
    
    try {
        // 1. 将原始16位数据转换为OpenCV Mat（零拷贝，直接包装QVector数据）
        cv::Mat rawMat(height, width, CV_16UC1, const_cast<uint16_t*>(rawData.constData()));
        
        // 2. 归一化到8位用于直方图均衡化（使用UMat以利用OpenCL）
        // 使用 copyTo 代替 getUMat 以获得更好的 OpenCL 驱动兼容性
        cv::UMat uRawMat;
        rawMat.copyTo(uRawMat);
        cv::UMat uMat8bit;
        double maxVal = (1 << bitDepth) - 1.0;
        uRawMat.convertTo(uMat8bit, CV_8UC1, 255.0 / maxVal);
        
        // 3. 执行直方图均衡化（在UMat上操作，利用OpenCL加速）
        cv::UMat uEqualizedMat;
        {
            QMutexLocker locker(&m_paramsMutex);
            if (m_params.adaptiveMode) {
                uEqualizedMat = performCLAHE(uMat8bit);
            } else {
                uEqualizedMat = performHistogramEqualization(uMat8bit);
            }
        }
        
        // 转回Mat用于QImage创建
        // IMPORTANT: Use copyTo instead of getMat to ensure data ownership.
        cv::Mat equalizedMat;
        cv::Mat mat8bit;
        uEqualizedMat.copyTo(equalizedMat);
        uMat8bit.copyTo(mat8bit);
        
        // 4. 转换为QImage
        QImage equalizedImage(equalizedMat.data, equalizedMat.cols, equalizedMat.rows,
                             equalizedMat.step, QImage::Format_Grayscale8);
        QImage equalizedCopy = equalizedImage.copy(); // 深拷贝以避免数据生命周期问题
        
        // 5. 生成原始图像的8位版本用于对比
        QImage originalImage(mat8bit.data, mat8bit.cols, mat8bit.rows,
                            mat8bit.step, QImage::Format_Grayscale8);
        QImage originalCopy = originalImage.copy();
        
        // 6. 发送结果
        emit imageProcessed(equalizedCopy, originalCopy);
        
        // 7. 更新统计信息
        qint64 elapsed = timer.elapsed();
        m_frameCount++;
        
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - m_lastStatisticsTime >= 1000) { // 每秒更新一次统计
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

void HistogramEqualizationWorker::processImage(const QImage &image)
{
    if (m_shouldStop.load() || !m_params.enabled) {
        return;
    }
    
    if (m_isProcessing.load()) {
        return;
    }
    
    m_isProcessing.store(true);
    
    QElapsedTimer timer;
    timer.start();
    
    try {
        // 转换为灰度图像
        QImage grayImage = image;
        if (image.format() != QImage::Format_Grayscale8) {
            grayImage = image.convertToFormat(QImage::Format_Grayscale8);
        }
        
        if (grayImage.isNull()) {
            emit errorOccurred("Failed to convert image to grayscale");
            m_isProcessing.store(false);
            return;
        }
        
        // 转换为OpenCV Mat
        // 注意: QImage 可能有 padding (bytesPerLine != width), 导致非连续内存
        cv::Mat matWrapper(grayImage.height(), grayImage.width(), CV_8UC1,
                   const_cast<uchar*>(grayImage.bits()), grayImage.bytesPerLine());
        
        // UMat 转换策略:
        // - 连续 Mat: 使用 getUMat (零拷贝, 快速)
        // - 非连续 Mat (QImage 有 padding): 使用 copyTo (安全)
        // 非连续 Mat + getUMat 在某些 OpenCL 驱动上会崩溃
        cv::UMat uMat;
        if (matWrapper.isContinuous()) {
            uMat = matWrapper.getUMat(cv::ACCESS_READ);
        } else {
            matWrapper.copyTo(uMat);
        }
        
        // 执行直方图均衡化
        cv::UMat uEqualizedMat;
        {
            QMutexLocker locker(&m_paramsMutex);
            if (m_params.adaptiveMode) {
                uEqualizedMat = performCLAHE(uMat);
            } else {
                uEqualizedMat = performHistogramEqualization(uMat);
            }
        }
        
        // 转回Mat用于QImage创建
        // IMPORTANT: Use copyTo instead of getMat to ensure data ownership.
        cv::Mat equalizedMat;
        uEqualizedMat.copyTo(equalizedMat);
        
        // 转换为QImage
        QImage equalizedImage(equalizedMat.data, equalizedMat.cols, equalizedMat.rows,
                             equalizedMat.step, QImage::Format_Grayscale8);
        QImage equalizedCopy = equalizedImage.copy();
        
        // 发送结果
        emit imageProcessed(equalizedCopy, grayImage.copy());
        
        // 更新统计
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

void HistogramEqualizationWorker::updateParameters(const HistogramEqualizationParams &params)
{
    QMutexLocker locker(&m_paramsMutex);
    m_params = params;
}

void HistogramEqualizationWorker::stop()
{
    m_shouldStop.store(true);
    m_params.enabled = false;
}

void HistogramEqualizationWorker::setEnabled(bool enabled)
{
    QMutexLocker locker(&m_paramsMutex);
    m_params.enabled = enabled;
    
    if (!enabled) {
        m_frameCount = 0;
        m_lastStatisticsTime = QDateTime::currentMSecsSinceEpoch();
    }
}

cv::UMat HistogramEqualizationWorker::performHistogramEqualization(const cv::UMat &input)
{
    cv::UMat output;
    cv::equalizeHist(input, output);
    return output;
}

cv::UMat HistogramEqualizationWorker::performCLAHE(const cv::UMat &input)
{
    // CLAHE (Contrast Limited Adaptive Histogram Equalization)
    // 自适应直方图均衡化，可以更好地处理局部对比度
    // OpenCL加速的CLAHE
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(m_params.clipLimit, 
                                                cv::Size(m_params.tileGridSize, m_params.tileGridSize));
    cv::UMat output;
    clahe->apply(input, output);
    return output;
}

QImage HistogramEqualizationWorker::convertRawTo8Bit(const QVector<uint16_t> &rawData, 
                                                      int width, int height, int bitDepth)
{
    QImage image(width, height, QImage::Format_Grayscale8);
    
    if (rawData.isEmpty() || width <= 0 || height <= 0) {
        return QImage();
    }
    
    double maxVal = (1 << bitDepth) - 1.0;
    
    for (int y = 0; y < height; ++y) {
        uchar *scanLine = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (idx < rawData.size()) {
                uint16_t val = rawData[idx];
                scanLine[x] = static_cast<uchar>(val * 255.0 / maxVal);
            }
        }
    }
    
    return image;
}
