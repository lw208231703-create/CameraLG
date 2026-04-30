#include "image_depth_converter.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <opencv2/opencv.hpp>

// Sapera SDK includes for LUT operations (optional)
#ifdef ENABLE_SAPERA_CAMERA
#include <SapClassBasic.h>

/**
 * @brief 根据位深选择合适的Sapera格式
 * @param srcBitDepth 源图像位深
 * @return SapFormat 对应的Sapera格式
 */
static SapFormat selectSapFormatForBitDepth(int srcBitDepth)
{
    if (srcBitDepth <= 8) return SapFormatUint8;
    else if (srcBitDepth == 10) return SapFormatUint10;
    else if (srcBitDepth == 12) return SapFormatUint12;
    else if (srcBitDepth == 14) return SapFormatUint14;
    else return SapFormatUint16;
}

/**
 * @brief RAII 包装器，确保 SapLut 资源自动清理
 */
class SapLutGuard {
public:
    SapLutGuard(SapLut& lut) : m_lut(lut), m_created(false) {}
    
    bool create() {
        m_created = m_lut.Create();
        return m_created;
    }
    
    ~SapLutGuard() {
        if (m_created) {
            m_lut.Destroy();
        }
    }
    
    // 禁止拷贝
    SapLutGuard(const SapLutGuard&) = delete;
    SapLutGuard& operator=(const SapLutGuard&) = delete;
    
private:
    SapLut& m_lut;
    bool m_created;
};
#endif

QImage ImageDepthConverter::convert(const uint16_t *srcData, int width, int height,
                                    int srcPitch, int srcBitDepth)
{
    // 根据编译时宏定义选择转换方法
#if defined(DEPTH_CONVERSION_METHOD_BIT_EXTRACT)
    return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 
                      DEPTH_CONV_BIT_EXTRACT_VARIANT);

#elif defined(DEPTH_CONVERSION_METHOD_LINEAR)
    return linearScaling(srcData, width, height, srcPitch, srcBitDepth);

#elif defined(DEPTH_CONVERSION_METHOD_HISTOGRAM_EQ)
    return histogramEqualization(srcData, width, height, srcPitch, srcBitDepth);

#elif defined(DEPTH_CONVERSION_METHOD_GAMMA)
    return gammaCorrection(srcData, width, height, srcPitch, srcBitDepth, 
                           DEPTH_CONV_GAMMA_VALUE);

#elif defined(DEPTH_CONVERSION_METHOD_THRESHOLD)
    return thresholdTruncation(srcData, width, height, srcPitch, srcBitDepth,
                               DEPTH_CONV_THRESHOLD_LOW_PERCENT,
                               DEPTH_CONV_THRESHOLD_HIGH_PERCENT);

#elif defined(DEPTH_CONVERSION_METHOD_LOG)
    return logarithmicCompression(srcData, width, height, srcPitch, srcBitDepth);

#elif defined(DEPTH_CONVERSION_METHOD_EXP)
    return exponentialCompression(srcData, width, height, srcPitch, srcBitDepth);

#elif defined(DEPTH_CONVERSION_METHOD_QUANTIZATION)
    return quantization(srcData, width, height, srcPitch, srcBitDepth,
                        DEPTH_CONV_QUANTIZATION_LEVELS);

#else
    // 默认使用线性缩放法
    return linearScaling(srcData, width, height, srcPitch, srcBitDepth);
#endif
}

