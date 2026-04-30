#include "image_algorithms_segment.h"
#include <QCoreApplication>
#include <cmath>
#include <opencv2/photo.hpp>

// Color Range Segmentation
ColorRangeSegmentAlgorithm::ColorRangeSegmentAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["lowThreshold"] = 100;
    m_parameters["highThreshold"] = 200;
}

AlgorithmInfo ColorRangeSegmentAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "segment.color_range";
    info.name = QCoreApplication::translate("Algorithms", "灰度范围分割");
    info.category = QCoreApplication::translate("Algorithms", "图像分割");
    info.description = QCoreApplication::translate("Algorithms", "根据灰度值范围分割图像");
    
    info.parameters.append(AlgorithmParameter(
        "lowThreshold",
        QCoreApplication::translate("Algorithms", "下限阈值"),
        "int", 100, 0, 65535, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "分割的灰度值下限")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "highThreshold",
        QCoreApplication::translate("Algorithms", "上限阈值"),
        "int", 200, 0, 65535, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "分割的灰度值上限")
    ));
    
    return info;
}

cv::UMat ColorRangeSegmentAlgorithm::processImpl(const cv::UMat &input)
{
    int lowThreshold = getParameter("lowThreshold").toInt();
    int highThreshold = getParameter("highThreshold").toInt();
    
    if (lowThreshold < 0) lowThreshold = 0;
    if (highThreshold > 65535) highThreshold = 65535;
    if (lowThreshold > highThreshold) std::swap(lowThreshold, highThreshold);
    
    cv::UMat mask;
    cv::inRange(input, cv::Scalar(lowThreshold), cv::Scalar(highThreshold), mask);
    
    cv::UMat output;
    input.copyTo(output, mask);
    return output;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat ColorRangeSegmentAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    int lowThreshold = getParameter("lowThreshold").toInt();
    int highThreshold = getParameter("highThreshold").toInt();
    
    if (lowThreshold < 0) lowThreshold = 0;
    if (highThreshold > 65535) highThreshold = 65535;
    if (lowThreshold > highThreshold) std::swap(lowThreshold, highThreshold);
    
    cv::cuda::GpuMat mask;
    cv::cuda::inRange(input, cv::Scalar(lowThreshold), cv::Scalar(highThreshold), mask, stream);
    
    cv::cuda::GpuMat output;
    input.copyTo(output, mask, stream);
    return output;
}
#endif

// K-Means Segmentation
KMeansSegmentAlgorithm::KMeansSegmentAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["clusters"] = 3;
    m_parameters["iterations"] = 10;
    m_parameters["attempts"] = 3;
    m_parameters["epsilon"] = 0.1;
    m_parameters["maxProcessPixels"] = 307200;  // 640*480 = ~300K
}

AlgorithmInfo KMeansSegmentAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "segment.kmeans";
    info.name = QCoreApplication::translate("Algorithms", "K-Means聚类分割");
    info.category = QCoreApplication::translate("Algorithms", "图像分割");
    info.description = QCoreApplication::translate("Algorithms", "使用K-Means聚类算法分割图像");
    
    info.parameters.append(AlgorithmParameter(
        "clusters",
        QCoreApplication::translate("Algorithms", "聚类数量"),
        "int", 3, 2, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "分割成的类别数量")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "iterations",
        QCoreApplication::translate("Algorithms", "迭代次数"),
        "int", 10, 1, 100, 5,
        QStringList(),
        QCoreApplication::translate("Algorithms", "K-Means算法的最大迭代次数")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "attempts",
        QCoreApplication::translate("Algorithms", "尝试次数"),
        "int", 3, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "使用不同初始中心执行算法的次数")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "epsilon",
        QCoreApplication::translate("Algorithms", "收敛精度"),
        "double", 0.1, 0.001, 1.0, 0.01,
        QStringList(),
        QCoreApplication::translate("Algorithms", "迭代终止的精度阈值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "maxProcessPixels",
        QCoreApplication::translate("Algorithms", "最大处理像素数"),
        "int", 307200, 0, 5000000, 50000,
        QStringList(),
        QCoreApplication::translate("Algorithms", "降采样阈值，超过此像素数将缩小图像处理以提高速度 (0=不降采样)")
    ));
    
    return info;
}

