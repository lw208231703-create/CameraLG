#include "image_algorithms_geometry.h"
#include <QCoreApplication>
#include <opencv2/cudawarping.hpp>

namespace {

// Helper function to calculate perspective transform points from percentage offsets
std::pair<std::vector<cv::Point2f>, std::vector<cv::Point2f>> 
calculatePerspectivePoints(float w, float h, 
                           double topLeftX, double topLeftY,
                           double topRightX, double topRightY,
                           double bottomLeftX, double bottomLeftY,
                           double bottomRightX, double bottomRightY)
{
    // Source points (original corners)
    std::vector<cv::Point2f> srcPoints = {
        cv::Point2f(0, 0),
        cv::Point2f(w - 1, 0),
        cv::Point2f(0, h - 1),
        cv::Point2f(w - 1, h - 1)
    };
    
    // Destination points (offset corners based on percentage)
    std::vector<cv::Point2f> dstPoints = {
        cv::Point2f(w * static_cast<float>(topLeftX) / 100.0f, 
                    h * static_cast<float>(topLeftY) / 100.0f),
        cv::Point2f(w - 1 + w * static_cast<float>(topRightX) / 100.0f, 
                    h * static_cast<float>(topRightY) / 100.0f),
        cv::Point2f(w * static_cast<float>(bottomLeftX) / 100.0f, 
                    h - 1 + h * static_cast<float>(bottomLeftY) / 100.0f),
        cv::Point2f(w - 1 + w * static_cast<float>(bottomRightX) / 100.0f, 
                    h - 1 + h * static_cast<float>(bottomRightY) / 100.0f)
    };
    
    return {srcPoints, dstPoints};
}

} // anonymous namespace

// Rotate Transform
RotateTransformAlgorithm::RotateTransformAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["angle"] = 0.0;
    m_parameters["scale"] = 1.0;
}

AlgorithmInfo RotateTransformAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "geometry.rotate";
    info.name = QCoreApplication::translate("Algorithms", "旋转变换");
    info.category = QCoreApplication::translate("Algorithms", "几何变换");
    info.description = QCoreApplication::translate("Algorithms", "围绕图像中心旋转图像");
    
    info.parameters.append(AlgorithmParameter(
        "angle",
        QCoreApplication::translate("Algorithms", "旋转角度"),
        "double", 0.0, -180.0, 180.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "旋转角度 (正值为逆时针)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "scale",
        QCoreApplication::translate("Algorithms", "缩放比例"),
        "double", 1.0, 0.1, 5.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "旋转时的缩放比例")
    ));
    
    return info;
}

cv::UMat RotateTransformAlgorithm::processImpl(const cv::UMat &input)
{
    double angle = getParameter("angle").toDouble();
    double scale = getParameter("scale").toDouble();
    
    if (scale <= 0) scale = 1.0;
    
    cv::Point2f center(input.cols / 2.0f, input.rows / 2.0f);
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, scale);
    
    cv::UMat output;
    cv::warpAffine(input, output, rotationMatrix, input.size());
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat RotateTransformAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double angle = getParameter("angle").toDouble();
    double scale = getParameter("scale").toDouble();

    if (scale <= 0) scale = 1.0;

    cv::Point2f center(input.cols / 2.0f, input.rows / 2.0f);
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, scale);

    cv::cuda::GpuMat output;
    cv::cuda::warpAffine(input, output, rotationMatrix, input.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(), stream);
    return output;
}
#endif

// Translate Transform
TranslateTransformAlgorithm::TranslateTransformAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["offsetX"] = 0.0;
    m_parameters["offsetY"] = 0.0;
}

AlgorithmInfo TranslateTransformAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "geometry.translate";
    info.name = QCoreApplication::translate("Algorithms", "平移变换");
    info.category = QCoreApplication::translate("Algorithms", "几何变换");
    info.description = QCoreApplication::translate("Algorithms", "将图像沿X和Y轴平移");
    
    info.parameters.append(AlgorithmParameter(
        "offsetX",
        QCoreApplication::translate("Algorithms", "X轴偏移"),
        "double", 0.0, -500.0, 500.0, 10.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "水平方向的偏移量 (像素)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "offsetY",
        QCoreApplication::translate("Algorithms", "Y轴偏移"),
        "double", 0.0, -500.0, 500.0, 10.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "垂直方向的偏移量 (像素)")
    ));
    
    return info;
}

