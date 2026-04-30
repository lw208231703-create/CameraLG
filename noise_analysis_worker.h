#ifndef NOISE_ANALYSIS_WORKER_H
#define NOISE_ANALYSIS_WORKER_H

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QQueue>
#include <atomic>
#include <opencv2/opencv.hpp>

// 单个增益配置的噪声分析结果
struct NoiseAnalysisResult {
    QString gainName;           // 增益名称
    int exposureTime;           // 曝光时间
    QVector<double> stdDevs;    // 每个像素的标准差
    int width;                  // 分析区域宽度
    int height;                 // 分析区域高度
    bool valid;                 // 结果是否有效
    QVector<cv::Mat> sampleImages;  // 采样的图像（保存用于对比数据）
    QVector<cv::Mat> sampleFullImages; // 采样的整幅图像（与 sampleImages 一一对应）
    
    NoiseAnalysisResult() : exposureTime(0), width(0), height(0), valid(false) {}
};

// 待处理的图像帧数据
struct FrameData {
    QVector<uint16_t> pixels;   // 像素数据
    int width;                  // 图像宽度
    int height;                 // 图像高度
    int gainIndex;              // 增益索引
    
    FrameData() : width(0), height(0), gainIndex(-1) {}
    
    // 移动构造函数，避免不必要的数据拷贝
    FrameData(FrameData &&other) noexcept
        : pixels(std::move(other.pixels))
        , width(other.width)
        , height(other.height)
        , gainIndex(other.gainIndex)
    {
        other.width = 0;
        other.height = 0;
        other.gainIndex = -1;
    }
    
    FrameData& operator=(FrameData &&other) noexcept {
        if (this != &other) {
            pixels = std::move(other.pixels);
            width = other.width;
            height = other.height;
            gainIndex = other.gainIndex;
            other.width = 0;
            other.height = 0;
            other.gainIndex = -1;
        }
        return *this;
    }
    
    // 禁用拷贝构造和赋值，强制使用移动语义
    FrameData(const FrameData&) = delete;
    FrameData& operator=(const FrameData&) = delete;
};

Q_DECLARE_METATYPE(NoiseAnalysisResult)

// 噪声分析工作线程
class NoiseAnalysisWorker : public QObject
{
    Q_OBJECT
public:
    explicit NoiseAnalysisWorker(QObject *parent = nullptr);
    ~NoiseAnalysisWorker() override;

public slots:
    // 开始新的分析任务
    void startAnalysis(int gainCount, int samplesPerGain, int roiWidth, int roiHeight);
    
    // 从文件列表开始分析
    void startFileAnalysis(const QStringList &files, const QRect &roi);
    
    // 添加帧数据到处理队列（使用指针避免拷贝）
    void addFrameData(const QVector<uint16_t> &rawData, int width, int height, 
                      const QRect &roi, int gainIndex);
    
    // 直接使用 cv::Mat 添加帧数据（避免 QVector 拷贝，用于本地文件分析）
    void addFrameDataDirect(const cv::Mat &img16, const QRect &roi, int gainIndex);
    
    // 停止分析
    void stop();
    
    // 清理资源
    void cleanup();

signals:
    // 单个增益配置分析完成
    void gainAnalysisComplete(int gainIndex, const NoiseAnalysisResult &result);
    
    // 所有分析完成
    void allAnalysisComplete();
    
    // 处理进度更新（当前增益索引，当前帧数，总帧数）
    void progressUpdate(int gainIndex, int currentFrame, int totalFrames);
    
    // 错误信号
    void analysisError(const QString &error);

private:
    // 处理队列中的帧数据
    void processFrameQueue();
    
    // 计算并返回当前增益配置的结果
    NoiseAnalysisResult computeGainResult(int gainIndex);
    
    // 清空当前增益的帧数据
    void clearCurrentGainData();

    QMutex m_mutex;
    QQueue<FrameData> m_frameQueue;         // 待处理的帧队列
    std::atomic<bool> m_shouldStop{false};
    std::atomic<bool> m_isAnalyzing{false};
    
    // 分析参数
    int m_gainCount{0};
    int m_samplesPerGain{0};
    int m_roiWidth{0};
    int m_roiHeight{0};
    
    // 当前增益的累积数据 (使用 OpenCV 矩阵替代巨大的 QVector<QVector<double>>)
    // m_sumMat: 存储像素值的累加和 (CV_64F)
    // m_sqSumMat: 存储像素值平方的累加和 (CV_64F)
    cv::Mat m_sumMat;
    cv::Mat m_sqSumMat;
    
    // 当前增益索引和已收集的帧数
    int m_currentGainIndex{-1};
    int m_currentFrameCount{0};
    
    // 当前增益的采样图像存储
    QVector<cv::Mat> m_currentSampleImages;
    QVector<cv::Mat> m_currentFullSampleImages;
};

#endif // NOISE_ANALYSIS_WORKER_H
