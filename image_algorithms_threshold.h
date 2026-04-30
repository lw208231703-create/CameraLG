#ifndef IMAGE_ALGORITHMS_THRESHOLD_H
#define IMAGE_ALGORITHMS_THRESHOLD_H

#include "image_algorithm_base.h"

/**
 * @brief Binary Threshold Algorithm
 */
class BinaryThresholdAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit BinaryThresholdAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Adaptive Threshold Algorithm
 */
class AdaptiveThresholdAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit AdaptiveThresholdAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Otsu Threshold Algorithm
 */
class OtsuThresholdAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit OtsuThresholdAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif


    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Register all threshold algorithms
 */
void registerThresholdAlgorithms();

#endif // IMAGE_ALGORITHMS_THRESHOLD_H