cv::UMat TranslateTransformAlgorithm::processImpl(const cv::UMat &input)
{
    double offsetX = getParameter("offsetX").toDouble();
    double offsetY = getParameter("offsetY").toDouble();
    
    // Create translation matrix
    cv::Mat translationMatrix = (cv::Mat_<double>(2, 3) << 1, 0, offsetX, 0, 1, offsetY);
    
    cv::UMat output;
    cv::warpAffine(input, output, translationMatrix, input.size());
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat TranslateTransformAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double offsetX = getParameter("offsetX").toDouble();
    double offsetY = getParameter("offsetY").toDouble();

    // Create translation matrix
    cv::Mat translationMatrix = (cv::Mat_<double>(2, 3) << 1, 0, offsetX, 0, 1, offsetY);

    cv::cuda::GpuMat output;
    cv::cuda::warpAffine(input, output, translationMatrix, input.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(), stream);
    return output;
}
#endif

// Scale Transform
ScaleTransformAlgorithm::ScaleTransformAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["scaleX"] = 1.0;
    m_parameters["scaleY"] = 1.0;
    m_parameters["interpolation"] = 0;
}

AlgorithmInfo ScaleTransformAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "geometry.scale";
    info.name = QCoreApplication::translate("Algorithms", "缩放变换");
    info.category = QCoreApplication::translate("Algorithms", "几何变换");
    info.description = QCoreApplication::translate("Algorithms", "按比例缩放图像");
    
    info.parameters.append(AlgorithmParameter(
        "scaleX",
        QCoreApplication::translate("Algorithms", "X轴缩放"),
        "double", 1.0, 0.1, 5.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "水平方向的缩放比例")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "scaleY",
        QCoreApplication::translate("Algorithms", "Y轴缩放"),
        "double", 1.0, 0.1, 5.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "垂直方向的缩放比例")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "interpolation",
        QCoreApplication::translate("Algorithms", "插值方式"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "最近邻")
                      << QCoreApplication::translate("Algorithms", "双线性")
                      << QCoreApplication::translate("Algorithms", "三次样条")
                      << QCoreApplication::translate("Algorithms", "区域"),
        QCoreApplication::translate("Algorithms", "缩放时使用的插值方法")
    ));
    
    return info;
}

cv::UMat ScaleTransformAlgorithm::processImpl(const cv::UMat &input)
{
    double scaleX = getParameter("scaleX").toDouble();
    double scaleY = getParameter("scaleY").toDouble();
    int interpIdx = getParameter("interpolation").toInt();
    
    if (scaleX <= 0) scaleX = 1.0;
    if (scaleY <= 0) scaleY = 1.0;
    
    int interpolation = cv::INTER_LINEAR;
    switch (interpIdx) {
        case 0: interpolation = cv::INTER_NEAREST; break;
        case 1: interpolation = cv::INTER_LINEAR; break;
        case 2: interpolation = cv::INTER_CUBIC; break;
        case 3: interpolation = cv::INTER_AREA; break;
        default: interpolation = cv::INTER_LINEAR; break;
    }
    
    cv::UMat scaled;
    cv::resize(input, scaled, cv::Size(), scaleX, scaleY, interpolation);
    
    // Crop or pad to original size for consistent output
    cv::UMat output = cv::UMat::zeros(input.size(), input.type());
    int copyWidth = std::min(scaled.cols, input.cols);
    int copyHeight = std::min(scaled.rows, input.rows);
    scaled(cv::Rect(0, 0, copyWidth, copyHeight)).copyTo(output(cv::Rect(0, 0, copyWidth, copyHeight)));
    
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat ScaleTransformAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double scaleX = getParameter("scaleX").toDouble();
    double scaleY = getParameter("scaleY").toDouble();
    int interpIdx = getParameter("interpolation").toInt();

    if (scaleX <= 0) scaleX = 1.0;
    if (scaleY <= 0) scaleY = 1.0;

    int interpolation = cv::INTER_LINEAR;
    switch (interpIdx) {
        case 0: interpolation = cv::INTER_NEAREST; break;
        case 1: interpolation = cv::INTER_LINEAR; break;
        case 2: interpolation = cv::INTER_CUBIC; break;
        case 3: interpolation = cv::INTER_AREA; break;
        default: interpolation = cv::INTER_LINEAR; break;
    }

    cv::cuda::GpuMat scaled;
    cv::cuda::resize(input, scaled, cv::Size(), scaleX, scaleY, interpolation, stream);

    // Crop or pad to original size
    cv::cuda::GpuMat output(input.size(), input.type());
    output.setTo(cv::Scalar(0), stream);
    int copyWidth = std::min(scaled.cols, input.cols);
    int copyHeight = std::min(scaled.rows, input.rows);
    scaled(cv::Rect(0, 0, copyWidth, copyHeight)).copyTo(output(cv::Rect(0, 0, copyWidth, copyHeight)), stream);

    return output;
}
#endif

