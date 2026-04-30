#include "image_algorithms_enhance.h"
#include <QCoreApplication>
#include <cmath>

// Gamma Correction
GammaCorrectionAlgorithm::GammaCorrectionAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["gamma"] = 1.0;
}

AlgorithmInfo GammaCorrectionAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "enhance.gamma";
    info.name = QCoreApplication::translate("Algorithms", "Gamma校正");
    info.category = QCoreApplication::translate("Algorithms", "图像增强");
    info.description = QCoreApplication::translate("Algorithms", "非线性亮度调整，使用幂函数变换");
    
    info.parameters.append(AlgorithmParameter(
        "gamma",
        QCoreApplication::translate("Algorithms", "Gamma值"),
        "double", 1.0, 0.1, 5.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "Gamma值 (<1使图像变亮, >1使图像变暗)")
    ));
    
    return info;
}

cv::UMat GammaCorrectionAlgorithm::processImpl(const cv::UMat &input)
{
    double gamma = getParameter("gamma").toDouble();
    
    if (gamma <= 0) gamma = 1.0;
    
    // Build lookup table
    cv::Mat lookUpTable(1, 256, CV_8U);
    uchar* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<uchar>(std::pow(i / 255.0, gamma) * 255.0);
    }
    
    cv::UMat output;
    cv::LUT(input, lookUpTable, output);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat GammaCorrectionAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double gamma = getParameter("gamma").toDouble();

    if (gamma <= 0) gamma = 1.0;

    // Build lookup table
    cv::Mat lookUpTable(1, 256, CV_8U);
    uchar* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<uchar>(std::pow(i / 255.0, gamma) * 255.0);
    }

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::LookUpTable> lut = cv::cuda::createLookUpTable(lookUpTable);
    lut->transform(input, output, stream);

    return output;
}
#endif

// Brightness/Contrast
BrightnessContrastAlgorithm::BrightnessContrastAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["brightness"] = 0;
    m_parameters["contrast"] = 1.0;
}

AlgorithmInfo BrightnessContrastAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "enhance.brightness_contrast";
    info.name = QCoreApplication::translate("Algorithms", "亮度/对比度");
    info.category = QCoreApplication::translate("Algorithms", "图像增强");
    info.description = QCoreApplication::translate("Algorithms", "调整图像的亮度和对比度");
    
    info.parameters.append(AlgorithmParameter(
        "brightness",
        QCoreApplication::translate("Algorithms", "亮度"),
        "int", 0, -65535, 65535, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "亮度调整值 (-65535 到 65535)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "contrast",
        QCoreApplication::translate("Algorithms", "对比度"),
        "double", 1.0, 0.0, 3.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "对比度倍数 (0-3)")
    ));
    
    return info;
}

cv::UMat BrightnessContrastAlgorithm::processImpl(const cv::UMat &input)
{
    int brightness = getParameter("brightness").toInt();
    double contrast = getParameter("contrast").toDouble();
    
    cv::UMat output;
    // new_pixel = old_pixel * contrast + brightness
    input.convertTo(output, -1, contrast, brightness);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat BrightnessContrastAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int brightness = getParameter("brightness").toInt();
    double contrast = getParameter("contrast").toDouble();

    cv::cuda::GpuMat output;
    // new_pixel = old_pixel * contrast + brightness
    input.convertTo(output, -1, contrast, brightness, stream);
    return output;
}
#endif

// Histogram Equalization
HistogramEqualizationAlgorithm::HistogramEqualizationAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    // No parameters needed for basic histogram equalization
}

AlgorithmInfo HistogramEqualizationAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "enhance.histogram_eq";
    info.name = QCoreApplication::translate("Algorithms", "直方图均衡化");
    info.category = QCoreApplication::translate("Algorithms", "图像增强");
    info.description = QCoreApplication::translate("Algorithms", "通过重新分配像素值来增强图像对比度");
    
    // No parameters for basic histogram equalization
    
    return info;
}

cv::UMat HistogramEqualizationAlgorithm::processImpl(const cv::UMat &input)
{
    cv::UMat output;
    cv::equalizeHist(input, output);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat HistogramEqualizationAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    cv::cuda::GpuMat output;
    cv::cuda::equalizeHist(input, output, stream);
    return output;
}
#endif

// CLAHE
CLAHEAlgorithm::CLAHEAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["clipLimit"] = 2.0;
    m_parameters["tileGridSize"] = 8;
}