cv::UMat KMeansSegmentAlgorithm::processImpl(const cv::UMat &input)
{
    int clusters = getParameter("clusters").toInt();
    int iterations = getParameter("iterations").toInt();
    int attempts = getParameter("attempts").toInt();
    double epsilon = getParameter("epsilon").toDouble();
    
    if (clusters < 2) clusters = 3;
    if (iterations < 1) iterations = 10;
    if (attempts < 1) attempts = 3;
    if (epsilon <= 0) epsilon = 0.1;
    
    int maxProcessPixels = getParameter("maxProcessPixels").toInt();
    if (maxProcessPixels < 0) maxProcessPixels = 307200;
    
    cv::Mat inputMat = input.getMat(cv::ACCESS_READ);
    
    // ===================== OPTIMIZATION: Downsampling =====================
    // For large images, downsample to compute cluster centers faster
    // Then apply centers to original resolution image
    int totalPixels = inputMat.rows * inputMat.cols;
    double scale = 1.0;
    cv::Mat workingMat = inputMat;
    
    if (maxProcessPixels > 0 && totalPixels > maxProcessPixels) {
        scale = std::sqrt(static_cast<double>(maxProcessPixels) / totalPixels);
        cv::resize(inputMat, workingMat, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    
    // Reshape image to be a list of pixels
    cv::Mat samples = workingMat.reshape(1, workingMat.rows * workingMat.cols);
    samples.convertTo(samples, CV_32F);
    
    // Apply K-Means using optimized termination criteria
    cv::Mat labels, centers;
    cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, iterations, epsilon);
    
    cv::kmeans(samples, clusters, labels,
               criteria, attempts, cv::KMEANS_PP_CENTERS, centers);
    
    // ===================== Apply to full resolution =====================
    // Create lookup table (LUT) with 256 entries
    cv::Mat lut = cv::Mat::zeros(1, 256, CV_8U);
    
    // Populate LUT: each grayscale value maps to nearest cluster center
    std::vector<float> centerValues(clusters);
    for (int i = 0; i < clusters; i++) {
        centerValues[i] = centers.at<float>(i, 0);
    }
    
    // For each possible grayscale value, find nearest cluster
    for (int v = 0; v < 256; v++) {
        float minDist = std::numeric_limits<float>::max();
        int bestCluster = 0;
        for (int c = 0; c < clusters; c++) {
            float dist = std::abs(static_cast<float>(v) - centerValues[c]);
            if (dist < minDist) {
                minDist = dist;
                bestCluster = c;
            }
        }
        lut.at<uchar>(v) = static_cast<uchar>(centerValues[bestCluster]);
    }
    
    // Apply LUT to original full-resolution image (very fast)
    cv::Mat segmented;
    cv::LUT(inputMat, lut, segmented);
    
    cv::UMat output;
    segmented.copyTo(output);
    return output;
}

// Inpaint Algorithm
InpaintAlgorithm::InpaintAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["threshold"] = 250;
    m_parameters["radius"] = 3.0;
    m_parameters["method"] = 0;
    m_parameters["dilationSize"] = 3;
    m_parameters["maxDimension"] = 800;
}

AlgorithmInfo InpaintAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "segment.inpaint";
    info.name = QCoreApplication::translate("Algorithms", "图像修复");
    info.category = QCoreApplication::translate("Algorithms", "图像分割");
    info.description = QCoreApplication::translate("Algorithms", "修复图像中的高亮区域（如过曝点）");
    
    info.parameters.append(AlgorithmParameter(
        "threshold",
        QCoreApplication::translate("Algorithms", "亮度阈值"),
        "int", 250, 0, 65535, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "高于此阈值的像素将被修复")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "radius",
        QCoreApplication::translate("Algorithms", "修复半径"),
        "double", 3.0, 1.0, 20.0, 1.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "修复算法考虑的邻域半径")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "method",
        QCoreApplication::translate("Algorithms", "修复方法"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "Telea")
                      << QCoreApplication::translate("Algorithms", "Navier-Stokes"),
        QCoreApplication::translate("Algorithms", "选择修复算法")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "dilationSize",
        QCoreApplication::translate("Algorithms", "膨胀核大小"),
        "int", 3, 1, 15, 2,
        QStringList(),
        QCoreApplication::translate("Algorithms", "掩膜膨胀的结构元素大小")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "maxDimension",
        QCoreApplication::translate("Algorithms", "最大处理尺寸"),
        "int", 800, 0, 4096, 100,
        QStringList(),
        QCoreApplication::translate("Algorithms", "降采样阈值，超过此尺寸将缩小图像处理以提高速度 (0=不降采样)")
    ));
    
    return info;
}