// Flip Transform
FlipTransformAlgorithm::FlipTransformAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["flipMode"] = 0;
}

AlgorithmInfo FlipTransformAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "geometry.flip";
    info.name = QCoreApplication::translate("Algorithms", "翻转变换");
    info.category = QCoreApplication::translate("Algorithms", "几何变换");
    info.description = QCoreApplication::translate("Algorithms", "水平或垂直翻转图像");
    
    info.parameters.append(AlgorithmParameter(
        "flipMode",
        QCoreApplication::translate("Algorithms", "翻转方式"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "水平翻转")
                      << QCoreApplication::translate("Algorithms", "垂直翻转")
                      << QCoreApplication::translate("Algorithms", "双向翻转"),
        QCoreApplication::translate("Algorithms", "选择翻转的方向")
    ));
    
    return info;
}

cv::UMat FlipTransformAlgorithm::processImpl(const cv::UMat &input)
{
    int flipMode = getParameter("flipMode").toInt();
    
    // OpenCV flip codes: 0 = vertical, 1 = horizontal, -1 = both
    int flipCode = 1; // Default horizontal
    switch (flipMode) {
        case 0: flipCode = 1; break;  // Horizontal
        case 1: flipCode = 0; break;  // Vertical
        case 2: flipCode = -1; break; // Both
        default: flipCode = 1; break;
    }
    
    cv::UMat output;
    cv::flip(input, output, flipCode);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat FlipTransformAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int flipMode = getParameter("flipMode").toInt();

    int flipCode = 1;
    switch (flipMode) {
        case 0: flipCode = 1; break;
        case 1: flipCode = 0; break;
        case 2: flipCode = -1; break;
        default: flipCode = 1; break;
    }

    cv::cuda::GpuMat output;
    cv::cuda::flip(input, output, flipCode, stream);
    return output;
}
#endif

// Perspective Transform
PerspectiveTransformAlgorithm::PerspectiveTransformAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    // Use percentage-based offsets for the four corners
    m_parameters["topLeftX"] = 0.0;
    m_parameters["topLeftY"] = 0.0;
    m_parameters["topRightX"] = 0.0;
    m_parameters["topRightY"] = 0.0;
    m_parameters["bottomLeftX"] = 0.0;
    m_parameters["bottomLeftY"] = 0.0;
    m_parameters["bottomRightX"] = 0.0;
    m_parameters["bottomRightY"] = 0.0;
}

