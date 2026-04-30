#include "image_algorithms_edge.h"
#include <QCoreApplication>

// Canny Edge Detection
CannyEdgeAlgorithm::CannyEdgeAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["threshold1"] = 50.0;
    m_parameters["threshold2"] = 150.0;
    m_parameters["apertureSize"] = 3;
}

AlgorithmInfo CannyEdgeAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "edge.canny";
    info.name = QCoreApplication::translate("Algorithms", "Canny边缘检测");
    info.category = QCoreApplication::translate("Algorithms", "边缘检测");
    info.description = QCoreApplication::translate("Algorithms", "经典的Canny边缘检测算法");
    
    info.parameters.append(AlgorithmParameter(
        "threshold1",
        QCoreApplication::translate("Algorithms", "低阈值"),
        "double", 50.0, 0.0, 65535.0, 10.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "滞后过程的第一个阈值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "threshold2",
        QCoreApplication::translate("Algorithms", "高阈值"),
        "double", 150.0, 0.0, 65535.0, 10.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "滞后过程的第二个阈值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "apertureSize",
        QCoreApplication::translate("Algorithms", "Sobel算子大小"),
        "int", 3, 3, 7, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "Sobel算子的孔径大小 (3, 5, 或 7)")
    ));
    
    return info;
}

cv::UMat CannyEdgeAlgorithm::processImpl(const cv::UMat &input)
{
    double threshold1 = getParameter("threshold1").toDouble();
    double threshold2 = getParameter("threshold2").toDouble();
    int apertureSize = getParameter("apertureSize").toInt();
    
    // Ensure aperture size is valid (3, 5, or 7)
    if (apertureSize != 3 && apertureSize != 5 && apertureSize != 7) {
        apertureSize = 3;
    }
    
    cv::UMat output;
    cv::Canny(input, output, threshold1, threshold2, apertureSize);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat CannyEdgeAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double threshold1 = getParameter("threshold1").toDouble();
    double threshold2 = getParameter("threshold2").toDouble();
    int apertureSize = getParameter("apertureSize").toInt();

    // Ensure aperture size is valid (3, 5, or 7)
    if (apertureSize != 3 && apertureSize != 5 && apertureSize != 7) {
        apertureSize = 3;
    }

    cv::cuda::GpuMat output;
    cv::Ptr<cv::cuda::CannyEdgeDetector> detector = cv::cuda::createCannyEdgeDetector(threshold1, threshold2, apertureSize);
    detector->detect(input, output, stream);

    return output;
}
#endif

// Sobel Edge Detection
SobelEdgeAlgorithm::SobelEdgeAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["dx"] = 1;
    m_parameters["dy"] = 1;
    m_parameters["ksize"] = 3;
    m_parameters["scale"] = 1.0;
    m_parameters["delta"] = 0.0;
}

AlgorithmInfo SobelEdgeAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "edge.sobel";
    info.name = QCoreApplication::translate("Algorithms", "Sobel边缘检测");
    info.category = QCoreApplication::translate("Algorithms", "边缘检测");
    info.description = QCoreApplication::translate("Algorithms", "使用Sobel算子计算图像的一阶导数");
    
    info.parameters.append(AlgorithmParameter(
        "dx",
        QCoreApplication::translate("Algorithms", "X方向阶数"),
        "int", 1, 0, 2, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "x方向的导数阶数")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "dy",
        QCoreApplication::translate("Algorithms", "Y方向阶数"),
        "int", 1, 0, 2, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "y方向的导数阶数")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "ksize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 3, 1, 7, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "扩展Sobel核的大小 (1, 3, 5, 或 7)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "scale",
        QCoreApplication::translate("Algorithms", "缩放因子"),
        "double", 1.0, 0.1, 10.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "计算导数值的可选缩放因子")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "delta",
        QCoreApplication::translate("Algorithms", "偏移值"),
        "double", 0.0, -128.0, 128.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "添加到结果的可选偏移值")
    ));
    
    return info;
}

