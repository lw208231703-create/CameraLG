#ifndef IMAGE_ALGORITHMS_ENHANCE_H
#define IMAGE_ALGORITHMS_ENHANCE_H

#include "image_algorithm_base.h"

/**
 * @brief Gamma Correction Algorithm
 */
class GammaCorrectionAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit GammaCorrectionAlgorithm(QObject *parent = nullptr);

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
 * @brief Brightness/Contrast Adjustment Algorithm
 */
class BrightnessContrastAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit BrightnessContrastAlgorithm(QObject *parent = nullptr);
    
    AlgorithmInfo algorithmInfo() const override;
    
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Histogram Equalization Algorithm
 */
class HistogramEqualizationAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit HistogramEqualizationAlgorithm(QObject *parent = nullptr);

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
 * @brief CLAHE (Contrast Limited Adaptive Histogram Equalization) Algorithm
 */
class CLAHEAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit CLAHEAlgorithm(QObject *parent = nullptr);

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
 * @brief Image Negative Algorithm
 */
class NegativeAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    bool supportsCuda() const override { return true; }
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif


public:
    explicit NegativeAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;
    
protected:
    cv::UMat processImpl(const cv::UMat &input) override;
};

/**
 * @brief Sharpen Filter Algorithm
 */
class SharpenAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit SharpenAlgorithm(QObject *parent = nullptr);

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
 * @brief Logarithmic Transform Algorithm
 */
class LogarithmicTransformAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit LogarithmicTransformAlgorithm(QObject *parent = nullptr);

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
 * @brief Distance Transform Algorithm
 */
class DistanceTransformAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit DistanceTransformAlgorithm(QObject *parent = nullptr);

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
 * @brief Register all enhancement algorithms
 */
void registerEnhanceAlgorithms();

#endif // IMAGE_ALGORITHMS_ENHANCE_H
