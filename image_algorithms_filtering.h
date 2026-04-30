#ifndef IMAGE_ALGORITHMS_FILTERING_H
#define IMAGE_ALGORITHMS_FILTERING_H

#include "image_algorithm_base.h"

/**
 * @brief Gaussian Blur Algorithm
 * 
 * Applies Gaussian smoothing to reduce noise and details
 */
class GaussianBlurAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit GaussianBlurAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Median Blur Algorithm
 * 
 * Applies median filter to remove salt-and-pepper noise
 */
class MedianBlurAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit MedianBlurAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Bilateral Filter Algorithm
 * 
 * Edge-preserving smoothing filter
 */
class BilateralFilterAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit BilateralFilterAlgorithm(QObject *parent = nullptr);
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif


    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Box Filter Algorithm
 * 
 * Simple averaging filter
 */
class BoxFilterAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit BoxFilterAlgorithm(QObject *parent = nullptr);
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif


    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Register all filtering algorithms
 */
void registerFilteringAlgorithms();

#endif // IMAGE_ALGORITHMS_FILTERING_H
