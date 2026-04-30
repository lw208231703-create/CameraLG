#include "image_algorithms_threshold.h"
#include <QCoreApplication>

// Binary Threshold
BinaryThresholdAlgorithm::BinaryThresholdAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["threshold"] = 127.0;
    m_parameters["maxValue"] = 255.0;
    m_parameters["thresholdType"] = 0; // THRESH_BINARY
}

AlgorithmInfo BinaryThresholdAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "threshold.binary";
    info.name = QCoreApplication::translate("Algorithms", "二值化阈值");
    info.category = QCoreApplication::translate("Algorithms", "阈值处理");
    info.description = QCoreApplication::translate("Algorithms", "将图像转换为二值图像");
    
    info.parameters.append(AlgorithmParameter(
        "threshold",
        QCoreApplication::translate("Algorithms", "阈值"),
        "double", 127.0, 0.0, 65535.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "用于分类像素值的阈值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "maxValue",
        QCoreApplication::translate("Algorithms", "最大值"),
        "double", 255.0, 0.0, 65535.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "超过阈值的像素所赋的值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "thresholdType",
        QCoreApplication::translate("Algorithms", "阈值类型"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "二值化")
                      << QCoreApplication::translate("Algorithms", "反二值化")
                      << QCoreApplication::translate("Algorithms", "截断")
                      << QCoreApplication::translate("Algorithms", "置零")
                      << QCoreApplication::translate("Algorithms", "反置零"),
        QCoreApplication::translate("Algorithms", "阈值处理的类型")
    ));
    
    return info;
}

cv::UMat BinaryThresholdAlgorithm::processImpl(const cv::UMat &input)
{
    double threshValue = getParameter("threshold").toDouble();
    double maxValue = getParameter("maxValue").toDouble();
    int thresholdType = getParameter("thresholdType").toInt();
    
    cv::UMat output;
    cv::threshold(input, output, threshValue, maxValue, thresholdType);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat BinaryThresholdAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double threshValue = getParameter("threshold").toDouble();
    double maxValue = getParameter("maxValue").toDouble();
    int thresholdType = getParameter("thresholdType").toInt();

    cv::cuda::GpuMat output;
    cv::cuda::threshold(input, output, threshValue, maxValue, thresholdType, stream);
    return output;
}
#endif

// Adaptive Threshold
AdaptiveThresholdAlgorithm::AdaptiveThresholdAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["maxValue"] = 255.0;
    m_parameters["adaptiveMethod"] = 0; // ADAPTIVE_THRESH_MEAN_C
    m_parameters["thresholdType"] = 0; // THRESH_BINARY
    m_parameters["blockSize"] = 11;
    m_parameters["C"] = 2.0;
}

AlgorithmInfo AdaptiveThresholdAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "threshold.adaptive";
    info.name = QCoreApplication::translate("Algorithms", "自适应阈值");
    info.category = QCoreApplication::translate("Algorithms", "阈值处理");
    info.description = QCoreApplication::translate("Algorithms", "基于局部邻域计算阈值的自适应阈值处理");
    
    info.parameters.append(AlgorithmParameter(
        "maxValue",
        QCoreApplication::translate("Algorithms", "最大值"),
        "double", 255.0, 0.0, 65535.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "满足条件的像素所赋的值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "adaptiveMethod",
        QCoreApplication::translate("Algorithms", "自适应方法"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "均值")
                      << QCoreApplication::translate("Algorithms", "高斯"),
        QCoreApplication::translate("Algorithms", "计算阈值的方法")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "thresholdType",
        QCoreApplication::translate("Algorithms", "阈值类型"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "二值化")
                      << QCoreApplication::translate("Algorithms", "反二值化"),
        QCoreApplication::translate("Algorithms", "阈值处理的类型")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "blockSize",
        QCoreApplication::translate("Algorithms", "块大小"),
        "int", 11, 3, 51, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "计算阈值的邻域大小 (必须为奇数)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "C",
        QCoreApplication::translate("Algorithms", "常数C"),
        "double", 2.0, -50.0, 50.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "从计算出的阈值中减去的常数")
    ));
    
    return info;
}