cv::UMat SobelEdgeAlgorithm::processImpl(const cv::UMat &input)
{
    int dx = getParameter("dx").toInt();
    int dy = getParameter("dy").toInt();
    int ksize = getParameter("ksize").toInt();
    double scale = getParameter("scale").toDouble();
    double delta = getParameter("delta").toDouble();
    
    // Ensure valid ksize
    if (ksize != 1 && ksize != 3 && ksize != 5 && ksize != 7) {
        ksize = 3;
    }
    
    // At least one direction must be non-zero
    if (dx == 0 && dy == 0) {
        dx = 1;
    }
    
    cv::UMat gradX, gradY, output;

    const bool isFloatInput = (input.depth() == CV_32F);
    if (isFloatInput) {
        // Float path: compute gradients in CV_32F and normalize to 8-bit for display.
        if (dx > 0 && dy > 0) {
            cv::Sobel(input, gradX, CV_32F, dx, 0, ksize, scale, delta);
            cv::Sobel(input, gradY, CV_32F, 0, dy, ksize, scale, delta);

            cv::UMat mag;
            cv::magnitude(gradX, gradY, mag);

            // Use saturation instead of normalization to match integer behavior
            // This ensures that weak edges are preserved and strong edges are saturated,
            // rather than scaling everything down based on the strongest edge.
            mag.convertTo(output, CV_8UC1);
        } else {
            cv::UMat gradTemp;
            cv::Sobel(input, gradTemp, CV_32F, dx, dy, ksize, scale, delta);

            cv::UMat absGrad;
            cv::absdiff(gradTemp, cv::Scalar::all(0), absGrad);

            // Use saturation instead of normalization
            absGrad.convertTo(output, CV_8UC1);
        }
        return output;
    }

    // Legacy path (expects CV_8U input)
    if (dx > 0 && dy > 0) {
        // Compute both gradients and combine
        cv::Sobel(input, gradX, CV_16S, dx, 0, ksize, scale, delta);
        cv::Sobel(input, gradY, CV_16S, 0, dy, ksize, scale, delta);

        cv::UMat absGradX, absGradY;
        cv::convertScaleAbs(gradX, absGradX);
        cv::convertScaleAbs(gradY, absGradY);

        cv::addWeighted(absGradX, 0.5, absGradY, 0.5, 0, output);
    } else {
        cv::UMat gradTemp;
        cv::Sobel(input, gradTemp, CV_16S, dx, dy, ksize, scale, delta);
        cv::convertScaleAbs(gradTemp, output);
    }

    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat SobelEdgeAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int dx = getParameter("dx").toInt();
    int dy = getParameter("dy").toInt();
    int ksize = getParameter("ksize").toInt();
    double scale = getParameter("scale").toDouble();
    double delta = getParameter("delta").toDouble();

    // Ensure valid ksize
    if (ksize != 1 && ksize != 3 && ksize != 5 && ksize != 7) {
        ksize = 3;
    }

    // At least one direction must be non-zero
    if (dx == 0 && dy == 0) {
        dx = 1;
    }

    cv::cuda::GpuMat output;

    // Use CV_32F for calculation to handle negative values and avoid overflow/wrapping
    cv::cuda::GpuMat inputFloat;
    if (input.type() != CV_32F) {
        input.convertTo(inputFloat, CV_32F, stream);
    } else {
        inputFloat = input;
    }

    if (dx > 0 && dy > 0) {
        // Compute both gradients and combine
        cv::cuda::GpuMat gradX, gradY;
        cv::Ptr<cv::cuda::Filter> filterX = cv::cuda::createSobelFilter(CV_32F, CV_32F, dx, 0, ksize, scale);
        cv::Ptr<cv::cuda::Filter> filterY = cv::cuda::createSobelFilter(CV_32F, CV_32F, 0, dy, ksize, scale);

        filterX->apply(inputFloat, gradX, stream);
        filterY->apply(inputFloat, gradY, stream);

        cv::cuda::GpuMat absGradX, absGradY;
        cv::cuda::abs(gradX, absGradX, stream);
        cv::cuda::abs(gradY, absGradY, stream);

        cv::cuda::GpuMat absGradX8, absGradY8;
        absGradX.convertTo(absGradX8, CV_8U, 1.0, delta, stream);
        absGradY.convertTo(absGradY8, CV_8U, 1.0, delta, stream);

        cv::cuda::addWeighted(absGradX8, 0.5, absGradY8, 0.5, 0, output, -1, stream);
    } else {
        cv::cuda::GpuMat gradTemp;
        cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createSobelFilter(CV_32F, CV_32F, dx, dy, ksize, scale);
        filter->apply(inputFloat, gradTemp, stream);

        cv::cuda::GpuMat absGrad;
        cv::cuda::abs(gradTemp, absGrad, stream);
        absGrad.convertTo(output, CV_8U, 1.0, delta, stream);
    }

    return output;
}
#endif