AlgorithmInfo PerspectiveTransformAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "geometry.perspective";
    info.name = QCoreApplication::translate("Algorithms", "透视变换");
    info.category = QCoreApplication::translate("Algorithms", "几何变换");
    info.description = QCoreApplication::translate("Algorithms", "通过调整四个角点进行透视变换");
    
    info.parameters.append(AlgorithmParameter(
        "topLeftX",
        QCoreApplication::translate("Algorithms", "左上X偏移%"),
        "double", 0.0, -50.0, 50.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "左上角X方向偏移百分比")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "topLeftY",
        QCoreApplication::translate("Algorithms", "左上Y偏移%"),
        "double", 0.0, -50.0, 50.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "左上角Y方向偏移百分比")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "topRightX",
        QCoreApplication::translate("Algorithms", "右上X偏移%"),
        "double", 0.0, -50.0, 50.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "右上角X方向偏移百分比")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "topRightY",
        QCoreApplication::translate("Algorithms", "右上Y偏移%"),
        "double", 0.0, -50.0, 50.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "右上角Y方向偏移百分比")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "bottomLeftX",
        QCoreApplication::translate("Algorithms", "左下X偏移%"),
        "double", 0.0, -50.0, 50.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "左下角X方向偏移百分比")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "bottomLeftY",
        QCoreApplication::translate("Algorithms", "左下Y偏移%"),
        "double", 0.0, -50.0, 50.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "左下角Y方向偏移百分比")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "bottomRightX",
        QCoreApplication::translate("Algorithms", "右下X偏移%"),
        "double", 0.0, -50.0, 50.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "右下角X方向偏移百分比")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "bottomRightY",
        QCoreApplication::translate("Algorithms", "右下Y偏移%"),
        "double", 0.0, -50.0, 50.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "右下角Y方向偏移百分比")
    ));
    
    return info;
}

cv::UMat PerspectiveTransformAlgorithm::processImpl(const cv::UMat &input)
{
    double topLeftX = getParameter("topLeftX").toDouble();
    double topLeftY = getParameter("topLeftY").toDouble();
    double topRightX = getParameter("topRightX").toDouble();
    double topRightY = getParameter("topRightY").toDouble();
    double bottomLeftX = getParameter("bottomLeftX").toDouble();
    double bottomLeftY = getParameter("bottomLeftY").toDouble();
    double bottomRightX = getParameter("bottomRightX").toDouble();
    double bottomRightY = getParameter("bottomRightY").toDouble();
    
    float w = static_cast<float>(input.cols);
    float h = static_cast<float>(input.rows);
    
    auto [srcPoints, dstPoints] = calculatePerspectivePoints(w, h,
        topLeftX, topLeftY, topRightX, topRightY,
        bottomLeftX, bottomLeftY, bottomRightX, bottomRightY);
    
    cv::Mat perspMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);
    
    cv::UMat output;
    cv::warpPerspective(input, output, perspMatrix, input.size());
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat PerspectiveTransformAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double topLeftX = getParameter("topLeftX").toDouble();
    double topLeftY = getParameter("topLeftY").toDouble();
    double topRightX = getParameter("topRightX").toDouble();
    double topRightY = getParameter("topRightY").toDouble();
    double bottomLeftX = getParameter("bottomLeftX").toDouble();
    double bottomLeftY = getParameter("bottomLeftY").toDouble();
    double bottomRightX = getParameter("bottomRightX").toDouble();
    double bottomRightY = getParameter("bottomRightY").toDouble();

    float w = static_cast<float>(input.cols);
    float h = static_cast<float>(input.rows);

    auto [srcPoints, dstPoints] = calculatePerspectivePoints(w, h,
        topLeftX, topLeftY, topRightX, topRightY,
        bottomLeftX, bottomLeftY, bottomRightX, bottomRightY);

    cv::Mat perspMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);

    cv::cuda::GpuMat output;
    cv::cuda::warpPerspective(input, output, perspMatrix, input.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(), stream);
    return output;
}
#endif

// Register geometry algorithms
void registerGeometryAlgorithms()
{
    ImageAlgorithmFactory::instance().registerAlgorithm<RotateTransformAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<TranslateTransformAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<ScaleTransformAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<FlipTransformAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<PerspectiveTransformAlgorithm>();
}