AlgorithmInfo CLAHEAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "enhance.clahe";
    info.name = QCoreApplication::translate("Algorithms", "CLAHE");
    info.category = QCoreApplication::translate("Algorithms", "图像增强");
    info.description = QCoreApplication::translate("Algorithms", "对比度受限自适应直方图均衡化，更好地处理局部对比度");
    
    info.parameters.append(AlgorithmParameter(
        "clipLimit",
        QCoreApplication::translate("Algorithms", "对比度限制"),
        "double", 2.0, 1.0, 10.0, 0.5,
        QStringList(),
        QCoreApplication::translate("Algorithms", "对比度限制阈值 (典型值: 2.0-4.0)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "tileGridSize",
        QCoreApplication::translate("Algorithms", "网格大小"),
        "int", 8, 2, 32, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "直方图均衡化的网格大小")
    ));
    
    return info;
}

cv::UMat CLAHEAlgorithm::processImpl(const cv::UMat &input)
{
    double clipLimit = getParameter("clipLimit").toDouble();
    int tileGridSize = getParameter("tileGridSize").toInt();
    
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clipLimit, cv::Size(tileGridSize, tileGridSize));
    
    cv::UMat output;
    clahe->apply(input, output);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat CLAHEAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double clipLimit = getParameter("clipLimit").toDouble();
    int tileGridSize = getParameter("tileGridSize").toInt();

    cv::Ptr<cv::cuda::CLAHE> clahe = cv::cuda::createCLAHE(clipLimit, cv::Size(tileGridSize, tileGridSize));

    cv::cuda::GpuMat output;
    clahe->apply(input, output, stream);
    return output;
}
#endif

// Negative
NegativeAlgorithm::NegativeAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    // No parameters needed
}

AlgorithmInfo NegativeAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "enhance.negative";
    info.name = QCoreApplication::translate("Algorithms", "图像反相");
    info.category = QCoreApplication::translate("Algorithms", "图像增强");
    info.description = QCoreApplication::translate("Algorithms", "将图像转换为负片效果");
    
    // No parameters for negative
    
    return info;
}

cv::UMat NegativeAlgorithm::processImpl(const cv::UMat &input)
{
    cv::UMat output;
    cv::bitwise_not(input, output);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat NegativeAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    cv::cuda::GpuMat output;
    cv::cuda::bitwise_not(input, output, cv::noArray(), stream);
    return output;
}
#endif

// Sharpen Filter
SharpenAlgorithm::SharpenAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["strength"] = 1.0;
}

AlgorithmInfo SharpenAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "enhance.sharpen";
    info.name = QCoreApplication::translate("Algorithms", "锐化滤波");
    info.category = QCoreApplication::translate("Algorithms", "图像增强");
    info.description = QCoreApplication::translate("Algorithms", "增强图像边缘，使图像更清晰");
    
    info.parameters.append(AlgorithmParameter(
        "strength",
        QCoreApplication::translate("Algorithms", "锐化强度"),
        "double", 1.0, 0.1, 5.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "锐化效果的强度 (1.0为标准)")
    ));
    
    return info;
}

cv::UMat SharpenAlgorithm::processImpl(const cv::UMat &input)
{
    double strength = getParameter("strength").toDouble();
    if (strength <= 0) strength = 1.0;
    
    // Create sharpen kernel with explicit normalization
    // Standard Laplacian sharpen: center = 1 + 8*strength, neighbors = -strength
    double s = strength;
    cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
        -s/9.0, -s/9.0, -s/9.0,
        -s/9.0, 1.0 + 8.0*s/9.0, -s/9.0,
        -s/9.0, -s/9.0, -s/9.0);
    
    cv::UMat output;
    cv::filter2D(input, output, -1, kernel);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat SharpenAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double strength = getParameter("strength").toDouble();
    if (strength <= 0) strength = 1.0;

    // Create sharpen kernel with explicit normalization
    double s = strength;
    cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
        -s/9.0, -s/9.0, -s/9.0,
        -s/9.0, 1.0 + 8.0*s/9.0, -s/9.0,
        -s/9.0, -s/9.0, -s/9.0);

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createLinearFilter(input.type(), input.type(), kernel);
    filter->apply(input, output, stream);
    return output;
}
#endif

// Logarithmic Transform
LogarithmicTransformAlgorithm::LogarithmicTransformAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["c"] = 1.0;
}

AlgorithmInfo LogarithmicTransformAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "enhance.log_transform";
    info.name = QCoreApplication::translate("Algorithms", "对数变换");
    info.category = QCoreApplication::translate("Algorithms", "图像增强");
    info.description = QCoreApplication::translate("Algorithms", "使用对数函数压缩动态范围，增强暗部细节");
    
    info.parameters.append(AlgorithmParameter(
        "c",
        QCoreApplication::translate("Algorithms", "对数系数"),
        "double", 1.0, 0.1, 5.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "对数变换的系数 (控制变换强度)")
    ));
    
    return info;
}

