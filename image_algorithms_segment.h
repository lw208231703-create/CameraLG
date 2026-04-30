#ifndef IMAGE_ALGORITHMS_SEGMENT_H
#define IMAGE_ALGORITHMS_SEGMENT_H

#include "image_algorithm_base.h"

/**
 * @brief Color Range Segmentation Algorithm
 */
class ColorRangeSegmentAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit ColorRangeSegmentAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    bool supportsCuda() const override { return true; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif
};

/**
 * @brief K-Means Clustering Segmentation Algorithm
 */
class KMeansSegmentAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit KMeansSegmentAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Image Inpainting Algorithm
 * Note: CUDA inpainting is not available in OpenCV, so this uses CPU/OpenCL only
 */
class InpaintAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit InpaintAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    bool supportsCuda() const override { return false; } // No CUDA inpaint in OpenCV
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Contour Detection Algorithm
 */
class ContourDetectionAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit ContourDetectionAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Watershed Segmentation Algorithm
 */
class WatershedAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit WatershedAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Register all segmentation algorithms
 */
void registerSegmentAlgorithms();

#endif // IMAGE_ALGORITHMS_SEGMENT_H
