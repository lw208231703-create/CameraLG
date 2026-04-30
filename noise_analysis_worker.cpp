#include "noise_analysis_worker.h"
#include <QMutexLocker>
#include <QRect>
#include <cmath>
#include <QThread>
#include <QFile>
#include <QImageReader>
#include <QByteArray>

// 注册自定义类型
static int noiseAnalysisResultMetaTypeId = qRegisterMetaType<NoiseAnalysisResult>("NoiseAnalysisResult");

NoiseAnalysisWorker::NoiseAnalysisWorker(QObject *parent)
    : QObject(parent)
{
}

NoiseAnalysisWorker::~NoiseAnalysisWorker()
{
    stop();
    cleanup();
}

void NoiseAnalysisWorker::startAnalysis(int gainCount, int samplesPerGain, int roiWidth, int roiHeight)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isAnalyzing.load()) {
        qWarning() << "NoiseAnalysisWorker: Analysis already in progress";
        emit analysisError(tr("分析已在进行中"));
        return;
    }
    
    // 清空之前的数据
    m_frameQueue.clear();
    m_sumMat = cv::Mat::zeros(roiHeight, roiWidth, CV_64F);
    m_sqSumMat = cv::Mat::zeros(roiHeight, roiWidth, CV_64F);
    
    // 设置新的分析参数
    m_gainCount = gainCount;
    m_samplesPerGain = samplesPerGain;
    m_roiWidth = roiWidth;
    m_roiHeight = roiHeight;
    m_currentGainIndex = 0;
    m_currentFrameCount = 0;
    m_shouldStop.store(false);
    m_isAnalyzing.store(true);
    
}

void NoiseAnalysisWorker::startFileAnalysis(const QStringList &files, const QRect &roi)
{
    if (files.isEmpty()) {
        emit analysisError(tr("未选择文件"));
        return;
    }
    
    // 读取第一张图片以获取尺寸
    cv::Mat firstImg;
    
    // 优化：使用 lambda 加载图像，优先保持原始数据完整性
    auto loadImageFromQtPath = [](const QString &path) -> cv::Mat {
        // 优先尝试基于 QByteArray + OpenCV imdecode（保持原始位深，性能更好）
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray bytes = file.readAll();
            if (!bytes.isEmpty()) {
                // 使用 cv::_InputArray 避免额外的 vector 拷贝
                cv::Mat img = cv::imdecode(
                    cv::_InputArray(reinterpret_cast<const uchar*>(bytes.constData()), bytes.size()),
                    cv::IMREAD_UNCHANGED);
                if (!img.empty()) {
                    return img;
                }
            }
        }
        
        // 回退到 QImageReader（支持更多格式和 Unicode 路径）
        QImageReader reader(path);
        reader.setAutoTransform(false);  // 禁用自动变换以保持原始数据
        QImage qimg = reader.read();
        if (!qimg.isNull()) {
            QImage conv = qimg.convertToFormat(QImage::Format_Grayscale16);
            int w = conv.width();
            int h = conv.height();
            
            // 优化：检查 QImage 是否连续存储，避免逐行拷贝
            int bytesPerLine = conv.bytesPerLine();
            int expectedBytesPerLine = w * sizeof(quint16);
            
            cv::Mat mat(h, w, CV_16UC1);
            if (bytesPerLine == expectedBytesPerLine) {
                // 连续存储，直接整块拷贝
                memcpy(mat.data, conv.constBits(), static_cast<size_t>(h) * expectedBytesPerLine);
            } else {
                // 非连续存储，使用优化的逐行拷贝
                const uchar* srcData = conv.constBits();
                uchar* dstData = mat.data;
                for (int y = 0; y < h; ++y) {
                    memcpy(dstData + y * expectedBytesPerLine, 
                           srcData + y * bytesPerLine, 
                           expectedBytesPerLine);
                }
            }
            return mat;
        }
        
        return cv::Mat();
    };

    firstImg = loadImageFromQtPath(files[0]);
    
    if (firstImg.empty()) {
        emit analysisError(tr("无法读取文件: ") + files[0]);
        return;
    }
    
    int imgWidth = firstImg.cols;
    int imgHeight = firstImg.rows;
    
    // 确定有效的 ROI
    QRect validRoi = roi;
    if (validRoi.isEmpty()) {
        validRoi = QRect(0, 0, imgWidth, imgHeight);
    } else {
        validRoi = validRoi.intersected(QRect(0, 0, imgWidth, imgHeight));
    }
    
    if (validRoi.isEmpty()) {
        emit analysisError(tr("无效的分析区域"));
        return;
    }
    
    // 启动分析 (1个增益配置, 样本数为文件数, 使用 ROI 尺寸)
    startAnalysis(1, files.size(), validRoi.width(), validRoi.height());
    
    // 循环处理文件
    for (int i = 0; i < files.size(); ++i) {
        // 检查是否请求停止
        if (m_shouldStop.load()) {
            break;
        }
        
        cv::Mat img = loadImageFromQtPath(files[i]);
        if (img.empty()) {
            qWarning() << "Failed to read image:" << files[i];
            continue;
        }
        
        // 确保尺寸匹配
        if (img.cols != imgWidth || img.rows != imgHeight) {
            qWarning() << "Image size mismatch:" << files[i];
            continue;
        }
        
        // 转换为 16位无符号整数
        cv::Mat img16;
        if (img.type() == CV_8UC1) {
            img.convertTo(img16, CV_16U); 
        } else if (img.type() == CV_16UC1) {
            img16 = img;
        } else if (img.channels() > 1) {
            // 多通道图像，先转灰度再转16位
            cv::Mat gray;
            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
            if (gray.depth() != CV_16U) {
                gray.convertTo(img16, CV_16U);
            } else {
                img16 = gray;
            }
        } else {
            img.convertTo(img16, CV_16U);
        }
        
        // 优化：直接使用 cv::Mat 进行帧累加，避免 QVector 中转拷贝
        addFrameDataDirect(img16, validRoi, 0);
        
        if (i % 5 == 0) {
            QThread::yieldCurrentThread();
        }
    }
}