cv::UMat LogarithmicTransformAlgorithm::processImpl(const cv::UMat &input)
{
    double c = getParameter("c").toDouble();
    if (c <= 0) c = 1.0;
    
    // Build lookup table for log transform
    cv::Mat lookUpTable(1, 256, CV_8U);
    uchar* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i) {
        // s = c * log(1 + r)
        double value = c * std::log(1.0 + i / 255.0) / std::log(2.0) * 255.0;
        p[i] = cv::saturate_cast<uchar>(value);
    }
    
    cv::UMat output;
    cv::LUT(input, lookUpTable, output);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat LogarithmicTransformAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double c = getParameter("c").toDouble();
    if (c <= 0) c = 1.0;

    cv::Mat lookUpTable(1, 256, CV_8U);
    uchar* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i) {
        double value = c * std::log(1.0 + i / 255.0) / std::log(2.0) * 255.0;
        p[i] = cv::saturate_cast<uchar>(value);
    }

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::LookUpTable> lut = cv::cuda::createLookUpTable(lookUpTable);
    lut->transform(input, output, stream);
    return output;
}
#endif

// Distance Transform
DistanceTransformAlgorithm::DistanceTransformAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["distanceType"] = 0;
    m_parameters["threshold"] = 128;
}

AlgorithmInfo DistanceTransformAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "enhance.distance_transform";
    info.name = QCoreApplication::translate("Algorithms", "距离变换");
    info.category = QCoreApplication::translate("Algorithms", "图像增强");
    info.description = QCoreApplication::translate("Algorithms", "计算每个像素到最近零像素的距离");
    
    info.parameters.append(AlgorithmParameter(
        "threshold",
        QCoreApplication::translate("Algorithms", "二值化阈值"),
        "int", 128, 0, 65535, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "用于创建二值图像的阈值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "distanceType",
        QCoreApplication::translate("Algorithms", "距离类型"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "L2距离")
                      << QCoreApplication::translate("Algorithms", "L1距离")
                      << QCoreApplication::translate("Algorithms", "C距离"),
        QCoreApplication::translate("Algorithms", "计算距离使用的度量类型")
    ));
    
    return info;
}

cv::UMat DistanceTransformAlgorithm::processImpl(const cv::UMat &input)
{
    int distanceTypeIdx = getParameter("distanceType").toInt();
    int threshold = getParameter("threshold").toInt();
    
    if (threshold < 0) threshold = 128;
    
    int distanceType;
    switch (distanceTypeIdx) {
        case 0: distanceType = cv::DIST_L2; break;
        case 1: distanceType = cv::DIST_L1; break;
        case 2: distanceType = cv::DIST_C; break;
        default: distanceType = cv::DIST_L2; break;
    }
    
    cv::Mat inputMat = input.getMat(cv::ACCESS_READ);
    
    // Threshold to get binary image
    cv::Mat binary;
    cv::threshold(inputMat, binary, threshold, 255, cv::THRESH_BINARY);
    
    // Distance transform
    cv::Mat dist;
    cv::distanceTransform(binary, dist, distanceType, cv::DIST_MASK_3);
    
    // Convert to 8-bit and normalize
    cv::Mat dist8u;
    cv::normalize(dist, dist8u, 0, 255, cv::NORM_MINMAX);
    dist8u.convertTo(dist8u, CV_8U);
    
    cv::UMat output;
    dist8u.copyTo(output);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat DistanceTransformAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int distanceTypeIdx = getParameter("distanceType").toInt();
    int threshold = getParameter("threshold").toInt();

    if (threshold < 0) threshold = 128;

    int distanceType;
    switch (distanceTypeIdx) {
        case 0: distanceType = cv::DIST_L2; break;
        case 1: distanceType = cv::DIST_L1; break;
        case 2: distanceType = cv::DIST_C; break;
        default: distanceType = cv::DIST_L2; break;
    }

    // Threshold to get binary image
    cv::cuda::GpuMat binary;
    cv::cuda::threshold(input, binary, threshold, 255, cv::THRESH_BINARY, stream);

    // Download to CPU (OpenCV CUDA distanceTransform support is limited/missing)
    cv::Mat binaryCpu;
    binary.download(binaryCpu);

    cv::Mat dist;
    cv::distanceTransform(binaryCpu, dist, distanceType, cv::DIST_MASK_3);

    // Normalize and convert to 8-bit
    cv::Mat dist8u;
    cv::normalize(dist, dist8u, 0, 255, cv::NORM_MINMAX);
    dist8u.convertTo(dist8u, CV_8U);

    // Upload back to GPU
    cv::cuda::GpuMat result;
    result.upload(dist8u, stream);
    return result;
}
#endif

// Register enhancement algorithms
void registerEnhanceAlgorithms()
{
    ImageAlgorithmFactory::instance().registerAlgorithm<GammaCorrectionAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<BrightnessContrastAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<HistogramEqualizationAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<CLAHEAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<NegativeAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<SharpenAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<LogarithmicTransformAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<DistanceTransformAlgorithm>();
}