QImage ImageDepthConverter::convertWithMethod(DepthConversionMethod method,
                                              const uint16_t *srcData, int width, int height,
                                              int srcPitch, int srcBitDepth)
{
    switch (method) {
    case DepthConversionMethod::BitExtract_0_7:
        return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 0);
    case DepthConversionMethod::BitExtract_1_8:
        return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 1);
    case DepthConversionMethod::BitExtract_2_9:
        return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 2);
    case DepthConversionMethod::BitExtract_3_10:
        return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 3);
    case DepthConversionMethod::BitExtract_4_11:
        return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 4);
    case DepthConversionMethod::BitExtract_5_12:
        return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 5);
    case DepthConversionMethod::BitExtract_6_13:
        return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 6);
    case DepthConversionMethod::BitExtract_7_14:
        return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 7);
    case DepthConversionMethod::BitExtract_8_15:
        return bitExtract(srcData, width, height, srcPitch, srcBitDepth, 8);
    case DepthConversionMethod::LinearScaling:
        return linearScaling(srcData, width, height, srcPitch, srcBitDepth);
    case DepthConversionMethod::HistogramEqualization:
        return histogramEqualization(srcData, width, height, srcPitch, srcBitDepth);
    case DepthConversionMethod::GammaCorrection:
        return gammaCorrection(srcData, width, height, srcPitch, srcBitDepth, DEPTH_CONV_GAMMA_VALUE);
    case DepthConversionMethod::ThresholdTruncation:
        return thresholdTruncation(srcData, width, height, srcPitch, srcBitDepth,
                                   DEPTH_CONV_THRESHOLD_LOW_PERCENT, DEPTH_CONV_THRESHOLD_HIGH_PERCENT);
    case DepthConversionMethod::LogarithmicCompression:
        return logarithmicCompression(srcData, width, height, srcPitch, srcBitDepth);
    case DepthConversionMethod::ExponentialCompression:
        return exponentialCompression(srcData, width, height, srcPitch, srcBitDepth);
    case DepthConversionMethod::Quantization:
        return quantization(srcData, width, height, srcPitch, srcBitDepth, DEPTH_CONV_QUANTIZATION_LEVELS);
    default:
        return linearScaling(srcData, width, height, srcPitch, srcBitDepth);
    }
}

DepthConversionMethod ImageDepthConverter::getCurrentMethod()
{
#if defined(DEPTH_CONVERSION_METHOD_BIT_EXTRACT)
    switch (DEPTH_CONV_BIT_EXTRACT_VARIANT) {
    case 0: return DepthConversionMethod::BitExtract_0_7;
    case 1: return DepthConversionMethod::BitExtract_1_8;
    case 2: return DepthConversionMethod::BitExtract_2_9;
    case 3: return DepthConversionMethod::BitExtract_3_10;
    case 4: return DepthConversionMethod::BitExtract_4_11;
    case 5: return DepthConversionMethod::BitExtract_5_12;
    case 6: return DepthConversionMethod::BitExtract_6_13;
    case 7: return DepthConversionMethod::BitExtract_7_14;
    case 8: return DepthConversionMethod::BitExtract_8_15;
    default: return DepthConversionMethod::BitExtract_8_15;
    }
#elif defined(DEPTH_CONVERSION_METHOD_LINEAR)
    return DepthConversionMethod::LinearScaling;
#elif defined(DEPTH_CONVERSION_METHOD_HISTOGRAM_EQ)
    return DepthConversionMethod::HistogramEqualization;
#elif defined(DEPTH_CONVERSION_METHOD_GAMMA)
    return DepthConversionMethod::GammaCorrection;
#elif defined(DEPTH_CONVERSION_METHOD_THRESHOLD)
    return DepthConversionMethod::ThresholdTruncation;
#elif defined(DEPTH_CONVERSION_METHOD_LOG)
    return DepthConversionMethod::LogarithmicCompression;
#elif defined(DEPTH_CONVERSION_METHOD_EXP)
    return DepthConversionMethod::ExponentialCompression;
#elif defined(DEPTH_CONVERSION_METHOD_QUANTIZATION)
    return DepthConversionMethod::Quantization;
#else
    return DepthConversionMethod::LinearScaling;
#endif
}

