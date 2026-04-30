#ifndef IMAGE_DEPTH_CONVERTER_H
#define IMAGE_DEPTH_CONVERTER_H

#include <QImage>
#include <QString>
#include <cstdint>
#include <vector>

/**
 * @brief 图像位深转换方法枚举
 * 
 * 用于将高位深图像（如16位）转换为8位图像
 */
enum class DepthConversionMethod {
    // 位段提取法 - 直接截取16位中的8位，共9种变体
    BitExtract_0_7,     // 截取第0-7位 (最低8位)
    BitExtract_1_8,     // 截取第1-8位
    BitExtract_2_9,     // 截取第2-9位
    BitExtract_3_10,    // 截取第3-10位
    BitExtract_4_11,    // 截取第4-11位
    BitExtract_5_12,    // 截取第5-12位
    BitExtract_6_13,    // 截取第6-13位
    BitExtract_7_14,    // 截取第7-14位
    BitExtract_8_15,    // 截取第8-15位 (最高8位) 
    // 线性缩放法 - 归一化，最通用
    LinearScaling,
    // 直方图均衡化法 - 提升对比度
    HistogramEqualization,
    // 伽马校正法 - 非线性视觉适配
    GammaCorrection,
    // 阈值截断/裁剪法 - 聚焦感兴趣范围
    ThresholdTruncation,
    // 非线性压缩法 - 对数/指数映射
    LogarithmicCompression,
    ExponentialCompression, 
    // 量化法 - 分层舍入
    Quantization
};

/**
 * @brief 位段提取变体枚举
 * 
 * 用于指定位段提取法中截取哪8位
 */
enum class BitExtractVariant {
    Bits_0_7 = 0,   // 截取第0-7位 (最低8位)
    Bits_1_8 = 1,   // 截取第1-8位
    Bits_2_9 = 2,   // 截取第2-9位
    Bits_3_10 = 3,  // 截取第3-10位
    Bits_4_11 = 4,  // 截取第4-11位
    Bits_5_12 = 5,  // 截取第5-12位
    Bits_6_13 = 6,  // 截取第6-13位
    Bits_7_14 = 7,  // 截取第7-14位
    Bits_8_15 = 8   // 截取第8-15位 (最高8位)
};

//==============================================================================
// 宏定义 - 选择使用哪种缩放模型
//==============================================================================
// 取消注释以下其中一个宏来选择转换方法，或者使用默认的线性缩放法
// 位段提取法 (9种变体，通过 DEPTH_CONV_BIT_EXTRACT_VARIANT 选择)
#define DEPTH_CONVERSION_METHOD_BIT_EXTRACT

// 线性缩放法 (默认) - 归一化，最通用
//#define DEPTH_CONVERSION_METHOD_LINEAR

// 直方图均衡化法 - 提升对比度
// #define DEPTH_CONVERSION_METHOD_HISTOGRAM_EQ

// 伽马校正法 - 非线性视觉适配
// #define DEPTH_CONVERSION_METHOD_GAMMA

// 阈值截断/裁剪法 - 聚焦感兴趣范围
// #define DEPTH_CONVERSION_METHOD_THRESHOLD

// 非线性压缩法 - 对数映射
// #define DEPTH_CONVERSION_METHOD_LOG

// 非线性压缩法 - 指数映射
// #define DEPTH_CONVERSION_METHOD_EXP

// 量化法 - 分层舍入
// #define DEPTH_CONVERSION_METHOD_QUANTIZATION

//==============================================================================
// 位段提取法变体选择 (仅当 DEPTH_CONVERSION_METHOD_BIT_EXTRACT 启用时有效)
//==============================================================================
// 0 = 最低8位 (0-7), 8 = 最高8位 (8-15)
#define DEPTH_CONV_BIT_EXTRACT_VARIANT 5

//==============================================================================
// 参数配置
//==============================================================================
// 伽马校正参数 (默认值 2.2，常用于显示器)
#define DEPTH_CONV_GAMMA_VALUE 2.2

// 阈值截断参数 - 低阈值百分比 (0.0-1.0)
#define DEPTH_CONV_THRESHOLD_LOW_PERCENT 0.02

// 阈值截断参数 - 高阈值百分比 (0.0-1.0)
#define DEPTH_CONV_THRESHOLD_HIGH_PERCENT 0.98

