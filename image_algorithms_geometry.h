#ifndef IMAGE_ALGORITHMS_GEOMETRY_H
#define IMAGE_ALGORITHMS_GEOMETRY_H

#include "image_algorithm_base.h"

/**
 * @brief Rotate Transform Algorithm
 */
class RotateTransformAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit RotateTransformAlgorithm(QObject *parent = nullptr);

    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Translate Transform Algorithm
 */
class TranslateTransformAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit TranslateTransformAlgorithm(QObject *parent = nullptr);

    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Scale Transform Algorithm
 */
class ScaleTransformAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit ScaleTransformAlgorithm(QObject *parent = nullptr);

    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Flip Transform Algorithm
 */
class FlipTransformAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit FlipTransformAlgorithm(QObject *parent = nullptr);

    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Perspective Transform Algorithm
 */
class PerspectiveTransformAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit PerspectiveTransformAlgorithm(QObject *parent = nullptr);

    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Register all geometry transform algorithms
 */
void registerGeometryAlgorithms();

#endif // IMAGE_ALGORITHMS_GEOMETRY_H