QString ImageDepthConverter::getMethodName(DepthConversionMethod method)
{
    switch (method) {
    case DepthConversionMethod::BitExtract_0_7:
        return QStringLiteral("位段提取法 (0-7位)");
    case DepthConversionMethod::BitExtract_1_8:
        return QStringLiteral("位段提取法 (1-8位)");
    case DepthConversionMethod::BitExtract_2_9:
        return QStringLiteral("位段提取法 (2-9位)");
    case DepthConversionMethod::BitExtract_3_10:
        return QStringLiteral("位段提取法 (3-10位)");
    case DepthConversionMethod::BitExtract_4_11:
        return QStringLiteral("位段提取法 (4-11位)");
    case DepthConversionMethod::BitExtract_5_12:
        return QStringLiteral("位段提取法 (5-12位)");
    case DepthConversionMethod::BitExtract_6_13:
        return QStringLiteral("位段提取法 (6-13位)");
    case DepthConversionMethod::BitExtract_7_14:
        return QStringLiteral("位段提取法 (7-14位)");
    case DepthConversionMethod::BitExtract_8_15:
        return QStringLiteral("位段提取法 (8-15位)");
    case DepthConversionMethod::LinearScaling:
        return QStringLiteral("线性缩放法 (归一化)");
    case DepthConversionMethod::HistogramEqualization:
        return QStringLiteral("直方图均衡化法");
    case DepthConversionMethod::GammaCorrection:
        return QStringLiteral("伽马校正法");
    case DepthConversionMethod::ThresholdTruncation:
        return QStringLiteral("阈值截断/裁剪法");
    case DepthConversionMethod::LogarithmicCompression:
        return QStringLiteral("对数压缩法");
    case DepthConversionMethod::ExponentialCompression:
        return QStringLiteral("指数压缩法");
    case DepthConversionMethod::Quantization:
        return QStringLiteral("量化法");
    default:
        return QStringLiteral("未知方法");
    }
}

//==============================================================================
// 位段提取法实现
//==============================================================================
QImage ImageDepthConverter::bitExtract(const uint16_t *srcData, int width, int height,
                                       int srcPitch, int srcBitDepth, int startBit)
{
    QImage result(width, height, QImage::Format_Grayscale8);

    if (!srcData || width <= 0 || height <= 0 || srcPitch <= 0) {
        return QImage();
    }

    // 计算移位量：从startBit位开始取8位
    // startBit=0: 取最低8位 (>> 0)
    // startBit=8: 取最高8位 (>> 8)
    const int shift = startBit;

    // 允许最大移位到 8 (对应 8-15 位)，不限制于 srcBitDepth
    // 这样用户可以查看高位，即使 srcBitDepth 报告较小（例如14位时查看16位范围）
    // 之前的逻辑是 const int maxShift = srcBitDepth > 8 ? srcBitDepth - 8 : 0;
    // 这会导致在14位图像上无法查看第7、8种位移模式（被强制限制在6）
    const int maxShift = 8;
    const int actualShift = std::min(shift, maxShift);

    // 用 OpenCV 的 Mat 视图 + parallel_for_ 做并行遍历，减少手写循环维护成本
    cv::Mat srcMat(height, width, CV_16UC1, const_cast<uint16_t*>(srcData), srcPitch);
    cv::Mat dstMat(height, width, CV_8UC1, result.bits(), result.bytesPerLine());

    cv::parallel_for_(cv::Range(0, height), [&](const cv::Range& range) {
        for (int y = range.start; y < range.end; ++y) {
            const uint16_t* srcRow = srcMat.ptr<uint16_t>(y);
            uchar* dstRow = dstMat.ptr<uchar>(y);
            for (int x = 0; x < width; ++x) {
                dstRow[x] = static_cast<uchar>((srcRow[x] >> actualShift) & 0xFF);
            }
        }
    });

    Q_UNUSED(srcBitDepth);
    return result;
}