cv::UMat InpaintAlgorithm::processImpl(const cv::UMat &input)
{
    int threshold = getParameter("threshold").toInt();
    double radius = getParameter("radius").toDouble();
    int methodIdx = getParameter("method").toInt();
    int dilationSize = getParameter("dilationSize").toInt();
    
    if (threshold < 0) threshold = 250;
    if (radius < 1.0) radius = 3.0;
    if (dilationSize < 1) dilationSize = 3;
    
    int inpaintMethod = methodIdx == 0 ? cv::INPAINT_TELEA : cv::INPAINT_NS;
    int maxDimension = getParameter("maxDimension").toInt();
    if (maxDimension < 0) maxDimension = 800;
    
    cv::Mat inputMat = input.getMat(cv::ACCESS_READ);
    
    // ===================== OPTIMIZATION: Downsampling =====================
    // Inpainting is O(n^2) per pixel, so downsample for large images
    double scale = 1.0;
    cv::Mat workingMat = inputMat;
    
    int maxDim = std::max(inputMat.rows, inputMat.cols);
    if (maxDimension > 0 && maxDim > maxDimension) {
        scale = static_cast<double>(maxDimension) / maxDim;
        cv::resize(inputMat, workingMat, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    
    // Create mask from high intensity pixels (use OpenCL via UMat)
    cv::UMat workingUMat;
    workingMat.copyTo(workingUMat);
    
    cv::UMat maskUMat;
    cv::threshold(workingUMat, maskUMat, threshold, 255, cv::THRESH_BINARY);
    
    // Dilate mask slightly to cover edges (OpenCL accelerated)
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(dilationSize, dilationSize));
    cv::dilate(maskUMat, maskUMat, kernel);
    
    // Convert back to Mat for inpaint (inpaint doesn't support UMat)
    cv::Mat mask = maskUMat.getMat(cv::ACCESS_READ).clone();
    
    // Inpaint at reduced resolution
    cv::Mat result;
    cv::inpaint(workingMat, mask, result, radius, inpaintMethod);
    
    // ===================== Upscale result =====================
    if (scale < 1.0) {
        // Blend: use inpainted result for masked areas, original for rest
        cv::Mat upscaledResult;
        cv::resize(result, upscaledResult, inputMat.size(), 0, 0, cv::INTER_CUBIC);
        
        // Create full-resolution mask
        cv::Mat fullMask;
        cv::threshold(inputMat, fullMask, threshold, 255, cv::THRESH_BINARY);
        cv::dilate(fullMask, fullMask, kernel);
        
        // Blend: inpainted areas from upscaled, rest from original
        inputMat.copyTo(result);
        upscaledResult.copyTo(result, fullMask);
    }
    
    cv::UMat output;
    result.copyTo(output);
    return output;
}

// Contour Detection
ContourDetectionAlgorithm::ContourDetectionAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["threshold"] = 128;
    m_parameters["minArea"] = 100;
    m_parameters["showContours"] = true;
    m_parameters["lineThickness"] = 1;
    m_parameters["maxDimension"] = 1024;
}

AlgorithmInfo ContourDetectionAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "segment.contours";
    info.name = QCoreApplication::translate("Algorithms", "轮廓检测");
    info.category = QCoreApplication::translate("Algorithms", "图像分割");
    info.description = QCoreApplication::translate("Algorithms", "检测并绘制图像中的轮廓");
    
    info.parameters.append(AlgorithmParameter(
        "threshold",
        QCoreApplication::translate("Algorithms", "二值化阈值"),
        "int", 128, 0, 65535, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "用于检测轮廓的二值化阈值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "minArea",
        QCoreApplication::translate("Algorithms", "最小面积"),
        "int", 100, 0, 10000, 50,
        QStringList(),
        QCoreApplication::translate("Algorithms", "过滤掉小于此面积的轮廓")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "showContours",
        QCoreApplication::translate("Algorithms", "显示轮廓"),
        "bool", true,
        QVariant(), QVariant(), QVariant(),
        QStringList(),
        QCoreApplication::translate("Algorithms", "是否在原图上绘制轮廓")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "lineThickness",
        QCoreApplication::translate("Algorithms", "线条粗细"),
        "int", 1, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "轮廓线条的粗细")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "maxDimension",
        QCoreApplication::translate("Algorithms", "最大处理尺寸"),
        "int", 1024, 0, 4096, 128,
        QStringList(),
        QCoreApplication::translate("Algorithms", "降采样阈值，超过此尺寸将缩小图像处理以提高速度 (0=不降采样)")
    ));
    
    return info;
}