void NoiseAnalysisWorker::addFrameData(const QVector<uint16_t> &rawData, int width, int height, 
                                       const QRect &roi, int gainIndex)
{
    if (m_shouldStop.load() || !m_isAnalyzing.load()) {
        return;
    }
    
    // 验证增益索引
    if (gainIndex != m_currentGainIndex) {
        qWarning() << "NoiseAnalysisWorker: Received frame for wrong gain index"
                   << "expected:" << m_currentGainIndex << "got:" << gainIndex;
        return;
    }
    
    // 确保ROI有效
    QRect validRoi = roi.intersected(QRect(0, 0, width, height));
    if (validRoi.isEmpty()) {
        qWarning() << "NoiseAnalysisWorker: ROI is outside image bounds";
        return;
    }
    
    // 如果ROI被裁剪，发出警告（但仍继续处理有效部分）
    if (validRoi != roi) {
        qWarning() << "NoiseAnalysisWorker: ROI clipped from" << roi << "to" << validRoi 
                   << "(image size:" << width << "x" << height << ")";
    }
    
    QMutexLocker locker(&m_mutex);
    
    // 使用 OpenCV 累积数据
    try {
        // 构造原始数据的 Mat (CV_16U)
        cv::Mat rawMat(height, width, CV_16UC1, const_cast<uint16_t*>(rawData.data()));

        // 深拷贝整幅图像作为样本（避免 rawData 生命周期问题）
        cv::Mat fullSampleImage = rawMat.clone();

        // 从 FULL 样本中提取 ROI（确保 ROI 就是 FULL 的裁剪区域）
        cv::Rect cvRoi(validRoi.x(), validRoi.y(), validRoi.width(), validRoi.height());
        cv::Mat roiMat = fullSampleImage(cvRoi);
        
        // 转换为 CV_64F 以便累加
        cv::Mat floatMat;
        roiMat.convertTo(floatMat, CV_64F);
        
        // 确保累加矩阵尺寸正确 (处理可能的 ROI 变化)
        if (m_sumMat.empty() || m_sumMat.rows != validRoi.height() || m_sumMat.cols != validRoi.width()) {
             m_sumMat = cv::Mat::zeros(validRoi.height(), validRoi.width(), CV_64F);
             m_sqSumMat = cv::Mat::zeros(validRoi.height(), validRoi.width(), CV_64F);
             // 注意：如果 ROI 尺寸在采集中途改变，这会导致之前的累积数据失效或错位。
             // 假设 ROI 在一次分析过程中保持不变。
        }
        
        // 累加
        cv::accumulate(floatMat, m_sumMat);
        cv::accumulateSquare(floatMat, m_sqSumMat);
        
        // 保存样本：FULL + ROI（两者一一对应）
        m_currentFullSampleImages.push_back(fullSampleImage);
        m_currentSampleImages.push_back(roiMat.clone());
        
    } catch (const cv::Exception& e) {
        qWarning() << "OpenCV error in addFrameData:" << e.what();
        return;
    }
    
    m_currentFrameCount++;
    
    // 发送进度更新
    emit progressUpdate(m_currentGainIndex, m_currentFrameCount, m_samplesPerGain);
    
    // 检查是否完成当前增益配置的所有采样
    if (m_currentFrameCount >= m_samplesPerGain) {
        // 计算当前增益配置的结果
        NoiseAnalysisResult result = computeGainResult(m_currentGainIndex);
        emit gainAnalysisComplete(m_currentGainIndex, result);
        
        // 清空当前增益数据，准备下一个增益
        clearCurrentGainData();
        
        // 移到下一个增益配置
        m_currentGainIndex++;
        
        if (m_currentGainIndex >= m_gainCount) {
            // 所有增益配置都处理完成
            m_isAnalyzing.store(false);
            emit allAnalysisComplete();
        } else {
            // 初始化下一个增益的数据存储
            m_sumMat = cv::Mat::zeros(m_roiHeight, m_roiWidth, CV_64F);
            m_sqSumMat = cv::Mat::zeros(m_roiHeight, m_roiWidth, CV_64F);
            m_currentFrameCount = 0;
        }
    }
}