// Laplacian Edge Detection
LaplacianEdgeAlgorithm::LaplacianEdgeAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["ksize"] = 3;
    m_parameters["scale"] = 1.0;
    m_parameters["delta"] = 0.0;
}

AlgorithmInfo LaplacianEdgeAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "edge.laplacian";
    info.name = QCoreApplication::translate("Algorithms", "拉普拉斯边缘检测");
    info.category = QCoreApplication::translate("Algorithms", "边缘检测");
    info.description = QCoreApplication::translate("Algorithms", "使用拉普拉斯算子计算图像的二阶导数");
    
    info.parameters.append(AlgorithmParameter(
        "ksize",
        QCoreApplication::translate("Algorithms", "核大小"),
        "int", 3, 1, 31, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "用于计算二阶导数滤波器的孔径大小 (正奇数)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "scale",
        QCoreApplication::translate("Algorithms", "缩放因子"),
        "double", 1.0, 0.1, 10.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "计算拉普拉斯值的可选缩放因子")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "delta",
        QCoreApplication::translate("Algorithms", "偏移值"),
        "double", 0.0, -128.0, 128.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "添加到结果的可选偏移值")
    ));
    
    return info;
}

cv::UMat LaplacianEdgeAlgorithm::processImpl(const cv::UMat &input)
{
    int ksize = getParameter("ksize").toInt();
    double scale = getParameter("scale").toDouble();
    double delta = getParameter("delta").toDouble();
    
    // Ensure ksize is odd and positive
    if (ksize % 2 == 0) ksize++;
    if (ksize < 1) ksize = 1;
    
    const bool isFloatInput = (input.depth() == CV_32F);
    cv::UMat gradTemp, output;

    if (isFloatInput) {
        cv::Laplacian(input, gradTemp, CV_32F, ksize, scale, delta);

        cv::UMat absGrad;
        cv::absdiff(gradTemp, cv::Scalar::all(0), absGrad);

        // Use saturation instead of normalization to match integer behavior
        absGrad.convertTo(output, CV_8UC1);
        return output;
    }

    cv::Laplacian(input, gradTemp, CV_16S, ksize, scale, delta);
    cv::convertScaleAbs(gradTemp, output);

    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat LaplacianEdgeAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int ksize = getParameter("ksize").toInt();
    double scale = getParameter("scale").toDouble();
    double delta = getParameter("delta").toDouble();

    // Ensure ksize is odd and positive
    if (ksize % 2 == 0) ksize++;
    if (ksize < 1) ksize = 1;

    cv::cuda::GpuMat gradTemp, output;

    // Use CV_32F for calculation to handle negative values and avoid overflow/wrapping
    // especially for CV_16U input where negative Laplacian values would wrap to large positive values.
    cv::cuda::GpuMat inputFloat;
    if (input.type() != CV_32F) {
        input.convertTo(inputFloat, CV_32F, stream);
    } else {
        inputFloat = input;
    }

    // CUDA filters require srcType == dstType
    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createLaplacianFilter(CV_32F, CV_32F, ksize, scale);
    filter->apply(inputFloat, gradTemp, stream);

    // Take absolute value to handle negative edges
    cv::cuda::GpuMat absGrad;
    cv::cuda::abs(gradTemp, absGrad, stream);

    // Convert to 8-bit for display with delta offset
    absGrad.convertTo(output, CV_8U, 1.0, delta, stream);

    return output;
}
#endif

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat ScharrEdgeAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int dx = getParameter("dx").toInt();
    int dy = getParameter("dy").toInt();
    double scale = getParameter("scale").toDouble();
    double delta = getParameter("delta").toDouble();

    // Scharr requires exactly one of dx or dy to be 1
    if (dx + dy != 1) {
        dx = 1;
        dy = 0;
    }

    cv::cuda::GpuMat gradTemp, output;

    // Use CV_32F for calculation to handle negative values and avoid overflow/wrapping
    cv::cuda::GpuMat inputFloat;
    if (input.type() != CV_32F) {
        input.convertTo(inputFloat, CV_32F, stream);
    } else {
        inputFloat = input;
    }

    cv::Ptr<cv::cuda::Filter> filter = cv::cuda::createScharrFilter(CV_32F, CV_32F, dx, dy, scale);
    filter->apply(inputFloat, gradTemp, stream);

    cv::cuda::GpuMat absGrad;
    cv::cuda::abs(gradTemp, absGrad, stream);
    absGrad.convertTo(output, CV_8U, 1.0, delta, stream);

    return output;
}
#endif