//==============================================================================
// 线性缩放法实现 (归一化)
//==============================================================================
QImage ImageDepthConverter::linearScaling(const uint16_t *srcData, int width, int height,
                                          int srcPitch, int srcBitDepth)
{
    QImage result(width, height, QImage::Format_Grayscale8);
    
    // 使用 OpenCV 进行加速
    // 构造 cv::Mat (共享内存)
    cv::Mat srcMat(height, width, CV_16UC1, const_cast<uint16_t*>(srcData), srcPitch);
    cv::Mat dstMat(height, width, CV_8UC1, result.bits(), result.bytesPerLine());
    
    // 计算最大值（钳制到 1..16，避免位移溢出/UB）
    const int clampedBitDepth = std::max(1, std::min(srcBitDepth, 16));
    const double maxVal = (1u << clampedBitDepth) - 1;
    
    // 线性映射: output = input * 255 / maxVal
    // convertTo 会自动处理饱和度 (saturate_cast)，但这里我们是缩放，不会溢出
    srcMat.convertTo(dstMat, CV_8U, 255.0 / maxVal);
    
    return result;
}

//==============================================================================
// 直方图均衡化法实现
//==============================================================================
QImage ImageDepthConverter::histogramEqualization(const uint16_t *srcData, int width, int height,
                                                  int srcPitch, int srcBitDepth)
{
    QImage result(width, height, QImage::Format_Grayscale8);
    
    const int histSize = 1 << srcBitDepth;  // 直方图大小
    
    // 使用 OpenCV 计算直方图
    cv::Mat srcMat(height, width, CV_16UC1, const_cast<uint16_t*>(srcData), srcPitch);
    
    int channels[] = {0};
    int histSizeArr[] = {histSize};
    float range[] = {0, (float)histSize};
    const float* ranges[] = {range};
    cv::Mat hist;
    
    cv::calcHist(&srcMat, 1, channels, cv::Mat(), hist, 1, histSizeArr, ranges);
    
    // 2. 计算累积分布函数 (CDF)
    // OpenCV 的 hist 是 float 类型
    std::vector<int> cdf(histSize);
    cdf[0] = cvRound(hist.at<float>(0));
    for (int i = 1; i < histSize; ++i) {
        cdf[i] = cdf[i - 1] + cvRound(hist.at<float>(i));
    }
    
    // 3. 找到CDF的最小非零值
    int cdfMin = 0;
    for (int i = 0; i < histSize; ++i) {
        if (cdf[i] > 0) {
            cdfMin = cdf[i];
            break;
        }
    }
    
    // 4. 创建查找表
    std::vector<uchar> lut(histSize);
    int totalPixels = width * height;
    const int denom = totalPixels - cdfMin;
    
    if (denom > 0) {
        for (int i = 0; i < histSize; ++i) {
            lut[i] = static_cast<uchar>(
                std::round(static_cast<double>(cdf[i] - cdfMin) / denom * 255.0));
        }
    } else {
        std::fill(lut.begin(), lut.end(), 128);
    }
    
    // 5. 应用查找表 (cv::LUT 不支持 16位输入)
    // 使用 OpenCV parallel_for_ 做并行遍历，减少 UI 连续采集场景下的 CPU 峰值
    cv::Mat dstMat(height, width, CV_8UC1, result.bits(), result.bytesPerLine());

    cv::parallel_for_(cv::Range(0, height), [&](const cv::Range& range) {
        for (int y = range.start; y < range.end; ++y) {
            const uint16_t* srcRow = srcMat.ptr<uint16_t>(y);
            uchar* dstRow = dstMat.ptr<uchar>(y);
            for (int x = 0; x < width; ++x) {
                dstRow[x] = lut[srcRow[x]];
            }
        }
    });
    
    return result;
}

//==============================================================================
// 伽马校正法实现 - 使用 Sapera SapLut::Gamma 生成查找表
//==============================================================================
QImage ImageDepthConverter::gammaCorrection(const uint16_t *srcData, int width, int height,
                                            int srcPitch, int srcBitDepth, double gamma)
{
    QImage result(width, height, QImage::Format_Grayscale8);
    
    // 使用 Sapera SapLut 生成伽马校正查找表
    std::vector<uchar> lut = generateGammaLutViaSapera(srcBitDepth, static_cast<float>(gamma));
    
    // 应用查找表 (并行遍历)
    cv::Mat srcMat(height, width, CV_16UC1, const_cast<uint16_t*>(srcData), srcPitch);
    cv::Mat dstMat(height, width, CV_8UC1, result.bits(), result.bytesPerLine());

    cv::parallel_for_(cv::Range(0, height), [&](const cv::Range& range) {
        for (int y = range.start; y < range.end; ++y) {
            const uint16_t* srcRow = srcMat.ptr<uint16_t>(y);
            uchar* dstRow = dstMat.ptr<uchar>(y);
            for (int x = 0; x < width; ++x) {
                dstRow[x] = lut[srcRow[x]];
            }
        }
    });
    
    return result;
}

