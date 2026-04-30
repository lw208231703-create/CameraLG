#ifndef IMAGE_ALGORITHMS_EDGE_H
#define IMAGE_ALGORITHMS_EDGE_H

#include "image_algorithm_base.h"

/**
 * @brief Canny Edge Detection Algorithm
 */
class CannyEdgeAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit CannyEdgeAlgorithm(QObject *parent = nullptr);

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
 * @brief Sobel Edge Detection Algorithm
 */
class SobelEdgeAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit SobelEdgeAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Laplacian Edge Detection Algorithm
 */
class LaplacianEdgeAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit LaplacianEdgeAlgorithm(QObject *parent = nullptr);
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif


    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Scharr Edge Detection Algorithm
 */
class ScharrEdgeAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit ScharrEdgeAlgorithm(QObject *parent = nullptr);
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif


    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Register all edge detection algorithms
 */
void registerEdgeAlgorithms();

#endif // IMAGE_ALGORITHMS_EDGE_H