// Scharr Edge Detection
ScharrEdgeAlgorithm::ScharrEdgeAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["dx"] = 1;
    m_parameters["dy"] = 0;
    m_parameters["scale"] = 1.0;
    m_parameters["delta"] = 0.0;
}

AlgorithmInfo ScharrEdgeAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "edge.scharr";
    info.name = QCoreApplication::translate("Algorithms", "Scharr边缘检测");
    info.category = QCoreApplication::translate("Algorithms", "边缘检测");
    info.description = QCoreApplication::translate("Algorithms", "使用Scharr算子计算图像的一阶x或y导数");
    
    info.parameters.append(AlgorithmParameter(
        "dx",
        QCoreApplication::translate("Algorithms", "X方向阶数"),
        "int", 1, 0, 1, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "x方向的导数阶数 (0或1)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "dy",
        QCoreApplication::translate("Algorithms", "Y方向阶数"),
        "int", 0, 0, 1, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "y方向的导数阶数 (0或1)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "scale",
        QCoreApplication::translate("Algorithms", "缩放因子"),
        "double", 1.0, 0.1, 10.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "计算导数值的可选缩放因子")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "delta",
        QCoreApplication::translate("Algorithms", "偏移值"),
        "double", 0.0, -128.0, 128.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "添加到结果的可选偏移值")
    ));
    
    return info;
}

cv::UMat ScharrEdgeAlgorithm::processImpl(const cv::UMat &input)
{
    int dx = getParameter("dx").toInt();
    int dy = getParameter("dy").toInt();
    double scale = getParameter("scale").toDouble();
    double delta = getParameter("delta").toDouble();
    
    // Scharr requires exactly one of dx or dy to be 1
    if ((dx == 0 && dy == 0) || (dx > 0 && dy > 0)) {
        dx = 1;
        dy = 0;
    }
    
    const bool isFloatInput = (input.depth() == CV_32F);
    cv::UMat gradTemp, output;

    if (isFloatInput) {
        cv::Scharr(input, gradTemp, CV_32F, dx, dy, scale, delta);

        cv::UMat absGrad;
        cv::absdiff(gradTemp, cv::Scalar::all(0), absGrad);

        // Use saturation instead of normalization to match integer behavior
        absGrad.convertTo(output, CV_8UC1);
        return output;
    }

    cv::Scharr(input, gradTemp, CV_16S, dx, dy, scale, delta);
    cv::convertScaleAbs(gradTemp, output);

    return output;
}

// Register edge algorithms
void registerEdgeAlgorithms()
{
    ImageAlgorithmFactory::instance().registerAlgorithm<CannyEdgeAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<SobelEdgeAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<LaplacianEdgeAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<ScharrEdgeAlgorithm>();
}