//==============================================================================
// 阈值截断/裁剪法实现
//==============================================================================
QImage ImageDepthConverter::thresholdTruncation(const uint16_t *srcData, int width, int height,
                                                int srcPitch, int srcBitDepth,
                                                double lowPercent, double highPercent)
{
    QImage result(width, height, QImage::Format_Grayscale8);
    
    // 使用 OpenCV 进行加速
    cv::Mat srcMat(height, width, CV_16UC1, const_cast<uint16_t*>(srcData), srcPitch);
    cv::Mat dstMat(height, width, CV_8UC1, result.bits(), result.bytesPerLine());
    
    // 1. 找到实际数据的最小值和最大值
    double minVal, maxVal;
    cv::minMaxLoc(srcMat, &minVal, &maxVal);
    
    // 2. 计算阈值
    double range = maxVal - minVal;
    double lowThreshold = minVal + range * lowPercent;
    double highThreshold = minVal + range * highPercent;
    
    if (highThreshold <= lowThreshold) {
        highThreshold = lowThreshold + 1.0;
    }
    
    // 3. 应用阈值截断并线性映射
    // 公式: dst = (src - low) * 255 / (high - low)
    // convertTo: dst = src * alpha + beta
    // alpha = 255 / (high - low)
    // beta = -low * alpha
    
    double alpha = 255.0 / (highThreshold - lowThreshold);
    double beta = -lowThreshold * alpha;
    
    // convertTo 会自动处理饱和度 (saturate_cast)
    // 小于 lowThreshold 的值会变成负数，被截断为 0
    // 大于 highThreshold 的值会超过 255，被截断为 255
    srcMat.convertTo(dstMat, CV_8U, alpha, beta);
    
    return result;
}

//==============================================================================
// 对数压缩法实现
//==============================================================================
QImage ImageDepthConverter::logarithmicCompression(const uint16_t *srcData, int width, int height,
                                                   int srcPitch, int srcBitDepth)
{
    QImage result(width, height, QImage::Format_Grayscale8);
    
    // 预计算查找表
    const int lutSize = 1 << srcBitDepth;
    std::vector<uchar> lut(lutSize);
    
    // 对数压缩公式: output = 255 * log(1 + input) / log(1 + maxVal)
    const double logMax = std::log(static_cast<double>(lutSize));
    
    for (int i = 0; i < lutSize; ++i) {
        double logVal = std::log(1.0 + static_cast<double>(i));
        lut[i] = static_cast<uchar>(std::min(255.0, std::round(logVal / logMax * 255.0)));
    }
    
    // 应用查找表
    for (int y = 0; y < height; ++y) {
        uchar *destRow = result.scanLine(y);
        const uint16_t *srcRow = reinterpret_cast<const uint16_t*>(
            reinterpret_cast<const char*>(srcData) + y * srcPitch);
        
        for (int x = 0; x < width; ++x) {
            destRow[x] = lut[srcRow[x]];
        }
    }
    
    return result;
}

