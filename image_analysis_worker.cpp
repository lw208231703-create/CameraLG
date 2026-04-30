#include "image_analysis_worker.h"
#include <QMutexLocker>
#include <QDebug>
#include <cmath>
#include <opencv2/opencv.hpp>
// 注册自定义类型
static int imageStatisticsMetaTypeId = qRegisterMetaType<ImageStatistics>("ImageStatistics");

ImageAnalysisWorker::ImageAnalysisWorker(QObject *parent)
    : QObject(parent)
{
}

ImageAnalysisWorker::~ImageAnalysisWorker()
{
    stop();
}

void ImageAnalysisWorker::analyzeImage(const QImage &image, const QRect &roi)
{
    if (m_shouldStop.load()) {
        return;
    }
    
    // 如果已经在分析中，跳过此次请求（避免积压）
    if (m_isAnalyzing.exchange(true)) {
        return;
    }
    
    ImageStatistics stats = computeStatistics(image, roi);
    
    m_isAnalyzing.store(false);
    
    if (!m_shouldStop.load() && stats.valid) {
        emit analysisComplete(stats);
    }
}

void ImageAnalysisWorker::analyzeRawData(const QVector<uint16_t> &data, int width, int height, int bitDepth, const QRect &roi)
{
    if (m_shouldStop.load()) {
        return;
    }
    
    if (m_isAnalyzing.exchange(true)) {
        return;
    }
    
    ImageStatistics stats = computeStatisticsRaw(data, width, height, bitDepth, roi);
    
    m_isAnalyzing.store(false);
    
    if (!m_shouldStop.load() && stats.valid) {
        emit analysisComplete(stats);
    }
}

void ImageAnalysisWorker::analyzeSinglePixel(const QImage &image, const QPoint &point)
{
    if (m_shouldStop.load()) {
        return;
    }
    
    if (m_isAnalyzing.exchange(true)) {
        return;
    }
    
    ImageStatistics stats;
    stats.bitDepth = 8;
    stats.histogram.resize(256);
    stats.histogram.fill(0);
    
    if (image.isNull() || !image.rect().contains(point)) {
        m_isAnalyzing.store(false);
        return;
    }
    
    // 获取像素值
    int pixelValue = 0;
    if (image.format() == QImage::Format_Grayscale8 || image.format() == QImage::Format_Indexed8) {
        pixelValue = image.pixelIndex(point);
    } else {
        pixelValue = qGray(image.pixel(point));
    }
    
    // 更新历史数据
    QMutexLocker locker(&m_mutex);
    m_pixelHistory.append(static_cast<double>(pixelValue));
    while (m_pixelHistory.size() > m_historySize) {
        m_pixelHistory.removeFirst();
    }
    m_pixelHistoryValid = !m_pixelHistory.isEmpty();

    if (m_pixelHistoryValid) {
        double sum = 0.0;
        double minVal = m_pixelHistory[0];
        double maxVal = m_pixelHistory[0];
        
        for(double val : m_pixelHistory) {
            sum += val;
            if(val < minVal) minVal = val;
            if(val > maxVal) maxVal = val;
        }
        
        stats.minValue = minVal;
        stats.maxValue = maxVal;
        stats.range = maxVal - minVal;
        stats.mean = sum / m_pixelHistory.size();
        
        double sumSqDiff = 0.0;
        for(double val : m_pixelHistory) {
            double diff = val - stats.mean;
            sumSqDiff += diff * diff;
        }
        stats.variance = sumSqDiff / m_pixelHistory.size();
        stats.stdDev = std::sqrt(stats.variance);
        
        // 直方图显示历史数据的分布
        for(double val : m_pixelHistory) {
            int v = static_cast<int>(val);
            if (v >= 0 && v < stats.histogram.size()) {
                stats.histogram[v]++;
            }
        }
    }
    
    stats.historySize = m_historySize;
    stats.currentSampleCount = m_pixelHistory.size();
    
    stats.valid = true;
    locker.unlock();
    
    m_isAnalyzing.store(false);
    
    if (!m_shouldStop.load()) {
        emit analysisComplete(stats);
    }
}