// 量化法参数 - 量化级数 (2-256)
#define DEPTH_CONV_QUANTIZATION_LEVELS 16


/**
 * @brief 图像位深转换器类
 * 
 * 提供多种方法将高位深图像转换为8位图像
 */
class ImageDepthConverter
{
public:
    /**
     * @brief 使用编译时宏定义选择的方法转换图像
     * 
     * @param srcData 源数据指针 (16位像素)
     * @param width 图像宽度
     * @param height 图像高度
     * @param srcPitch 源数据行距 (字节)
     * @param srcBitDepth 源图像位深 (如 10, 12, 14, 16)
     * @return QImage 转换后的8位灰度图像
     */
    static QImage convert(const uint16_t *srcData, int width, int height, 
                          int srcPitch, int srcBitDepth);
    
    /**
     * @brief 使用指定方法转换图像
     * 
     * @param method 转换方法
     * @param srcData 源数据指针 (16位像素)
     * @param width 图像宽度
     * @param height 图像高度
     * @param srcPitch 源数据行距 (字节)
     * @param srcBitDepth 源图像位深 (如 10, 12, 14, 16)
     * @return QImage 转换后的8位灰度图像
     */
    static QImage convertWithMethod(DepthConversionMethod method,
                                    const uint16_t *srcData, int width, int height,
                                    int srcPitch, int srcBitDepth);

    /**
     * @brief 获取当前编译时选择的转换方法
     * @return DepthConversionMethod 当前方法
     */
    static DepthConversionMethod getCurrentMethod();
    
    /**
     * @brief 获取转换方法的名称描述
     * @param method 转换方法
     * @return QString 方法名称
     */
    static QString getMethodName(DepthConversionMethod method);

    // 位段提取法 - 公开以便直接调用
    static QImage bitExtract(const uint16_t *srcData, int width, int height,
                             int srcPitch, int srcBitDepth, int startBit);

    /**
     * @brief 使用 Sapera SapLut::Gamma 生成伽马校正查找表
     * @param srcBitDepth 源图像位深
     * @param gamma 伽马值
     * @return 8位查找表数据
     */
    static std::vector<uchar> generateGammaLutViaSapera(int srcBitDepth, float gamma);
    
    /**
     * @brief 使用 Sapera SapLut::Shift 生成位移查找表
     * @param srcBitDepth 源图像位深
     * @param shiftBits 位移量
     * @return 8位查找表数据
     */
    static std::vector<uchar> generateShiftLutViaSapera(int srcBitDepth, int shiftBits);
    
    /**
     * @brief 使用 Sapera SapLut::Normal 生成归一化查找表
     * @param srcBitDepth 源图像位深
     * @return 8位查找表数据
     */
    static std::vector<uchar> generateNormalLutViaSapera(int srcBitDepth);
    
    /**
     * @brief 使用 Sapera SapLut::Threshold 生成阈值查找表
     * @param srcBitDepth 源图像位深
     * @param lowThreshold 低阈值
     * @param highThreshold 高阈值
     * @return 8位查找表数据
     */
    static std::vector<uchar> generateThresholdLutViaSapera(int srcBitDepth, int lowThreshold, int highThreshold);

private:
    // 线性缩放法
    static QImage linearScaling(const uint16_t *srcData, int width, int height,
                                int srcPitch, int srcBitDepth);
    
    // 直方图均衡化法
    static QImage histogramEqualization(const uint16_t *srcData, int width, int height,
                                        int srcPitch, int srcBitDepth);
    
    // 伽马校正法
    static QImage gammaCorrection(const uint16_t *srcData, int width, int height,
                                  int srcPitch, int srcBitDepth, double gamma);
    
    // 阈值截断/裁剪法
    static QImage thresholdTruncation(const uint16_t *srcData, int width, int height,
                                      int srcPitch, int srcBitDepth,
                                      double lowPercent, double highPercent);
    
    // 对数压缩法
    static QImage logarithmicCompression(const uint16_t *srcData, int width, int height,
                                         int srcPitch, int srcBitDepth);
    
    // 指数压缩法
    static QImage exponentialCompression(const uint16_t *srcData, int width, int height,
                                         int srcPitch, int srcBitDepth);
    
    // 量化法
    static QImage quantization(const uint16_t *srcData, int width, int height,
                               int srcPitch, int srcBitDepth, int levels);
};

#endif // IMAGE_DEPTH_CONVERTER_H