//==============================================================================
// 指数压缩法实现
//==============================================================================
QImage ImageDepthConverter::exponentialCompression(const uint16_t *srcData, int width, int height,
                                                   int srcPitch, int srcBitDepth)
{
    QImage result(width, height, QImage::Format_Grayscale8);
    
    // 预计算查找表
    const int lutSize = 1 << srcBitDepth;
    std::vector<uchar> lut(lutSize);
    
    // 指数压缩公式: output = 255 * (exp(input/maxVal) - 1) / (e - 1)
    const double maxVal = static_cast<double>(lutSize - 1);
    const double eMinus1 = std::exp(1.0) - 1.0;
    
    for (int i = 0; i < lutSize; ++i) {
        double normalized = static_cast<double>(i) / maxVal;
        double expVal = (std::exp(normalized) - 1.0) / eMinus1;
        lut[i] = static_cast<uchar>(std::min(255.0, std::round(expVal * 255.0)));
    }
    
    // 应用查找表
    for (int y = 0; y < height; ++y) {
        uchar *destRow = result.scanLine(y);
        const uint16_t *srcRow = reinterpret_cast<const uint16_t*>(
            reinterpret_cast<const char*>(srcData) + y * srcPitch);
        
        for (int x = 0; x < width; ++x) {
            destRow[x] = lut[srcRow[x]];
        }
    }
    
    return result;
}

//==============================================================================
// 量化法实现
//==============================================================================
QImage ImageDepthConverter::quantization(const uint16_t *srcData, int width, int height,
                                         int srcPitch, int srcBitDepth, int levels)
{
    QImage result(width, height, QImage::Format_Grayscale8);
    
    // 确保量化级数在有效范围内
    levels = std::max(2, std::min(256, levels));
    
    // 预计算查找表
    const int lutSize = 1 << srcBitDepth;
    std::vector<uchar> lut(lutSize);
    
    // 计算每个量化级别的范围
    const double srcMax = static_cast<double>(lutSize - 1);
    const double levelStep = srcMax / levels;
    const double outputStep = 255.0 / (levels - 1);
    
    for (int i = 0; i < lutSize; ++i) {
        // 确定当前值属于哪个量化级别
        int level = static_cast<int>(static_cast<double>(i) / levelStep);
        level = std::min(level, levels - 1);
        // 映射到8位输出
        lut[i] = static_cast<uchar>(std::round(level * outputStep));
    }
    
    // 应用查找表
    for (int y = 0; y < height; ++y) {
        uchar *destRow = result.scanLine(y);
        const uint16_t *srcRow = reinterpret_cast<const uint16_t*>(
            reinterpret_cast<const char*>(srcData) + y * srcPitch);
        
        for (int x = 0; x < width; ++x) {
            destRow[x] = lut[srcRow[x]];
        }
    }
    
    return result;
}

//==============================================================================
// Sapera SapLut 辅助函数实现
//==============================================================================

std::vector<uchar> ImageDepthConverter::generateGammaLutViaSapera(int srcBitDepth, float gamma)
{
    const int lutSize = 1 << srcBitDepth;
    std::vector<uchar> result(lutSize);
    
#ifdef ENABLE_SAPERA_CAMERA
    // 使用 Sapera SapLut 生成伽马校正查找表
    SapFormat format = selectSapFormatForBitDepth(srcBitDepth);
    
    SapLut sapLut(lutSize, format);
    SapLutGuard guard(sapLut);  // RAII 确保资源自动清理
    
    if (guard.create()) {
        // 先生成归一化LUT
        sapLut.Normal();
        // 应用伽马校正
        sapLut.Gamma(gamma);
        
        // 读取LUT数据并缩放到8位
        // 注意：SapData::GetData().mono 对于单色格式始终有效
        const double maxVal = static_cast<double>(lutSize - 1);
        SapData data;  // 在循环外声明以避免重复构造/析构
        for (int i = 0; i < lutSize; ++i) {
            if (sapLut.Read(i, &data)) {
                // SapLut 输出与输入相同位深，需要缩放到8位
                int value = data.GetData().mono;
                result[i] = static_cast<uchar>(std::min(255.0, std::round(value * 255.0 / maxVal)));
            } else {
                // 回退到手动计算
                double normalized = static_cast<double>(i) / maxVal;
                double corrected = std::pow(normalized, 1.0 / gamma);
                result[i] = static_cast<uchar>(std::min(255.0, std::round(corrected * 255.0)));
            }
        }
    } else {
        // SapLut 创建失败，回退到手动计算
        const double maxVal = static_cast<double>(lutSize - 1);
        const double invGamma = 1.0 / gamma;
        for (int i = 0; i < lutSize; ++i) {
            double normalized = static_cast<double>(i) / maxVal;
            double corrected = std::pow(normalized, invGamma);
            result[i] = static_cast<uchar>(std::min(255.0, std::round(corrected * 255.0)));
        }
    }
#else
    // 非 Windows 平台使用手动计算
    const double maxVal = static_cast<double>(lutSize - 1);
    const double invGamma = 1.0 / gamma;
    for (int i = 0; i < lutSize; ++i) {
        double normalized = static_cast<double>(i) / maxVal;
        double corrected = std::pow(normalized, invGamma);
        result[i] = static_cast<uchar>(std::min(255.0, std::round(corrected * 255.0)));
    }
#endif
    
    return result;
}