void ImageAnalysisWorker::analyzeSinglePixelRaw(const QVector<uint16_t> &data, int width, int height, int bitDepth, const QPoint &point)
{
    if (m_shouldStop.load()) {
        return;
    }
    
    if (m_isAnalyzing.exchange(true)) {
        return;
    }
    
    ImageStatistics stats;
    stats.bitDepth = bitDepth;
    int histSize = 1 << bitDepth;
    stats.histogram.resize(histSize);
    stats.histogram.fill(0);
    
    int index = point.y() * width + point.x();
    if (data.isEmpty() || index < 0 || index >= data.size()) {
        m_isAnalyzing.store(false);
        return;
    }
    
    int pixelValue = data[index];
    
    // 更新历史数据
    QMutexLocker locker(&m_mutex);
    m_pixelHistory.append(static_cast<double>(pixelValue));
    while (m_pixelHistory.size() > m_historySize) {
        m_pixelHistory.removeFirst();
    }
    m_pixelHistoryValid = !m_pixelHistory.isEmpty();

    if (m_pixelHistoryValid) {
        double sum = 0.0;
        double minVal = m_pixelHistory[0];
        double maxVal = m_pixelHistory[0];
        
        for(double val : m_pixelHistory) {
            sum += val;
            if(val < minVal) minVal = val;
            if(val > maxVal) maxVal = val;
        }
        
        stats.minValue = minVal;
        stats.maxValue = maxVal;
        stats.range = maxVal - minVal;
        stats.mean = sum / m_pixelHistory.size();
        
        double sumSqDiff = 0.0;
        for(double val : m_pixelHistory) {
            double diff = val - stats.mean;
            sumSqDiff += diff * diff;
        }
        stats.variance = sumSqDiff / m_pixelHistory.size();
        stats.stdDev = std::sqrt(stats.variance);
        
        // 直方图显示历史数据的分布
        for(double val : m_pixelHistory) {
            int v = static_cast<int>(val);
            if (v >= 0 && v < stats.histogram.size()) {
                stats.histogram[v]++;
            }
        }
    }
    
    stats.historySize = m_historySize;
    stats.currentSampleCount = m_pixelHistory.size();
    
    stats.valid = true;
    locker.unlock();
    
    m_isAnalyzing.store(false);
    
    if (!m_shouldStop.load()) {
        emit analysisComplete(stats);
    }
}

void ImageAnalysisWorker::resetSinglePixelHistory()
{
    QMutexLocker locker(&m_mutex);
    m_pixelHistory.clear();
    m_pixelHistoryValid = false;
}

void ImageAnalysisWorker::setHistorySize(int size)
{
    QMutexLocker locker(&m_mutex);
    m_historySize = size;
    while (m_pixelHistory.size() > m_historySize) {
        m_pixelHistory.removeFirst();
    }
}

void ImageAnalysisWorker::stop()
{
    m_shouldStop.store(true);
}