// 优化版本：直接使用 cv::Mat，避免 QVector 中转拷贝（用于本地文件分析）
void NoiseAnalysisWorker::addFrameDataDirect(const cv::Mat &img16, const QRect &roi, int gainIndex)
{
    if (m_shouldStop.load() || !m_isAnalyzing.load()) {
        return;
    }
    
    // 验证增益索引
    if (gainIndex != m_currentGainIndex) {
        qWarning() << "NoiseAnalysisWorker: Received frame for wrong gain index"
                   << "expected:" << m_currentGainIndex << "got:" << gainIndex;
        return;
    }
    
    int width = img16.cols;
    int height = img16.rows;
    
    // 确保ROI有效
    QRect validRoi = roi.intersected(QRect(0, 0, width, height));
    if (validRoi.isEmpty()) {
        qWarning() << "NoiseAnalysisWorker: ROI is outside image bounds";
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    try {
        // 保存完整图像样本
        cv::Mat fullSampleImage = img16.clone();
        
        // 直接从图像中提取 ROI（零拷贝引用）
        cv::Rect cvRoi(validRoi.x(), validRoi.y(), validRoi.width(), validRoi.height());
        cv::Mat roiMat = fullSampleImage(cvRoi);
        
        // 转换为 CV_64F 以便累加
        cv::Mat floatMat;
        roiMat.convertTo(floatMat, CV_64F);
        
        // 确保累加矩阵尺寸正确
        if (m_sumMat.empty() || m_sumMat.rows != validRoi.height() || m_sumMat.cols != validRoi.width()) {
            m_sumMat = cv::Mat::zeros(validRoi.height(), validRoi.width(), CV_64F);
            m_sqSumMat = cv::Mat::zeros(validRoi.height(), validRoi.width(), CV_64F);
        }
        
        // 使用 OpenCV 优化的累加函数
        cv::accumulate(floatMat, m_sumMat);
        cv::accumulateSquare(floatMat, m_sqSumMat);
        
        // 保存样本
        m_currentFullSampleImages.push_back(fullSampleImage);
        m_currentSampleImages.push_back(roiMat.clone());
        
    } catch (const cv::Exception& e) {
        qWarning() << "OpenCV error in addFrameDataDirect:" << e.what();
        return;
    }
    
    m_currentFrameCount++;
    
    // 发送进度更新
    emit progressUpdate(m_currentGainIndex, m_currentFrameCount, m_samplesPerGain);
    
    // 检查是否完成当前增益配置的所有采样
    if (m_currentFrameCount >= m_samplesPerGain) {
        NoiseAnalysisResult result = computeGainResult(m_currentGainIndex);
        emit gainAnalysisComplete(m_currentGainIndex, result);
        
        clearCurrentGainData();
        m_currentGainIndex++;
        
        if (m_currentGainIndex >= m_gainCount) {
            m_isAnalyzing.store(false);
            emit allAnalysisComplete();
        } else {
            m_sumMat = cv::Mat::zeros(m_roiHeight, m_roiWidth, CV_64F);
            m_sqSumMat = cv::Mat::zeros(m_roiHeight, m_roiWidth, CV_64F);
            m_currentFrameCount = 0;
        }
    }
}

void NoiseAnalysisWorker::stop()
{
    m_shouldStop.store(true);
    m_isAnalyzing.store(false);
}

void NoiseAnalysisWorker::cleanup()
{
    QMutexLocker locker(&m_mutex);
    
    // 清空队列和数据
    m_frameQueue.clear();
    clearCurrentGainData();
    
    m_gainCount = 0;
    m_samplesPerGain = 0;
    m_roiWidth = 0;
    m_roiHeight = 0;
    m_currentGainIndex = -1;
    m_currentFrameCount = 0;
    
}

void NoiseAnalysisWorker::processFrameQueue()
{
    // 此函数预留用于从队列处理帧数据
    // 当前实现中，帧数据在 addFrameData 中直接处理以避免队列积压
}

NoiseAnalysisResult NoiseAnalysisWorker::computeGainResult(int gainIndex)
{
    NoiseAnalysisResult result;
    result.width = m_roiWidth;
    result.height = m_roiHeight;
    result.valid = false;
    
    if (m_sumMat.empty() || m_currentFrameCount == 0) {
        qWarning() << "NoiseAnalysisWorker: No data to compute for gain" << gainIndex;
        return result;
    }
    
    int totalPixels = m_roiWidth * m_roiHeight;
    result.stdDevs.resize(totalPixels);
    
    try {
        // 优化：使用就地操作和更高效的计算方法
        double invN = 1.0 / m_currentFrameCount;
        
        // 计算均值: Mean = Sum / N (就地缩放避免额外分配)
        cv::Mat meanMat;
        m_sumMat.convertTo(meanMat, CV_64F, invN);
        
        // 计算方差: Var = (SqSum / N) - Mean^2
        // 优化：使用 cv::multiply 替代 cv::pow(x, 2)，性能更好
        cv::Mat sqMeanMat;
        m_sqSumMat.convertTo(sqMeanMat, CV_64F, invN);
        
        cv::Mat meanSqMat;
        cv::multiply(meanMat, meanMat, meanSqMat);  // 比 cv::pow(meanMat, 2, meanSqMat) 更快
        
        // 优化：就地计算避免临时矩阵 varianceMat = sqMeanMat - meanSqMat
        cv::subtract(sqMeanMat, meanSqMat, sqMeanMat);  // 复用 sqMeanMat 作为 varianceMat
        
        // 计算标准差: StdDev = sqrt(Var)
        cv::Mat stdDevMat;
        cv::sqrt(sqMeanMat, stdDevMat);
        
        // 优化：确保矩阵连续以加速拷贝
        if (!stdDevMat.isContinuous()) {
            stdDevMat = stdDevMat.clone();  // clone 会创建连续存储的副本
        }
        
        // 直接内存拷贝（stdDevMat 现在保证是连续的）
        memcpy(result.stdDevs.data(), stdDevMat.ptr<double>(0), totalPixels * sizeof(double));
        
        result.valid = true;
        
        // 复制采样图像到结果
        result.sampleImages = m_currentSampleImages;
        result.sampleFullImages = m_currentFullSampleImages;
        
                 
    } catch (const cv::Exception& e) {
        qWarning() << "OpenCV error in computeGainResult:" << e.what();
        result.valid = false;
    }
    
    return result;
}

void NoiseAnalysisWorker::clearCurrentGainData()
{
    // 重置累积矩阵
    m_sumMat = cv::Mat();
    m_sqSumMat = cv::Mat();
    
    // 清空采样图像
    m_currentSampleImages.clear();
    m_currentFullSampleImages.clear();
    
}
