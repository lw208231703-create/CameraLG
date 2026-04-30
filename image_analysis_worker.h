#ifndef IMAGE_ANALYSIS_WORKER_H
#define IMAGE_ANALYSIS_WORKER_H

#include <QObject>
#include <QImage>
#include <QVector>
#include <QMutex>
#include <atomic>

#include "build_config.h"



// 图像统计数据结构
struct ImageStatistics
{
    double minValue{0.0};
    double maxValue{0.0};
    double range{0.0};        // 极差
    double mean{0.0};         // 平均值
    double stdDev{0.0};       // 标准差
    double variance{0.0};     // 方差
    QVector<int> histogram;   // 直方图数据 (256 或 65536 bins)
    bool valid{false};
    int bitDepth{8};          // 数据位深 (8 或 16)
    int historySize{0};       // 目标历史长度
    int currentSampleCount{0};// 当前样本数量
};

// 图像分析工作线程类
class ImageAnalysisWorker : public QObject
{
    Q_OBJECT
public:
    explicit ImageAnalysisWorker(QObject *parent = nullptr);
    ~ImageAnalysisWorker() override;

    // 检查是否正在分析
    bool isAnalyzing() const { return m_isAnalyzing.load(); }

public slots:
    // 分析图像 - 在工作线程中执行
    void analyzeImage(const QImage &image, const QRect &roi = QRect());
    
    // 分析原始数据
    void analyzeRawData(const QVector<uint16_t> &data, int width, int height, int bitDepth, const QRect &roi = QRect());
    
    // 分析单像素历史数据
    void analyzeSinglePixel(const QImage &image, const QPoint &point);
    void analyzeSinglePixelRaw(const QVector<uint16_t> &data, int width, int height, int bitDepth, const QPoint &point);
    
    // 重置单像素历史
    void resetSinglePixelHistory();

    // 设置历史记录长度
    void setHistorySize(int size);

    // 停止分析
    void stop();

signals:
    // 分析完成信号
    void analysisComplete(const ImageStatistics &stats);
    
    // 错误信号
    void analysisError(const QString &error);

private:
    // 计算图像统计数据
    ImageStatistics computeStatistics(const QImage &image, const QRect &roi = QRect());
    ImageStatistics computeStatisticsRaw(const QVector<uint16_t> &data, int width, int height, int bitDepth, const QRect &roi = QRect());
    
    std::atomic<bool> m_isAnalyzing{false};
    std::atomic<bool> m_shouldStop{false};
    QMutex m_mutex;

    // 单像素历史数据
    QVector<double> m_pixelHistory;
    int m_historySize{2};

    double m_pixelMin{0.0};
    double m_pixelMax{0.0};
    double m_pixelSum{0.0};
    double m_pixelSumSq{0.0};
    bool m_pixelHistoryValid{false};
};


// 注册自定义类型以便跨线程传递
Q_DECLARE_METATYPE(ImageStatistics)

#endif // IMAGE_ANALYSIS_WORKER_H
