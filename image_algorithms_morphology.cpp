#include "image_algorithms_morphology.h"
#include <QCoreApplication>

// Erosion
ErosionAlgorithm::ErosionAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["kernelSize"] = 3;
    m_parameters["kernelShape"] = 0; // MORPH_RECT
    m_parameters["iterations"] = 1;
}

AlgorithmInfo ErosionAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "morphology.erosion";
    info.name = QCoreApplication::translate("Algorithms", "腐蚀");
    info.category = QCoreApplication::translate("Algorithms", "形态学操作");
    info.description = QCoreApplication::translate("Algorithms", "腐蚀操作，收缩图像中的亮区域");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 3, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "结构元素的大小")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "kernelShape",
        QCoreApplication::translate("Algorithms", "核形状"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "矩形")
                      << QCoreApplication::translate("Algorithms", "十字形")
                      << QCoreApplication::translate("Algorithms", "椭圆形"),
        QCoreApplication::translate("Algorithms", "结构元素的形状")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "iterations",
        QCoreApplication::translate("Algorithms", "迭代次数"),
        "int", 1, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "腐蚀操作的迭代次数")
    ));
    
    return info;
}

cv::UMat ErosionAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();
    
    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;
    
    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));
    
    cv::UMat output;
    cv::erode(input, output, kernel, cv::Point(-1, -1), iterations);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat ErosionAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();

    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;

    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createMorphologyFilter(cv::MORPH_ERODE, input.type(), kernel, cv::Point(-1, -1), iterations);
    filter->apply(input, output, stream);

    return output;
}
#endif

// Dilation
DilationAlgorithm::DilationAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["kernelSize"] = 3;
    m_parameters["kernelShape"] = 0;
    m_parameters["iterations"] = 1;
}

AlgorithmInfo DilationAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "morphology.dilation";
    info.name = QCoreApplication::translate("Algorithms", "膨胀");
    info.category = QCoreApplication::translate("Algorithms", "形态学操作");
    info.description = QCoreApplication::translate("Algorithms", "膨胀操作，扩展图像中的亮区域");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 3, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "结构元素的大小")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "kernelShape",
        QCoreApplication::translate("Algorithms", "核形状"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "矩形")
                      << QCoreApplication::translate("Algorithms", "十字形")
                      << QCoreApplication::translate("Algorithms", "椭圆形"),
        QCoreApplication::translate("Algorithms", "结构元素的形状")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "iterations",
        QCoreApplication::translate("Algorithms", "迭代次数"),
        "int", 1, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "膨胀操作的迭代次数")
    ));
    
    return info;
}

cv::UMat DilationAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();
    
    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;
    
    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));
    
    cv::UMat output;
    cv::dilate(input, output, kernel, cv::Point(-1, -1), iterations);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat DilationAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();

    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;

    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createMorphologyFilter(cv::MORPH_DILATE, input.type(), kernel, cv::Point(-1, -1), iterations);
    filter->apply(input, output, stream);

    return output;
}
#endif

// Opening
OpeningAlgorithm::OpeningAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["kernelSize"] = 3;
    m_parameters["kernelShape"] = 0;
    m_parameters["iterations"] = 1;
}

AlgorithmInfo OpeningAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "morphology.opening";
    info.name = QCoreApplication::translate("Algorithms", "开运算");
    info.category = QCoreApplication::translate("Algorithms", "形态学操作");
    info.description = QCoreApplication::translate("Algorithms", "开运算 (先腐蚀后膨胀)，用于去除小的亮点");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 3, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "结构元素的大小")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "kernelShape",
        QCoreApplication::translate("Algorithms", "核形状"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "矩形")
                      << QCoreApplication::translate("Algorithms", "十字形")
                      << QCoreApplication::translate("Algorithms", "椭圆形"),
        QCoreApplication::translate("Algorithms", "结构元素的形状")
    ));
        info.parameters.append(AlgorithmParameter(
        "iterations",
        QCoreApplication::translate("Algorithms", "迭代次数"),
        "int", 1, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "开运算的迭代次数")
    ));
        return info;
}

cv::UMat OpeningAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();
    
    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;
    
    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));
    
    cv::UMat output;
    cv::morphologyEx(input, output, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), iterations);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat OpeningAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();

    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;

    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createMorphologyFilter(cv::MORPH_OPEN, input.type(), kernel, cv::Point(-1, -1), iterations);
    filter->apply(input, output, stream);

    return output;
}
#endif

// Closing
ClosingAlgorithm::ClosingAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["kernelSize"] = 3;
    m_parameters["kernelShape"] = 0;
    m_parameters["iterations"] = 1;
}

AlgorithmInfo ClosingAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "morphology.closing";
    info.name = QCoreApplication::translate("Algorithms", "闭运算");
    info.category = QCoreApplication::translate("Algorithms", "形态学操作");
    info.description = QCoreApplication::translate("Algorithms", "闭运算 (先膨胀后腐蚀)，用于填充小的暗洞");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 3, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "结构元素的大小")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "kernelShape",
        QCoreApplication::translate("Algorithms", "核形状"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "矩形")
                      << QCoreApplication::translate("Algorithms", "十字形")
                      << QCoreApplication::translate("Algorithms", "椭圆形"),
        QCoreApplication::translate("Algorithms", "结构元素的形状")
    ));
        info.parameters.append(AlgorithmParameter(
        "iterations",
        QCoreApplication::translate("Algorithms", "迭代次数"),
        "int", 1, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "闭运算的迭代次数")
    ));
        return info;
}