std::vector<uchar> ImageDepthConverter::generateShiftLutViaSapera(int srcBitDepth, int shiftBits)
{
    const int lutSize = 1 << srcBitDepth;
    std::vector<uchar> result(lutSize);
    
    // 确保位移值在有效范围内
    // 对于高位深数据（如14位），有效的右移范围是0到(srcBitDepth-8)
    // 例如：14位图像最大右移6位可提取有效的8位数据
    // 如果位深<=8位，则不需要移位
    const int maxAllowedShift = srcBitDepth > 8 ? srcBitDepth - 8 : 0;
    const int safeShift = std::max(0, std::min(shiftBits, maxAllowedShift));
    
#ifdef ENABLE_SAPERA_CAMERA
    // 使用 Sapera SapLut 生成位移查找表
    SapFormat format = selectSapFormatForBitDepth(srcBitDepth);
    
    SapLut sapLut(lutSize, format);
    SapLutGuard guard(sapLut);  // RAII 确保资源自动清理
    
    if (guard.create()) {
        // 先生成归一化LUT
        sapLut.Normal();
        // 应用位移操作（正值右移，负值左移）
        sapLut.Shift(safeShift);
        
        // 读取LUT数据
        // 注意：SapData::GetData().mono 对于单色格式始终有效
        SapData data;  // 在循环外声明以避免重复构造/析构
        for (int i = 0; i < lutSize; ++i) {
            if (sapLut.Read(i, &data)) {
                int value = data.GetData().mono;
                // 位移后取低8位
                result[i] = static_cast<uchar>(value & 0xFF);
            } else {
                // 回退到手动计算
                result[i] = static_cast<uchar>((i >> safeShift) & 0xFF);
            }
        }
    } else {
        // SapLut 创建失败，回退到手动计算
        for (int i = 0; i < lutSize; ++i) {
            result[i] = static_cast<uchar>((i >> safeShift) & 0xFF);
        }
    }
#else
    // 非 Windows 平台使用手动计算
    for (int i = 0; i < lutSize; ++i) {
        result[i] = static_cast<uchar>((i >> safeShift) & 0xFF);
    }
#endif
    
    return result;
}