cv::UMat ContourDetectionAlgorithm::processImpl(const cv::UMat &input)
{
    int threshold = getParameter("threshold").toInt();
    int minArea = getParameter("minArea").toInt();
    bool showContours = getParameter("showContours").toBool();
    int lineThickness = getParameter("lineThickness").toInt();
    
    if (threshold < 0) threshold = 128;
    if (minArea < 0) minArea = 100;
    if (lineThickness < 1) lineThickness = 1;
    
    int maxDimension = getParameter("maxDimension").toInt();
    if (maxDimension < 0) maxDimension = 1024;
    
    // ===================== OPTIMIZATION: Use OpenCL for preprocessing =====================
    // Threshold using UMat (OpenCL accelerated)
    cv::UMat binary;
    cv::threshold(input, binary, threshold, 255, cv::THRESH_BINARY);
    
    // Convert to Mat for findContours (doesn't support UMat)
    cv::Mat binaryMat = binary.getMat(cv::ACCESS_READ).clone();
    
    // ===================== OPTIMIZATION: Downsample for contour detection =====================
    double scale = 1.0;
    cv::Mat workingBinary = binaryMat;
    
    int maxDim = std::max(binaryMat.rows, binaryMat.cols);
    if (maxDimension > 0 && maxDim > maxDimension) {
        scale = static_cast<double>(maxDimension) / maxDim;
        cv::resize(binaryMat, workingBinary, cv::Size(), scale, scale, cv::INTER_NEAREST);
    }
    
    // Find contours on downsampled image
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(workingBinary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // Scale contours back if downsampled
    if (scale < 1.0) {
        double invScale = 1.0 / scale;
        for (auto& contour : contours) {
            for (auto& pt : contour) {
                pt.x = static_cast<int>(pt.x * invScale);
                pt.y = static_cast<int>(pt.y * invScale);
            }
        }
    }
    
    // Adjust minArea for scale
    double scaledMinArea = minArea;  // Already in original resolution
    
    // Create output image
    cv::Mat inputMat = input.getMat(cv::ACCESS_READ);
    cv::Mat result;
    if (showContours) {
        result = inputMat.clone();
    } else {
        result = cv::Mat::zeros(inputMat.size(), CV_8UC1);
    }
    
    // Draw contours (filter by area)
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area >= scaledMinArea) {
            cv::drawContours(result, contours, static_cast<int>(i), cv::Scalar(255), lineThickness);
        }
    }
    
    cv::UMat output;
    result.copyTo(output);
    return output;
}

// Watershed Segmentation
WatershedAlgorithm::WatershedAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["threshold"] = 128;
    m_parameters["distanceType"] = 0;
    m_parameters["foregroundThreshold"] = 0.4;
    m_parameters["dilationIterations"] = 3;
    m_parameters["maxDimension"] = 640;
}

AlgorithmInfo WatershedAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "segment.watershed";
    info.name = QCoreApplication::translate("Algorithms", "分水岭分割");
    info.category = QCoreApplication::translate("Algorithms", "图像分割");
    info.description = QCoreApplication::translate("Algorithms", "使用分水岭算法分割图像");
    
    info.parameters.append(AlgorithmParameter(
        "threshold",
        QCoreApplication::translate("Algorithms", "前景阈值"),
        "int", 128, 0, 65535, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "用于确定前景标记的阈值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "distanceType",
        QCoreApplication::translate("Algorithms", "距离类型"),
        "enum", 0,
        QVariant(), QVariant(), QVariant(),
        QStringList() << QCoreApplication::translate("Algorithms", "L2距离")
                      << QCoreApplication::translate("Algorithms", "L1距离"),
        QCoreApplication::translate("Algorithms", "距离变换使用的距离类型")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "foregroundThreshold",
        QCoreApplication::translate("Algorithms", "前景标记阈值"),
        "double", 0.4, 0.1, 0.9, 0.05,
        QStringList(),
        QCoreApplication::translate("Algorithms", "从距离变换中确定前景标记的阈值")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "dilationIterations",
        QCoreApplication::translate("Algorithms", "膨胀迭代次数"),
        "int", 3, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "背景区域膨胀的迭代次数")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "maxDimension",
        QCoreApplication::translate("Algorithms", "最大处理尺寸"),
        "int", 640, 0, 2048, 64,
        QStringList(),
        QCoreApplication::translate("Algorithms", "降采样阈值，超过此尺寸将缩小图像处理以提高速度 (0=不降采样)")
    ));
    
    return info;
}

