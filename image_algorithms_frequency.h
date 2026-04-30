#ifndef IMAGE_ALGORITHMS_FREQUENCY_H
#define IMAGE_ALGORITHMS_FREQUENCY_H

#include "image_algorithm_base.h"

/**
 * @brief Ideal Low Pass Filter Algorithm
 */
class IdealLowPassAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit IdealLowPassAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    // CUDA DFT does not support DFT_COMPLEX_OUTPUT, so disable CUDA for frequency domain
    bool supportsCuda() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif
};

/**
 * @brief Ideal High Pass Filter Algorithm
 */
class IdealHighPassAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit IdealHighPassAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    // CUDA DFT does not support DFT_COMPLEX_OUTPUT, so disable CUDA for frequency domain
    bool supportsCuda() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif
};

/**
 * @brief Gaussian Low Pass Filter Algorithm (Frequency Domain)
 */
class GaussianLowPassAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit GaussianLowPassAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    // CUDA DFT does not support DFT_COMPLEX_OUTPUT, so disable CUDA for frequency domain
    bool supportsCuda() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif
};

/**
 * @brief Gaussian High Pass Filter Algorithm (Frequency Domain)
 */
class GaussianHighPassAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit GaussianHighPassAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    // CUDA DFT does not support DFT_COMPLEX_OUTPUT, so disable CUDA for frequency domain
    bool supportsCuda() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif
};

/**
 * @brief Butterworth Low Pass Filter Algorithm
 */
class ButterworthLowPassAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit ButterworthLowPassAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    // CUDA DFT does not support DFT_COMPLEX_OUTPUT, so disable CUDA for frequency domain
    bool supportsCuda() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif
};

/**
 * @brief Butterworth High Pass Filter Algorithm
 */
class ButterworthHighPassAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit ButterworthHighPassAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    // CUDA DFT does not support DFT_COMPLEX_OUTPUT, so disable CUDA for frequency domain
    bool supportsCuda() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif
};

/**
 * @brief Homomorphic Filter Algorithm
 */
class HomomorphicFilterAlgorithm : public ImageAlgorithmBase
{
    Q_OBJECT
    
public:
    explicit HomomorphicFilterAlgorithm(QObject *parent = nullptr);

    bool supportsFloat32Input() const override { return false; }
    // CUDA DFT does not support DFT_COMPLEX_OUTPUT, so disable CUDA for frequency domain
    bool supportsCuda() const override { return false; }
    
    AlgorithmInfo algorithmInfo() const override;

protected:
    cv::UMat processImpl(const cv::UMat &input) override;
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream) override;
#endif
};

/**
 * @brief Register all frequency domain algorithms
 */
void registerFrequencyAlgorithms();

#endif // IMAGE_ALGORITHMS_FREQUENCY_H