std::vector<uchar> ImageDepthConverter::generateNormalLutViaSapera(int srcBitDepth)
{
    const int lutSize = 1 << srcBitDepth;
    std::vector<uchar> result(lutSize);
    
#ifdef ENABLE_SAPERA_CAMERA
    // 使用 Sapera SapLut 生成归一化查找表
    SapFormat format = selectSapFormatForBitDepth(srcBitDepth);
    
    SapLut sapLut(lutSize, format);
    SapLutGuard guard(sapLut);  // RAII 确保资源自动清理
    
    if (guard.create()) {
        // 生成归一化LUT（从0到maxVal的线性映射）
        sapLut.Normal();
        
        // 读取LUT数据并缩放到8位
        // 注意：SapData::GetData().mono 对于单色格式始终有效
        const double maxVal = static_cast<double>(lutSize - 1);
        SapData data;  // 在循环外声明以避免重复构造/析构
        for (int i = 0; i < lutSize; ++i) {
            if (sapLut.Read(i, &data)) {
                int value = data.GetData().mono;
                result[i] = static_cast<uchar>(std::min(255.0, std::round(value * 255.0 / maxVal)));
            } else {
                // 回退到手动计算
                result[i] = static_cast<uchar>(std::min(255.0, std::round(i * 255.0 / maxVal)));
            }
        }
    } else {
        // SapLut 创建失败，回退到手动计算
        const double maxVal = static_cast<double>(lutSize - 1);
        for (int i = 0; i < lutSize; ++i) {
            result[i] = static_cast<uchar>(std::min(255.0, std::round(i * 255.0 / maxVal)));
        }
    }
#else
    // 非 Windows 平台使用手动计算
    const double maxVal = static_cast<double>(lutSize - 1);
    for (int i = 0; i < lutSize; ++i) {
        result[i] = static_cast<uchar>(std::min(255.0, std::round(i * 255.0 / maxVal)));
    }
#endif
    
    return result;
}

std::vector<uchar> ImageDepthConverter::generateThresholdLutViaSapera(int srcBitDepth, int lowThreshold, int highThreshold)
{
    const int lutSize = 1 << srcBitDepth;
    std::vector<uchar> result(lutSize);
    
    // 防止除零：确保阈值范围有效
    if (highThreshold <= lowThreshold) {
        highThreshold = lowThreshold + 1;
    }
    
    // 确保阈值在有效范围内（0 到 lutSize-1）
    lowThreshold = std::max(0, std::min(lowThreshold, lutSize - 1));
    highThreshold = std::max(lowThreshold + 1, std::min(highThreshold, lutSize - 1));
    
    const double range = static_cast<double>(highThreshold - lowThreshold);
    
#ifdef ENABLE_SAPERA_CAMERA
    // 使用 Sapera SapLut 生成阈值查找表
    SapFormat format = selectSapFormatForBitDepth(srcBitDepth);
    
    SapLut sapLut(lutSize, format);
    SapLutGuard guard(sapLut);  // RAII 确保资源自动清理
    
    if (guard.create()) {
        // 先生成归一化LUT
        sapLut.Normal();
        // 应用双阈值操作
        SapDataMono lowData(lowThreshold);
        SapDataMono highData(highThreshold);
        sapLut.Threshold(lowData, highData);
        
        // 读取LUT数据并缩放到8位
        // 注意：SapData::GetData().mono 对于单色格式始终有效
        SapData data;  // 在循环外声明以避免重复构造/析构
        for (int i = 0; i < lutSize; ++i) {
            if (sapLut.Read(i, &data)) {
                int value = data.GetData().mono;
                // 映射阈值范围到8位
                if (value <= lowThreshold) {
                    result[i] = 0;
                } else if (value >= highThreshold) {
                    result[i] = 255;
                } else {
                    result[i] = static_cast<uchar>(std::round((value - lowThreshold) * 255.0 / range));
                }
            } else {
                // 回退到手动计算
                if (i <= lowThreshold) {
                    result[i] = 0;
                } else if (i >= highThreshold) {
                    result[i] = 255;
                } else {
                    result[i] = static_cast<uchar>(std::round((i - lowThreshold) * 255.0 / range));
                }
            }
        }
    } else {
        // SapLut 创建失败，回退到手动计算
        for (int i = 0; i < lutSize; ++i) {
            if (i <= lowThreshold) {
                result[i] = 0;
            } else if (i >= highThreshold) {
                result[i] = 255;
            } else {
                result[i] = static_cast<uchar>(std::round((i - lowThreshold) * 255.0 / range));
            }
        }
    }
#else
    // 非 Windows 平台使用手动计算
    for (int i = 0; i < lutSize; ++i) {
        if (i <= lowThreshold) {
            result[i] = 0;
        } else if (i >= highThreshold) {
            result[i] = 255;
        } else {
            result[i] = static_cast<uchar>(std::round((i - lowThreshold) * 255.0 / range));
        }
    }
#endif
    
    return result;
}