ImageStatistics ImageAnalysisWorker::computeStatistics(const QImage &image, const QRect &roi)
{
    ImageStatistics stats;
    stats.bitDepth = 8;
    stats.histogram.resize(256);
    stats.histogram.fill(0);
    
    if (image.isNull()) {
        return stats;
    }
    
    // 转换为灰度图像进行分析
    QImage grayImage = image;
    if (image.format() != QImage::Format_Grayscale8 && 
        image.format() != QImage::Format_Indexed8) {
        grayImage = image.convertToFormat(QImage::Format_Grayscale8);
    }
    
    if (grayImage.isNull()) {
        return stats;
    }
    
    // 使用 OpenCV 进行加速计算
    try {
        // 构造 cv::Mat (共享内存，不拷贝)
        cv::Mat mat(grayImage.height(), grayImage.width(), CV_8UC1, 
                    const_cast<uchar*>(grayImage.bits()), grayImage.bytesPerLine());
        
        cv::Mat processMat = mat;
        if (!roi.isEmpty()) {
            QRect intersect = roi.intersected(grayImage.rect());
            if (intersect.isEmpty()) return stats;
            
            cv::Rect cvRoi(intersect.x(), intersect.y(), intersect.width(), intersect.height());
            processMat = mat(cvRoi);
        }
        
        if (m_shouldStop.load()) return stats;

        // 1. 计算最小值和最大值
        double minVal, maxVal;
        cv::minMaxLoc(processMat, &minVal, &maxVal);
        stats.minValue = minVal;
        stats.maxValue = maxVal;
        stats.range = maxVal - minVal;
        
        if (m_shouldStop.load()) return stats;

        // 2. 计算均值和标准差
        cv::Scalar mean, stddev;
        cv::meanStdDev(processMat, mean, stddev);
        stats.mean = mean[0];
        stats.stdDev = stddev[0];
        stats.variance = stats.stdDev * stats.stdDev;
        
        if (m_shouldStop.load()) return stats;

        // 3. 计算直方图
        int channels[] = {0};
        int histSize[] = {256};
        float range[] = {0, 256};
        const float* ranges[] = {range};
        cv::Mat hist;
        
        cv::calcHist(&processMat, 1, channels, cv::Mat(), hist, 1, histSize, ranges);
        
        // 填充结果直方图
        for(int i = 0; i < 256; ++i) {
            stats.histogram[i] = cvRound(hist.at<float>(i));
        }
        
        stats.valid = true;
    } catch (const cv::Exception& e) {
        qWarning() << "OpenCV error in computeStatistics:" << e.what();
    }
    
    return stats;
}

ImageStatistics ImageAnalysisWorker::computeStatisticsRaw(const QVector<uint16_t> &data, int width, int height, int bitDepth, const QRect &roi)
{
    ImageStatistics stats;
    stats.bitDepth = bitDepth;
    int histSizeVal = 1 << bitDepth;
    stats.histogram.resize(histSizeVal);
    stats.histogram.fill(0);
    
    int totalPixels = width * height;
    if (data.size() != totalPixels || totalPixels == 0) {
        return stats;
    }
    
    // 使用 OpenCV 进行加速计算
    try {
        // 构造 cv::Mat (共享内存，不拷贝)
        cv::Mat mat(height, width, CV_16UC1, const_cast<uint16_t*>(data.data()));
        
        cv::Mat processMat = mat;
        if (!roi.isEmpty()) {
            QRect intersect = roi.intersected(QRect(0, 0, width, height));
            if (intersect.isEmpty()) return stats;
            
            cv::Rect cvRoi(intersect.x(), intersect.y(), intersect.width(), intersect.height());
            processMat = mat(cvRoi);
        }
        
        if (m_shouldStop.load()) return stats;

        // 1. 计算最小值和最大值
        double minVal, maxVal;
        cv::minMaxLoc(processMat, &minVal, &maxVal);
        stats.minValue = minVal;
        stats.maxValue = maxVal;
        stats.range = maxVal - minVal;
        
        if (m_shouldStop.load()) return stats;

        // 2. 计算均值和标准差
        cv::Scalar mean, stddev;
        cv::meanStdDev(processMat, mean, stddev);
        stats.mean = mean[0];
        stats.stdDev = stddev[0];
        stats.variance = stats.stdDev * stats.stdDev;
        
        if (m_shouldStop.load()) return stats;

        // 3. 计算直方图
        int channels[] = {0};
        int histSize[] = {histSizeVal};
        float range[] = {0, (float)histSizeVal};
        const float* ranges[] = {range};
        cv::Mat hist;
        
        cv::calcHist(&processMat, 1, channels, cv::Mat(), hist, 1, histSize, ranges);
        
        // 填充结果直方图
        // 注意：如果 bitDepth 很大（如16位），直方图可能很大，这里假设 stats.histogram 已经正确 resize
        // OpenCV 输出的 hist 是 float 类型
        for(int i = 0; i < histSizeVal; ++i) {
            if (i < hist.rows) {
                stats.histogram[i] = cvRound(hist.at<float>(i));
            }
        }
        
        stats.valid = true;
    } catch (const cv::Exception& e) {
       // qWarning() << "OpenCV error in computeStatisticsRaw:" << e.what();
    }
    
    return stats;
}