cv::UMat AdaptiveThresholdAlgorithm::processImpl(const cv::UMat &input)
{
    double maxValue = getParameter("maxValue").toDouble();
    int adaptiveMethod = getParameter("adaptiveMethod").toInt();
    int thresholdType = getParameter("thresholdType").toInt();
    int blockSize = getParameter("blockSize").toInt();
    double C = getParameter("C").toDouble();
    
    // Ensure blockSize is odd and >= 3
    if (blockSize % 2 == 0) blockSize++;
    if (blockSize < 3) blockSize = 3;
    
    // Map threshold type (only binary and binary_inv are supported)
    int cvThreshType = (thresholdType == 0) ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;
    
    cv::UMat output;
    cv::adaptiveThreshold(input, output, maxValue, adaptiveMethod, cvThreshType, blockSize, C);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat AdaptiveThresholdAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double maxValue = getParameter("maxValue").toDouble();
    int adaptiveMethod = getParameter("adaptiveMethod").toInt();
    int thresholdType = getParameter("thresholdType").toInt();
    int blockSize = getParameter("blockSize").toInt();
    double C = getParameter("C").toDouble();

    // Ensure blockSize is odd and >= 3
    if (blockSize % 2 == 0) blockSize++;
    if (blockSize < 3) blockSize = 3;

    cv::cuda::GpuMat output;

    // Calculate local mean or gaussian weighted sum
    cv::cuda::GpuMat blurred;
    if (adaptiveMethod == 0) { // ADAPTIVE_THRESH_MEAN_C
        cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createBoxFilter(input.type(), input.type(), cv::Size(blockSize, blockSize));
        filter->apply(input, blurred, stream);
    } else { // ADAPTIVE_THRESH_GAUSSIAN_C
        // Sigma is computed from blockSize in standard OpenCV: 0.3*((blockSize-1)*0.5 - 1) + 0.8
        double sigma = 0.3 * ((blockSize - 1) * 0.5 - 1) + 0.8;
        cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createGaussianFilter(input.type(), input.type(), cv::Size(blockSize, blockSize), sigma);
        filter->apply(input, blurred, stream);
    }

    // Subtract C
    cv::cuda::GpuMat thresholdMat;
    cv::cuda::subtract(blurred, cv::Scalar(C), thresholdMat, cv::noArray(), -1, stream);

    // Compare
    cv::cuda::GpuMat mask;
    cv::cuda::compare(input, thresholdMat, mask, cv::CMP_GT, stream);

    if (thresholdType == 0) { // THRESH_BINARY
        if (maxValue == 255.0) {
            output = mask;
        } else {
            mask.convertTo(output, -1, maxValue / 255.0, 0, stream);
        }
    } else { // THRESH_BINARY_INV
        cv::cuda::bitwise_not(mask, mask, cv::noArray(), stream);
        if (maxValue == 255.0) {
            output = mask;
        } else {
            mask.convertTo(output, -1, maxValue / 255.0, 0, stream);
        }
    }

    return output;
}
#endif

// Otsu Threshold
OtsuThresholdAlgorithm::OtsuThresholdAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["maxValue"] = 255.0;
    m_parameters["thresholdType"] = 0; // THRESH_BINARY
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat OtsuThresholdAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double maxValue = getParameter("maxValue").toDouble();
    int thresholdType = getParameter("thresholdType").toInt();

    // Map threshold type
    int cvThreshType = (thresholdType == 0) ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;

    // Calculate Otsu threshold on CPU
    cv::Mat cpuMat;
    input.download(cpuMat, stream);
    stream.waitForCompletion();

    double otsuThresh = cv::threshold(cpuMat, cpuMat, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::cuda::GpuMat output;
    cv::cuda::threshold(input, output, otsuThresh, maxValue, cvThreshType, stream);

    return output;
}
#endif

AlgorithmInfo OtsuThresholdAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "threshold.otsu";
    info.name = QCoreApplication::translate("Algorithms", "Otsu阈值");
    info.category = QCoreApplication::translate("Algorithms", "阈值处理");
    info.description = QCoreApplication::translate("Algorithms", "使用Otsu方法自动计算最佳阈值");
    
    info.parameters.append(AlgorithmParameter(
        "maxValue",
        QCoreApplication::translate("Algorithms", "最大值"),
        "double", 255.0, 0.0, 65535.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "超过阈值的像素所赋的值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "thresholdType",
        QCoreApplication::translate("Algorithms", "阈值类型"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "二值化")
                      << QCoreApplication::translate("Algorithms", "反二值化"),
        QCoreApplication::translate("Algorithms", "阈值处理的类型")
    ));
    
    return info;
}

cv::UMat OtsuThresholdAlgorithm::processImpl(const cv::UMat &input)
{
    double maxValue = getParameter("maxValue").toDouble();
    int thresholdType = getParameter("thresholdType").toInt();
    
    // Map threshold type
    int cvThreshType = (thresholdType == 0) ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;
    
    cv::UMat output;
    cv::threshold(input, output, 0, maxValue, cvThreshType | cv::THRESH_OTSU);
    return output;
}

// Register threshold algorithms
void registerThresholdAlgorithms()
{
    ImageAlgorithmFactory::instance().registerAlgorithm<BinaryThresholdAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<AdaptiveThresholdAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<OtsuThresholdAlgorithm>();
}
