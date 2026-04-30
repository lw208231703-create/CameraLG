#ifndef HISTOGRAM_EQUALIZATION_WORKER_H
#define HISTOGRAM_EQUALIZATION_WORKER_H

#include <QObject>
#include <QImage>
#include <QVector>
#include <QMutex>
#include <atomic>
#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>

/**
 * @brief 直方图均衡化处理参数
 */
struct HistogramEqualizationParams {
    bool enabled{false};           // 是否启用直方图均衡化
    bool adaptiveMode{false};      // 是否使用自适应直方图均衡化(CLAHE)
    double clipLimit{2.0};         // CLAHE 的对比度限制参数 (典型值: 2.0-4.0)
    int tileGridSize{8};           // CLAHE 的网格大小 (典型值: 8x8 或 16x16)
    
    // 输入图像格式信息
    int bitDepth{14};              // 输入图像位深
    
    HistogramEqualizationParams() = default;
};

/**
 * @brief 直方图均衡化工作线程
 * 
 * 从原始采集图像队列中读取数据，应用直方图均衡化处理，
 * 在单独的窗口中实时显示均衡化后的结果
 */
class HistogramEqualizationWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit HistogramEqualizationWorker(QObject *parent = nullptr);
    ~HistogramEqualizationWorker();
    
public slots:
    /**
     * @brief 处理新的原始图像数据 (16位)
     * @param rawData 原始16位图像数据
     * @param width 图像宽度
     * @param height 图像高度
     * @param bitDepth 图像位深
     */
    void processRawImage(const QVector<uint16_t> &rawData, int width, int height, int bitDepth);
    
    /**
     * @brief 处理新的8位图像数据
     * @param image 8位QImage图像
     */
    void processImage(const QImage &image);
    
    /**
     * @brief 更新处理参数
     * @param params 新的参数设置
     */
    void updateParameters(const HistogramEqualizationParams &params);
    
    /**
     * @brief 停止处理
     */
    void stop();
    
    /**
     * @brief 启用/禁用处理
     * @param enabled 是否启用
     */
    void setEnabled(bool enabled);
    
signals:
    /**
     * @brief 均衡化处理完成信号
     * @param equalizedImage 均衡化后的8位图像
     * @param originalImage 原始图像 (可选，用于对比显示)
     */
    void imageProcessed(const QImage &equalizedImage, const QImage &originalImage);
    
    /**
     * @brief 处理统计信息
     * @param processingTime 处理耗时(ms)
     * @param fps 处理帧率
     */
    void statisticsUpdated(double processingTime, double fps);
    
    /**
     * @brief 错误信号
     * @param error 错误信息
     */
    void errorOccurred(const QString &error);
    
private:
    /**
     * @brief 执行标准直方图均衡化（使用UMat支持OpenCL加速）
     */
    cv::UMat performHistogramEqualization(const cv::UMat &input);
    
    /**
     * @brief 执行自适应直方图均衡化(CLAHE)（使用UMat支持OpenCL加速）
     */
    cv::UMat performCLAHE(const cv::UMat &input);
    
    /**
     * @brief 将16位数据转换为8位(用于显示对比)
     */
    QImage convertRawTo8Bit(const QVector<uint16_t> &rawData, int width, int height, int bitDepth);
    
    HistogramEqualizationParams m_params;
    std::atomic<bool> m_shouldStop{false};
    std::atomic<bool> m_isProcessing{false};
    QMutex m_paramsMutex;
    
    // 性能统计
    qint64 m_lastProcessTime{0};
    int m_frameCount{0};
    qint64 m_lastStatisticsTime{0};
};

#endif // HISTOGRAM_EQUALIZATION_WORKER_H
