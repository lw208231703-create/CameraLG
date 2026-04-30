#ifndef SPOT_DETECTION_WORKER_H
#define SPOT_DETECTION_WORKER_H

#include <QObject>
#include <QVector>
#include <QRect>
#include <opencv2/opencv.hpp>
#include <atomic>

/**
 * @brief 光斑检测结果结构
 */
struct SpotDetectionResult {
    double centerX{0.0};        ///< 光斑中心 X 坐标 (double 精度)
    double centerY{0.0};        ///< 光斑中心 Y 坐标 (double 精度)
    double velocityX{0.0};      ///< X 方向速度
    double velocityY{0.0};      ///< Y 方向速度
    
    // 测量质量评估
    double energy{0.0};         ///< 光斑能量 (像素值总和)
    int validPixelCount{0};     ///< 有效像素数量
    double variance{0.0};       ///< 二阶矩 (方差)
    double measurementR{0.0};   ///< 动态测量噪声 R
    
    // ROI 信息
    QRect roi;                  ///< 当前处理的 ROI 区域
    
    bool isValid{false};        ///< 检测结果是否有效
    qint64 processingTimeUs{0}; ///< 处理耗时 (微秒)
};

/**
 * @brief 光斑检测参数
 */
struct SpotDetectionParams {
    // ROI 参数
    int roiSize{12};            ///< ROI 尺寸 (8x8, 12x12, 16x16)
    
    // 预处理参数
    double thresholdRatio{0.25};///< 阈值比例 (0.2~0.3)
    double saturationRatio{0.95};///< 饱和像素判定比例
    bool useBackgroundRemoval{true}; ///< 是否去背景
    
    // 质心计算参数
    bool useSquareWeights{true};///< 使用平方权重 (I^2)
    
    // Kalman 滤波参数
    double dt{0.033};           ///< 时间间隔 (默认 30Hz)
    double processNoise{1.0};   ///< 过程噪声
    double baseR{1.0};          ///< 基准测量噪声
    
    // 质量评估阈值
    double minEnergy{100.0};    ///< 最小能量阈值
    int minValidPixels{5};      ///< 最小有效像素数
    double maxVariance{100.0};  ///< 最大方差阈值
};

/**
 * @brief 光斑检测工作线程
 * 
 * 实现完整的光斑检测流程：
 * 1. Kalman 预测
 * 2. 预测驱动 ROI 裁剪
 * 3. 光斑预处理（去背景 + 阈值）
 * 4. 加权质心计算
 * 5. 测量质量评估
 * 6. Kalman 更新
 * 7. 输出中心点
 */
class SpotDetectionWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit SpotDetectionWorker(QObject *parent = nullptr);
    ~SpotDetectionWorker();
    
    /**
     * @brief 设置检测参数
     */
    void setParams(const SpotDetectionParams &params);
    
    /**
     * @brief 获取当前参数
     */
    SpotDetectionParams getParams() const { return params_; }
    
public slots:
    /**
     * @brief 处理图像进行光斑检测
     * @param image 输入图像 (8-bit 或 16-bit)
     * @param initialX 初始 X 位置 (仅首次或重置时使用)
     * @param initialY 初始 Y 位置 (仅首次或重置时使用)
     */
    void processImage(cv::Mat image, double initialX = -1, double initialY = -1);
    
    /**
     * @brief 重置 Kalman 滤波器状态
     */
    void reset();
    
    /**
     * @brief 停止工作线程
     */
    void stop();
    
signals:
    /**
     * @brief 检测完成信号
     */
    void detectionFinished(SpotDetectionResult result);
    
    /**
     * @brief 检测错误信号
     */
    void detectionError(QString error);
    
private:
    /**
     * @brief Kalman 预测步骤
     */
    void kalmanPredict();
    
    /**
     * @brief 提取预测驱动的 ROI
     * @param image 输入图像
     * @return ROI 图像
     */
    cv::Mat extractPredictedROI(const cv::Mat &image, QRect &roiRect);
    
    /**
     * @brief 光斑预处理
     * @param roi 输入 ROI
     * @param preprocessed 输出预处理后的图像
     * @return 背景值
     */
    double preprocessSpot(const cv::Mat &roi, cv::Mat &preprocessed);
    
    /**
     * @brief 计算加权质心
     * @param preprocessed 预处理后的图像
     * @param roiRect ROI 区域
     * @param cx 输出中心 X (全局坐标)
     * @param cy 输出中心 Y (全局坐标)
     * @param energy 输出能量
     * @param validPixels 输出有效像素数
     * @param variance 输出方差
     * @return 是否计算成功
     */
    bool calculateWeightedCentroid(const cv::Mat &preprocessed, 
                                   const QRect &roiRect,
                                   double &cx, double &cy,
                                   double &energy, int &validPixels,
                                   double &variance);
    
    /**
     * @brief 评估测量质量
     * @param energy 能量
     * @param validPixels 有效像素数
     * @param variance 方差
     * @return 动态测量噪声 R
     */
    double assessMeasurementQuality(double energy, int validPixels, double variance);
    
    /**
     * @brief Kalman 更新步骤
     * @param measuredX 测量的 X 坐标
     * @param measuredY 测量的 Y 坐标
     * @param R 测量噪声
     */
    void kalmanUpdate(double measuredX, double measuredY, double R);
    
    // 参数
    SpotDetectionParams params_;
    
    // Kalman 状态: [cx, cy, vx, vy]
    cv::Mat state_;           ///< 状态向量 (4x1)
    cv::Mat covariance_;      ///< 协方差矩阵 (4x4)
    cv::Mat processNoiseCov_; ///< 过程噪声协方差矩阵 (4x4)
    
    bool isInitialized_{false}; ///< Kalman 滤波器是否已初始化
    
    // 控制标志
    std::atomic<bool> shouldStop_{false};
    
    // 当前图像尺寸
    int imageWidth_{0};
    int imageHeight_{0};
};

// 注册自定义类型用于 Qt 信号/槽跨线程通信
Q_DECLARE_METATYPE(SpotDetectionResult)
Q_DECLARE_METATYPE(SpotDetectionParams)
//Q_DECLARE_METATYPE(cv::Mat)

#endif // SPOT_DETECTION_WORKER_H