cv::UMat ClosingAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();
    
    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;
    
    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));
    
    cv::UMat output;
    cv::morphologyEx(input, output, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), iterations);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat ClosingAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();

    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;

    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createMorphologyFilter(cv::MORPH_CLOSE, input.type(), kernel, cv::Point(-1, -1), iterations);
    filter->apply(input, output, stream);

    return output;
}
#endif

// Morphological Gradient
MorphGradientAlgorithm::MorphGradientAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["kernelSize"] = 3;
    m_parameters["kernelShape"] = 0;
    m_parameters["iterations"] = 1;
}

AlgorithmInfo MorphGradientAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "morphology.gradient";
    info.name = QCoreApplication::translate("Algorithms", "形态学梯度");
    info.category = QCoreApplication::translate("Algorithms", "形态学操作");
    info.description = QCoreApplication::translate("Algorithms", "形态学梯度 (膨胀与腐蚀的差)，突出边缘");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 3, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "结构元素的大小")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "kernelShape",
        QCoreApplication::translate("Algorithms", "核形状"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "矩形")
                      << QCoreApplication::translate("Algorithms", "十字形")
                      << QCoreApplication::translate("Algorithms", "椭圆形"),
        QCoreApplication::translate("Algorithms", "结构元素的形状")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "iterations",
        QCoreApplication::translate("Algorithms", "迭代次数"),
        "int", 1, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "形态学梯度的迭代次数")
    ));
    
    return info;
}

cv::UMat MorphGradientAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();
    
    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;
    
    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));
    
    cv::UMat output;
    cv::morphologyEx(input, output, cv::MORPH_GRADIENT, kernel, cv::Point(-1, -1), iterations);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat MorphGradientAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();

    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;

    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createMorphologyFilter(cv::MORPH_GRADIENT, input.type(), kernel, cv::Point(-1, -1), iterations);
    filter->apply(input, output, stream);

    return output;
}
#endif

// Top Hat
TopHatAlgorithm::TopHatAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["kernelSize"] = 9;
    m_parameters["kernelShape"] = 0;
    m_parameters["iterations"] = 1;
}

AlgorithmInfo TopHatAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "morphology.tophat";
    info.name = QCoreApplication::translate("Algorithms", "顶帽变换");
    info.category = QCoreApplication::translate("Algorithms", "形态学操作");
    info.description = QCoreApplication::translate("Algorithms", "顶帽变换 (原图与开运算的差)，提取亮的细节");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 9, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "结构元素的大小")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "kernelShape",
        QCoreApplication::translate("Algorithms", "核形状"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "矩形")
                      << QCoreApplication::translate("Algorithms", "十字形")
                      << QCoreApplication::translate("Algorithms", "椭圆形"),
        QCoreApplication::translate("Algorithms", "结构元素的形状")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "iterations",
        QCoreApplication::translate("Algorithms", "迭代次数"),
        "int", 1, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "顶帽变换的迭代次数")
    ));
    
    return info;
}

cv::UMat TopHatAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();
    
    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;
    
    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));
    
    cv::UMat output;
    cv::morphologyEx(input, output, cv::MORPH_TOPHAT, kernel, cv::Point(-1, -1), iterations);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat TopHatAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();

    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;

    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createMorphologyFilter(cv::MORPH_TOPHAT, input.type(), kernel, cv::Point(-1, -1), iterations);
    filter->apply(input, output, stream);

    return output;
}
#endif

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat BlackHatAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();

    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;

    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createMorphologyFilter(cv::MORPH_BLACKHAT, input.type(), kernel, cv::Point(-1, -1), iterations);
    filter->apply(input, output, stream);

    return output;
}
#endif

// Black Hat
BlackHatAlgorithm::BlackHatAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["kernelSize"] = 9;
    m_parameters["kernelShape"] = 0;
    m_parameters["iterations"] = 1;
}

AlgorithmInfo BlackHatAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "morphology.blackhat";
    info.name = QCoreApplication::translate("Algorithms", "黑帽变换");
    info.category = QCoreApplication::translate("Algorithms", "形态学操作");
    info.description = QCoreApplication::translate("Algorithms", "黑帽变换 (闭运算与原图的差)，提取暗的细节");
    
    info.parameters.append(AlgorithmParameter(
        "kernelSize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 9, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "结构元素的大小")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "kernelShape",
        QCoreApplication::translate("Algorithms", "核形状"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "矩形")
                      << QCoreApplication::translate("Algorithms", "十字形")
                      << QCoreApplication::translate("Algorithms", "椭圆形"),
        QCoreApplication::translate("Algorithms", "结构元素的形状")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "iterations",
        QCoreApplication::translate("Algorithms", "迭代次数"),
        "int", 1, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "黑帽变换的迭代次数")
    ));
    
    return info;
}

cv::UMat BlackHatAlgorithm::processImpl(const cv::UMat &input)
{
    int kernelSize = getParameter("kernelSize").toInt();
    int kernelShape = getParameter("kernelShape").toInt();
    int iterations = getParameter("iterations").toInt();
    
    if (kernelSize < 1) kernelSize = 1;
    if (iterations < 1) iterations = 1;
    
    cv::Mat kernel = cv::getStructuringElement(kernelShape, cv::Size(kernelSize, kernelSize));
    
    cv::UMat output;
    cv::morphologyEx(input, output, cv::MORPH_BLACKHAT, kernel, cv::Point(-1, -1), iterations);
    return output;
}

// Register morphology algorithms
void registerMorphologyAlgorithms()
{
    ImageAlgorithmFactory::instance().registerAlgorithm<ErosionAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<DilationAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<OpeningAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<ClosingAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<MorphGradientAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<TopHatAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<BlackHatAlgorithm>();
}