cv::UMat WatershedAlgorithm::processImpl(const cv::UMat &input)
{
    int threshold = getParameter("threshold").toInt();
    int distanceTypeIdx = getParameter("distanceType").toInt();
    double foregroundThreshold = getParameter("foregroundThreshold").toDouble();
    int dilationIterations = getParameter("dilationIterations").toInt();
    
    if (threshold < 0) threshold = 128;
    if (foregroundThreshold <= 0 || foregroundThreshold >= 1) foregroundThreshold = 0.4;
    if (dilationIterations < 1) dilationIterations = 3;
    
    int distanceType = distanceTypeIdx == 0 ? cv::DIST_L2 : cv::DIST_L1;
    int maxDimension = getParameter("maxDimension").toInt();
    if (maxDimension < 0) maxDimension = 640;
    
    cv::Mat inputMat = input.getMat(cv::ACCESS_READ);
    
    // ===================== OPTIMIZATION: Downsampling =====================
    // Watershed is computationally expensive, downsample for processing
    double scale = 1.0;
    cv::Mat workingMat = inputMat;
    
    int maxDim = std::max(inputMat.rows, inputMat.cols);
    if (maxDimension > 0 && maxDim > maxDimension) {
        scale = static_cast<double>(maxDimension) / maxDim;
        cv::resize(inputMat, workingMat, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    
    // ===================== Use OpenCL for preprocessing =====================
    cv::UMat workingUMat;
    workingMat.copyTo(workingUMat);
    
    // Threshold to get binary image (OpenCL accelerated)
    cv::UMat binaryUMat;
    cv::threshold(workingUMat, binaryUMat, threshold, 255, cv::THRESH_BINARY);
    
    cv::Mat binary = binaryUMat.getMat(cv::ACCESS_READ).clone();
    
    // Distance transform - use optimal mask size based on distance type
    cv::Mat dist;
    int maskSize = (distanceType == cv::DIST_L2) ? cv::DIST_MASK_5 : cv::DIST_MASK_3;
    cv::distanceTransform(binary, dist, distanceType, maskSize);
    cv::normalize(dist, dist, 0, 1.0, cv::NORM_MINMAX);
    
    // Threshold to get foreground markers
    cv::Mat foreground;
    cv::threshold(dist, foreground, foregroundThreshold, 1.0, cv::THRESH_BINARY);
    foreground.convertTo(foreground, CV_8U, 255);
    
    // Find unknown region (background)
    cv::Mat background;
    cv::dilate(binary, background, cv::Mat(), cv::Point(-1, -1), dilationIterations);
    cv::subtract(background, foreground, background);
    
    // Create markers using connected components
    cv::Mat markers;
    cv::connectedComponents(foreground, markers);
    
    // Increment all markers by 1 so background becomes 1, and mark unknown as 0
    markers = markers + 1;
    markers.setTo(0, background == 255);
    
    // Convert input to 3-channel for watershed
    cv::Mat color3c;
    cv::cvtColor(workingMat, color3c, cv::COLOR_GRAY2BGR);
    
    // Apply watershed
    cv::watershed(color3c, markers);
    
    // ===================== Upscale result =====================
    cv::Mat result;
    if (scale < 1.0) {
        // Create boundary mask at low resolution
        cv::Mat boundaryMask = (markers == -1);
        boundaryMask.convertTo(boundaryMask, CV_8U, 255);
        
        // Upscale boundary mask
        cv::Mat upscaledMask;
        cv::resize(boundaryMask, upscaledMask, inputMat.size(), 0, 0, cv::INTER_NEAREST);
        
        // Dilate slightly to make boundaries visible
        cv::dilate(upscaledMask, upscaledMask, cv::Mat(), cv::Point(-1, -1), 1);
        
        // Apply to original image
        result = inputMat.clone();
        result.setTo(255, upscaledMask > 0);
    } else {
        result = inputMat.clone();
        result.setTo(255, markers == -1);
    }
    
    cv::UMat output;
    result.copyTo(output);
    return output;
}

// Register segmentation algorithms
void registerSegmentAlgorithms()
{
    ImageAlgorithmFactory::instance().registerAlgorithm<ColorRangeSegmentAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<KMeansSegmentAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<InpaintAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<ContourDetectionAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<WatershedAlgorithm>();
}
