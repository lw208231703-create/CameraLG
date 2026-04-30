#ifndef IMAGE_ALGORITHMS_MORPHOLOGY_H
#define IMAGE_ALGORITHMS_MORPHOLOGY_H

#include "image_algorithm_base.h"

/**
 * @brief Erosion Algorithm
 */
class ErosionAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit ErosionAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Dilation Algorithm
 */
class DilationAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit DilationAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Morphological Opening Algorithm
 */
class OpeningAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit OpeningAlgorithm(QObject *parent = nullptr);
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif


    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Morphological Closing Algorithm
 */
class ClosingAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit ClosingAlgorithm(QObject *parent = nullptr);
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif


    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Morphological Gradient Algorithm
 */
class MorphGradientAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif


public:
    explicit MorphGradientAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Top Hat Algorithm
 */
class TopHatAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

public:
    explicit TopHatAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Black Hat Algorithm
 */
class BlackHatAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif
public:
    explicit BlackHatAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Register all morphology algorithms
 */
void registerMorphologyAlgorithms();

#endif // IMAGE_ALGORITHMS_MORPHOLOGY_H
