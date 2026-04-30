#include "image_algorithms_filtering.h"
#include <QCoreApplication>

// Gaussian Blur
GaussianBlurAlgorithm::GaussianBlurAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    // Set default parameters
    m_parameters["kernelSize"] = 5;
    m_parameters["sigmaX"] = 0.0;
}

AlgorithmInfo GaussianBlurAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "filtering.gaussian_blur";
    info.name = QCoreApplication::translate("Algorithms", "高斯模糊");
    info.category = QCoreApplication::translate("Algorithms", "滤波处理");
    info.description = QCoreApplication::translate("Algorithms", "应用高斯平滑以减少噪声和细节");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 5, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "高斯核的大小 (必须为奇数)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "sigmaX",
        QCoreApplication::translate("Algorithms", "Sigma X"),
        "double", 0.0, 0.0, 10.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "X方向的标准差 (0表示自动计算)")
    ));
    
    return info;
}

cv::UMat GaussianBlurAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    double sigmaX = getParameter("sigmaX").toDouble();
    
    // Ensure kernel size is odd
    if (kernelSize % 2 == 0) kernelSize++;
    if (kernelSize < 1) kernelSize = 1;
    
    cv::UMat output;
    cv::GaussianBlur(input, output, cv::Size(kernelSize, kernelSize), sigmaX);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat GaussianBlurAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();
    double sigmaX = getParameter("sigmaX").toDouble();

    // Ensure kernel size is odd
    if (kernelSize % 2 == 0) kernelSize++;
    if (kernelSize < 1) kernelSize = 1;

    cv::cuda::GpuMat output;
    // Create filter
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createGaussianFilter(input.type(), input.type(), cv::Size(kernelSize, kernelSize), sigmaX);
    filter->apply(input, output, stream);

    return output;
}
#endif

// Median Blur
MedianBlurAlgorithm::MedianBlurAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["kernelSize"] = 5;
}

AlgorithmInfo MedianBlurAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "filtering.median_blur";
    info.name = QCoreApplication::translate("Algorithms", "中值滤波");
    info.category = QCoreApplication::translate("Algorithms", "滤波处理");
    info.description = QCoreApplication::translate("Algorithms", "应用中值滤波器以去除椒盐噪声");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 5, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "中值滤波核的大小 (必须为奇数；设为1表示不进行滤波)")
    ));
    
    return info;
}

cv::UMat MedianBlurAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    
    // Ensure kernel size is odd
    if (kernelSize % 2 == 0) kernelSize++;
    if (kernelSize < 1) kernelSize = 1;

    // Treat 1 as a no-op (also avoids OpenCV implementations that reject ksize==1)
    if (kernelSize <= 1) {
        return input;
    }
    
    cv::UMat output;
    cv::medianBlur(input, output, kernelSize);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat MedianBlurAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();

    // Ensure kernel size is odd
    if (kernelSize % 2 == 0) kernelSize++;
    if (kernelSize < 1) kernelSize = 1;

    // OpenCV CUDA median filter requires windowSize >= 3
    // For consistency with UI (allow kernelSize==1), treat 1 as a no-op.
    if (kernelSize <= 1) {
        return input;
    }

    if (kernelSize < 3) {
        kernelSize = 3;
    }

    cv::cuda::GpuMat output;
    // Note: createMedianFilter window size must be odd and greater than 1, and typically small (e.g. <= 7 or depending on implementation)
    // OpenCV CUDA median filter might have restrictions on kernel size.
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createMedianFilter(input.type(), kernelSize);
    filter->apply(input, output, stream);

    return output;
}
#endif

// Bilateral Filter
BilateralFilterAlgorithm::BilateralFilterAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["d"] = 9;
    m_parameters["sigmaColor"] = 75.0;
    m_parameters["sigmaSpace"] = 75.0;
}

AlgorithmInfo BilateralFilterAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "filtering.bilateral";
    info.name = QCoreApplication::translate("Algorithms", "双边滤波");
    info.category = QCoreApplication::translate("Algorithms", "滤波处理");
    info.description = QCoreApplication::translate("Algorithms", "保边平滑滤波器，在去噪的同时保留边缘");
    
    info.parameters.append(AlgorithmParameter(
        "d",
        QCoreApplication::translate("Algorithms", "像素邻域直径"),
        "int", 9, 1, 25, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "过滤期间使用的每个像素邻域的直径")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "sigmaColor",
        QCoreApplication::translate("Algorithms", "颜色空间Sigma"),
        "double", 75.0, 1.0, 65535.0, 5.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "颜色空间中的滤波sigma值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "sigmaSpace",
        QCoreApplication::translate("Algorithms", "坐标空间Sigma"),
        "double", 75.0, 1.0, 500.0, 5.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "坐标空间中的滤波sigma值")
    ));
    
    return info;
}

cv::UMat BilateralFilterAlgorithm::processImpl(const cv::UMat &input)
{
    int d = getParameter("d").toInt();
    double sigmaColor = getParameter("sigmaColor").toDouble();
    double sigmaSpace = getParameter("sigmaSpace").toDouble();
    
    cv::UMat output;
    cv::bilateralFilter(input, output, d, sigmaColor, sigmaSpace);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat BilateralFilterAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int d = getParameter("d").toInt();
    double sigmaColor = getParameter("sigmaColor").toDouble();
    double sigmaSpace = getParameter("sigmaSpace").toDouble();

    cv::cuda::GpuMat output;
    cv::cuda::bilateralFilter(input, output, d, sigmaColor, sigmaSpace, cv::BORDER_DEFAULT, stream);
    return output;
}
#endif

// Box Filter
BoxFilterAlgorithm::BoxFilterAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["kernelSize"] = 5;
    m_parameters["normalize"] = true;
}

AlgorithmInfo BoxFilterAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "filtering.box_filter";
    info.name = QCoreApplication::translate("Algorithms", "均值滤波");
    info.category = QCoreApplication::translate("Algorithms", "滤波处理");
    info.description = QCoreApplication::translate("Algorithms", "简单的均值滤波器");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 5, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "滤波核的大小")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "normalize",
        QCoreApplication::translate("Algorithms", "归一化"),
        "bool", true,
        QVariant(), QVariant(), QVariant(),
        QStringList(),
        QCoreApplication::translate("Algorithms", "是否对核进行归一化")
    ));
    
    return info;
}

cv::UMat BoxFilterAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    bool normalize = getParameter("normalize").toBool();
    
    if (kernelSize < 1) kernelSize = 1;
    
    cv::UMat output;
    cv::boxFilter(input, output, -1, cv::Size(kernelSize, kernelSize), cv::Point(-1, -1), normalize);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat BoxFilterAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();
    // bool normalize = getParameter("normalize").toBool(); // CUDA boxFilter is always normalized

    if (kernelSize < 1) kernelSize = 1;

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createBoxFilter(input.type(), input.type(), cv::Size(kernelSize, kernelSize));
    filter->apply(input, output, stream);

    return output;
}
#endif

// Register filtering algorithms
void registerFilteringAlgorithms()
{
    ImageAlgorithmFactory::instance().registerAlgorithm<GaussianBlurAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<MedianBlurAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<BilateralFilterAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<BoxFilterAlgorithm>();
}
