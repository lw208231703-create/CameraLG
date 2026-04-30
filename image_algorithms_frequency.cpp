#include "image_algorithms_frequency.h"
#include <QCoreApplication>
#include <QDebug>
#include <cmath>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/cudawarping.hpp>

namespace {

// Helper function to shift DFT quadrants (CPU version)
void shiftDFT(cv::Mat& magI)
{
    int cx = magI.cols / 2;
    int cy = magI.rows / 2;

    cv::Mat q0(magI, cv::Rect(0, 0, cx, cy));   // Top-Left
    cv::Mat q1(magI, cv::Rect(cx, 0, cx, cy));  // Top-Right
    cv::Mat q2(magI, cv::Rect(0, cy, cx, cy));  // Bottom-Left
    cv::Mat q3(magI, cv::Rect(cx, cy, cx, cy)); // Bottom-Right

    cv::Mat tmp;
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);

    q1.copyTo(tmp);
    q2.copyTo(q1);
    tmp.copyTo(q2);
}

// GPU-based FFT quadrant shift helper
void shiftDFTCuda(cv::cuda::GpuMat& magI, cv::cuda::Stream& stream)
{
    int cx = magI.cols / 2;
    int cy = magI.rows / 2;

    cv::cuda::GpuMat q0(magI, cv::Rect(0, 0, cx, cy));   // Top-Left
    cv::cuda::GpuMat q1(magI, cv::Rect(cx, 0, cx, cy));  // Top-Right
    cv::cuda::GpuMat q2(magI, cv::Rect(0, cy, cx, cy));  // Bottom-Left
    cv::cuda::GpuMat q3(magI, cv::Rect(cx, cy, cx, cy)); // Bottom-Right

    cv::cuda::GpuMat tmp;
    q0.copyTo(tmp, stream);
    q3.copyTo(q0, stream);
    tmp.copyTo(q3, stream);

    q1.copyTo(tmp, stream);
    q2.copyTo(q1, stream);
    tmp.copyTo(q2, stream);
}

// Helper function to shift DFT quadrants (UMat/OpenCL version)
void shiftDFTUMat(cv::UMat& magI)
{
    int cx = magI.cols / 2;
    int cy = magI.rows / 2;

    cv::UMat q0(magI, cv::Rect(0, 0, cx, cy));   // Top-Left
    cv::UMat q1(magI, cv::Rect(cx, 0, cx, cy));  // Top-Right
    cv::UMat q2(magI, cv::Rect(0, cy, cx, cy));  // Bottom-Left
    cv::UMat q3(magI, cv::Rect(cx, cy, cx, cy)); // Bottom-Right

    cv::UMat tmp;
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);

    q1.copyTo(tmp);
    q2.copyTo(q1);
    tmp.copyTo(q2);
}

// Helper function to apply frequency domain filter (UMat/OpenCL accelerated)
cv::UMat applyFrequencyFilterUMat(const cv::UMat& input, const cv::Mat& filter)
{
    // Get optimal DFT size
    int optWidth = cv::getOptimalDFTSize(input.cols);
    int optHeight = cv::getOptimalDFTSize(input.rows);
    
    // Pad input for optimal DFT
    cv::UMat padded;
    cv::copyMakeBorder(input, padded, 0, optHeight - input.rows, 0, optWidth - input.cols, 
                       cv::BORDER_CONSTANT, cv::Scalar::all(0));
    
    // Convert to float
    cv::UMat floatPadded;
    padded.convertTo(floatPadded, CV_32F);
    
    // Create complex planes
    std::vector<cv::UMat> planes(2);
    planes[0] = floatPadded;
    planes[1] = cv::UMat::zeros(floatPadded.size(), CV_32F);
    cv::UMat complexI;
    cv::merge(planes, complexI);
    
    // Apply DFT (OpenCL accelerated when available)
    cv::dft(complexI, complexI, cv::DFT_COMPLEX_OUTPUT);
    
    // Split real and imaginary parts
    cv::split(complexI, planes);
    
    // Shift to center
    shiftDFTUMat(planes[0]);
    shiftDFTUMat(planes[1]);
    
    // Apply filter to both parts
    cv::UMat resizedFilter;
    cv::resize(filter, resizedFilter, planes[0].size());
    cv::multiply(planes[0], resizedFilter, planes[0]);
    cv::multiply(planes[1], resizedFilter, planes[1]);
    
    // Shift back
    shiftDFTUMat(planes[0]);
    shiftDFTUMat(planes[1]);
    
    // Merge and apply inverse DFT
    cv::merge(planes, complexI);
    cv::idft(complexI, complexI, cv::DFT_SCALE | cv::DFT_COMPLEX_OUTPUT);
    
    // Split and get magnitude
    cv::split(complexI, planes);
    cv::UMat result;
    cv::magnitude(planes[0], planes[1], result);
    
    // Crop to original size
    result = result(cv::Rect(0, 0, input.cols, input.rows));
    
    // Normalize and convert to 8-bit
    cv::UMat normalized;
    cv::normalize(result, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
    
    return normalized;
}

// Helper function for homomorphic filter (UMat/OpenCL accelerated, preserves float data)
cv::UMat applyHomomorphicFilterUMat(const cv::UMat& logInput, const cv::Mat& filter)
{
    // Get optimal DFT size
    int optWidth = cv::getOptimalDFTSize(logInput.cols);
    int optHeight = cv::getOptimalDFTSize(logInput.rows);
    
    // Pad input for optimal DFT
    cv::UMat padded;
    cv::copyMakeBorder(logInput, padded, 0, optHeight - logInput.rows, 0, optWidth - logInput.cols, 
                       cv::BORDER_CONSTANT, cv::Scalar::all(0));
    
    // Create complex planes (input should already be float from log)
    std::vector<cv::UMat> planes(2);
    planes[0] = padded;
    planes[1] = cv::UMat::zeros(padded.size(), CV_32F);
    cv::UMat complexI;
    cv::merge(planes, complexI);
    
    // Apply DFT (OpenCL accelerated)
    cv::dft(complexI, complexI, cv::DFT_COMPLEX_OUTPUT);
    
    // Split real and imaginary parts
    cv::split(complexI, planes);
    
    // Shift to center
    shiftDFTUMat(planes[0]);
    shiftDFTUMat(planes[1]);
    
    // Apply filter to both parts
    cv::UMat resizedFilter;
    cv::resize(filter, resizedFilter, planes[0].size());
    cv::multiply(planes[0], resizedFilter, planes[0]);
    cv::multiply(planes[1], resizedFilter, planes[1]);
    
    // Shift back
    shiftDFTUMat(planes[0]);
    shiftDFTUMat(planes[1]);
    
    // Merge and apply inverse DFT
    cv::merge(planes, complexI);
    cv::idft(complexI, complexI, cv::DFT_SCALE | cv::DFT_COMPLEX_OUTPUT);
    
    // Split and get real part (for homomorphic, we need real part)
    cv::split(complexI, planes);
    
    // Crop to original size - return as float for exp operation
    cv::UMat result = planes[0](cv::Rect(0, 0, logInput.cols, logInput.rows)).clone();
    
    return result;
}

// Helper function to apply frequency domain filter
cv::Mat applyFrequencyFilter(const cv::Mat& input, const cv::Mat& filter)
{
    // Get optimal DFT size
    int optWidth = cv::getOptimalDFTSize(input.cols);
    int optHeight = cv::getOptimalDFTSize(input.rows);
    
    // Pad input for optimal DFT
    cv::Mat padded;
    cv::copyMakeBorder(input, padded, 0, optHeight - input.rows, 0, optWidth - input.cols, 
                       cv::BORDER_CONSTANT, cv::Scalar::all(0));
    
    // Convert to float
    cv::Mat floatPadded;
    padded.convertTo(floatPadded, CV_32F);
    
    // Create complex planes
    cv::Mat planes[] = {floatPadded, cv::Mat::zeros(floatPadded.size(), CV_32F)};
    cv::Mat complexI;
    cv::merge(planes, 2, complexI);
    
    // Apply DFT
    cv::dft(complexI, complexI, cv::DFT_COMPLEX_OUTPUT);
    
    // Split real and imaginary parts
    cv::split(complexI, planes);
    
    // Shift to center
    shiftDFT(planes[0]);
    shiftDFT(planes[1]);
    
    // Apply filter to both parts
    cv::Mat resizedFilter;
    cv::resize(filter, resizedFilter, planes[0].size());
    cv::multiply(planes[0], resizedFilter, planes[0]);
    cv::multiply(planes[1], resizedFilter, planes[1]);
    
    // Shift back
    shiftDFT(planes[0]);
    shiftDFT(planes[1]);
    
    // Merge and apply inverse DFT
    cv::merge(planes, 2, complexI);
    cv::idft(complexI, complexI, cv::DFT_SCALE | cv::DFT_COMPLEX_OUTPUT);
    
    // Split and get magnitude
    cv::split(complexI, planes);
    cv::Mat result;
    cv::magnitude(planes[0], planes[1], result);
    
    // Crop to original size
    result = result(cv::Rect(0, 0, input.cols, input.rows));
    
    // Normalize and convert to 8-bit
    cv::normalize(result, result, 0, 255, cv::NORM_MINMAX);
    result.convertTo(result, CV_8U);
    
    return result;
}

// Helper function for homomorphic filter (preserves float data)
cv::Mat applyHomomorphicFilter(const cv::Mat& logInput, const cv::Mat& filter)
{
    // Get optimal DFT size
    int optWidth = cv::getOptimalDFTSize(logInput.cols);
    int optHeight = cv::getOptimalDFTSize(logInput.rows);
    
    // Pad input for optimal DFT
    cv::Mat padded;
    cv::copyMakeBorder(logInput, padded, 0, optHeight - logInput.rows, 0, optWidth - logInput.cols, 
                       cv::BORDER_CONSTANT, cv::Scalar::all(0));
    
    // Create complex planes (input should already be float from log)
    cv::Mat planes[] = {padded, cv::Mat::zeros(padded.size(), CV_32F)};
    cv::Mat complexI;
    cv::merge(planes, 2, complexI);
    
    // Apply DFT
    cv::dft(complexI, complexI, cv::DFT_COMPLEX_OUTPUT);
    
    // Split real and imaginary parts
    cv::split(complexI, planes);
    
    // Shift to center
    shiftDFT(planes[0]);
    shiftDFT(planes[1]);
    
    // Apply filter to both parts
    cv::Mat resizedFilter;
    cv::resize(filter, resizedFilter, planes[0].size());
    cv::multiply(planes[0], resizedFilter, planes[0]);
    cv::multiply(planes[1], resizedFilter, planes[1]);
    
    // Shift back
    shiftDFT(planes[0]);
    shiftDFT(planes[1]);
    
    // Merge and apply inverse DFT
    cv::merge(planes, 2, complexI);
    cv::idft(complexI, complexI, cv::DFT_SCALE | cv::DFT_COMPLEX_OUTPUT);
    
    // Split and get real part (for homomorphic, we need real part)
    cv::split(complexI, planes);
    
    // Crop to original size - return as float for exp operation
    cv::Mat result = planes[0](cv::Rect(0, 0, logInput.cols, logInput.rows)).clone();
    
    return result;
}

// ===================== Optimized Kernel Generation =====================
// Use vectorized OpenCV operations instead of pixel-by-pixel loops

// Cache for generated kernels to avoid regeneration every frame
struct KernelCache {
    cv::Size lastSize;
    float lastParam1 = -1;
    float lastParam2 = -1;
    cv::Mat cachedKernel;
    
    bool isValid(cv::Size size, float param1, float param2 = 0) const {
        return lastSize == size && lastParam1 == param1 && lastParam2 == param2 && !cachedKernel.empty();
    }
    
    void update(cv::Size size, float param1, float param2, const cv::Mat& kernel) {
        lastSize = size;
        lastParam1 = param1;
        lastParam2 = param2;
        cachedKernel = kernel;
    }
};

// Thread-local caches for each filter type
static thread_local KernelCache s_idealLPCache;
static thread_local KernelCache s_idealHPCache;
static thread_local KernelCache s_gaussianLPCache;
static thread_local KernelCache s_gaussianHPCache;
static thread_local KernelCache s_butterworthLPCache;
static thread_local KernelCache s_butterworthHPCache;
static thread_local KernelCache s_homomorphicCache;

// Pre-compute distance matrix (reusable across different filters)
static thread_local cv::Mat s_distanceMatrix;
static thread_local cv::Size s_distanceMatrixSize;

cv::Mat getDistanceMatrix(cv::Size size)
{
    if (s_distanceMatrixSize == size && !s_distanceMatrix.empty()) {
        return s_distanceMatrix;
    }
    
    float cy = size.height / 2.0f;
    float cx = size.width / 2.0f;
    
    // Create 1D vectors for row and column indices
    cv::Mat rowVec(1, size.width, CV_32F);
    cv::Mat colVec(size.height, 1, CV_32F);
    
    float* rowPtr = rowVec.ptr<float>(0);
    for (int j = 0; j < size.width; j++) {
        float dx = j - cx;
        rowPtr[j] = dx * dx;
    }
    
    for (int i = 0; i < size.height; i++) {
        float dy = i - cy;
        colVec.at<float>(i, 0) = dy * dy;
    }
    
    // Expand to full matrices using repeat (very fast)
    cv::Mat rowMat = cv::repeat(rowVec, size.height, 1);
    cv::Mat colMat = cv::repeat(colVec, 1, size.width);
    
    // Compute distance: sqrt(dy^2 + dx^2)
    cv::Mat distSq;
    cv::add(rowMat, colMat, distSq);
    cv::sqrt(distSq, s_distanceMatrix);
    s_distanceMatrixSize = size;
    
    return s_distanceMatrix;
}

// Optimized Ideal Low Pass kernel using vectorized operations
cv::Mat generateIdealLPKernel(cv::Size size, float radius)
{
    if (s_idealLPCache.isValid(size, radius, 0)) {
        return s_idealLPCache.cachedKernel;
    }
    
    cv::Mat distance = getDistanceMatrix(size);
    cv::Mat kernel;
    cv::compare(distance, radius, kernel, cv::CMP_LE);
    kernel.convertTo(kernel, CV_32F, 1.0/255.0);
    
    s_idealLPCache.update(size, radius, 0, kernel);
    return kernel;
}

// Optimized Ideal High Pass kernel
cv::Mat generateIdealHPKernel(cv::Size size, float radius)
{
    if (s_idealHPCache.isValid(size, radius, 0)) {
        return s_idealHPCache.cachedKernel;
    }
    
    cv::Mat distance = getDistanceMatrix(size);
    cv::Mat kernel;
    cv::compare(distance, radius, kernel, cv::CMP_GT);
    kernel.convertTo(kernel, CV_32F, 1.0/255.0);
    
    s_idealHPCache.update(size, radius, 0, kernel);
    return kernel;
}

// Optimized Gaussian Low Pass kernel
cv::Mat generateGaussianLPKernel(cv::Size size, float sigma)
{
    if (s_gaussianLPCache.isValid(size, sigma, 0)) {
        return s_gaussianLPCache.cachedKernel;
    }
    
    cv::Mat distance = getDistanceMatrix(size);
    cv::Mat distSq;
    cv::multiply(distance, distance, distSq);
    
    cv::Mat kernel;
    float coeff = -1.0f / (2.0f * sigma * sigma);
    cv::multiply(distSq, coeff, kernel);
    cv::exp(kernel, kernel);
    
    s_gaussianLPCache.update(size, sigma, 0, kernel);
    return kernel;
}

// Optimized Gaussian High Pass kernel
cv::Mat generateGaussianHPKernel(cv::Size size, float sigma)
{
    if (s_gaussianHPCache.isValid(size, sigma, 0)) {
        return s_gaussianHPCache.cachedKernel;
    }
    
    cv::Mat lpKernel = generateGaussianLPKernel(size, sigma);
    cv::Mat kernel;
    cv::subtract(1.0, lpKernel, kernel);
    
    s_gaussianHPCache.update(size, sigma, 0, kernel);
    return kernel;
}

// Optimized Butterworth Low Pass kernel
cv::Mat generateButterworthLPKernel(cv::Size size, float D0, int n)
{
    if (s_butterworthLPCache.isValid(size, D0, static_cast<float>(n))) {
        return s_butterworthLPCache.cachedKernel;
    }
    
    cv::Mat distance = getDistanceMatrix(size);
    
    // H = 1 / (1 + (D/D0)^(2n))
    cv::Mat ratio;
    cv::divide(distance, D0, ratio);
    cv::pow(ratio, 2.0 * n, ratio);
    cv::add(ratio, 1.0, ratio);
    
    cv::Mat kernel;
    cv::divide(1.0, ratio, kernel);
    
    s_butterworthLPCache.update(size, D0, static_cast<float>(n), kernel);
    return kernel;
}

// Optimized Butterworth High Pass kernel
cv::Mat generateButterworthHPKernel(cv::Size size, float D0, int n)
{
    if (s_butterworthHPCache.isValid(size, D0, static_cast<float>(n))) {
        return s_butterworthHPCache.cachedKernel;
    }
    
    cv::Mat distance = getDistanceMatrix(size);
    
    // H = 1 / (1 + (D0/D)^(2n))
    // Handle division by zero at center
    cv::Mat safeDistance;
    cv::max(distance, 1e-10f, safeDistance);
    
    cv::Mat ratio;
    cv::divide(D0, safeDistance, ratio);
    cv::pow(ratio, 2.0 * n, ratio);
    cv::add(ratio, 1.0, ratio);
    
    cv::Mat kernel;
    cv::divide(1.0, ratio, kernel);
    
    s_butterworthHPCache.update(size, D0, static_cast<float>(n), kernel);
    return kernel;
}

// Optimized Homomorphic filter kernel
cv::Mat generateHomomorphicKernel(cv::Size size, float gammaH, float gammaL, float sigma, float c)
{
    // Use a simple hash for the 4 parameters
    float paramHash = gammaH * 1000 + gammaL * 100 + sigma * 10 + c;
    if (s_homomorphicCache.isValid(size, paramHash, 0)) {
        return s_homomorphicCache.cachedKernel;
    }
    
    cv::Mat distance = getDistanceMatrix(size);
    cv::Mat distSq;
    cv::multiply(distance, distance, distSq);
    
    // H = (gammaH - gammaL) * (1 - exp(-c * D^2 / sigma^2)) + gammaL
    float coeff = -c / (sigma * sigma);
    cv::Mat expPart;
    cv::multiply(distSq, coeff, expPart);
    cv::exp(expPart, expPart);
    
    cv::Mat kernel;
    cv::subtract(1.0, expPart, kernel);
    cv::multiply(kernel, gammaH - gammaL, kernel);
    cv::add(kernel, gammaL, kernel);
    
    s_homomorphicCache.update(size, paramHash, 0, kernel);
    return kernel;
}

// CUDA version: Helper function to apply frequency domain filter
cv::cuda::GpuMat applyFrequencyFilterCuda(const cv::cuda::GpuMat& input, const cv::Mat& filter, cv::cuda::Stream& stream)
{
    // Get optimal DFT size
    int optWidth = cv::getOptimalDFTSize(input.cols);
    int optHeight = cv::getOptimalDFTSize(input.rows);
    
    // Pad input for optimal DFT
    cv::cuda::GpuMat padded;
    cv::cuda::copyMakeBorder(input, padded, 0, optHeight - input.rows, 0, optWidth - input.cols, 
                       cv::BORDER_CONSTANT, cv::Scalar::all(0), stream);
    
    // Convert to float
    cv::cuda::GpuMat floatPadded;
    padded.convertTo(floatPadded, CV_32F, stream);
    
    // Apply DFT (CUDA DFT works on single-channel float)
    cv::cuda::GpuMat complexI;
    cv::cuda::dft(floatPadded, complexI, floatPadded.size(), cv::DFT_COMPLEX_OUTPUT, stream);
    
    // Upload filter to GPU and resize
    cv::cuda::GpuMat gpuFilter;
    filter.copyTo(gpuFilter);
    cv::cuda::GpuMat resizedFilter;
    cv::cuda::resize(gpuFilter, resizedFilter, complexI.size(), 0, 0, cv::INTER_LINEAR, stream);
    
    // Split complex result into real and imaginary
    cv::cuda::GpuMat planes[2];
    cv::cuda::split(complexI, planes, stream);
    
    // Perform GPU-based FFT quadrant shift
    shiftDFTCuda(planes[0], stream);
    shiftDFTCuda(planes[1], stream);
    
    // Apply filter to both parts (GPU operations)
    cv::cuda::multiply(planes[0], resizedFilter, planes[0], 1.0, -1, stream);
    cv::cuda::multiply(planes[1], resizedFilter, planes[1], 1.0, -1, stream);
    
    // Shift back
    shiftDFTCuda(planes[0], stream);
    shiftDFTCuda(planes[1], stream);
    
    // Merge and apply inverse DFT
    cv::cuda::merge(planes, 2, complexI, stream);
    cv::cuda::GpuMat result;
    cv::cuda::dft(complexI, result, complexI.size(), cv::DFT_INVERSE | cv::DFT_SCALE, stream);
    
    // Split and get magnitude
    cv::cuda::split(result, planes, stream);
    cv::cuda::GpuMat magnitude;
    cv::cuda::magnitude(planes[0], planes[1], magnitude, stream);
    
    // Crop to original size
    cv::cuda::GpuMat cropped(magnitude, cv::Rect(0, 0, input.cols, input.rows));
    
    // Normalize and convert to 8-bit
    cv::cuda::GpuMat normalized;
    cv::cuda::normalize(cropped, normalized, 0, 255, cv::NORM_MINMAX, CV_8U, cv::cuda::GpuMat(), stream);
    
    return normalized;
}

// CUDA version: Helper function for homomorphic filter
cv::cuda::GpuMat applyHomomorphicFilterCuda(const cv::cuda::GpuMat& logInput, const cv::Mat& filter, cv::cuda::Stream& stream)
{
    // Get optimal DFT size
    int optWidth = cv::getOptimalDFTSize(logInput.cols);
    int optHeight = cv::getOptimalDFTSize(logInput.rows);
    
    // Pad input for optimal DFT
    cv::cuda::GpuMat padded;
    cv::cuda::copyMakeBorder(logInput, padded, 0, optHeight - logInput.rows, 0, optWidth - logInput.cols, 
                       cv::BORDER_CONSTANT, cv::Scalar::all(0), stream);
    
    // Apply DFT
    cv::cuda::GpuMat complexI;
    cv::cuda::dft(padded, complexI, padded.size(), cv::DFT_COMPLEX_OUTPUT, stream);
    
    // Upload filter to GPU and resize
    cv::cuda::GpuMat gpuFilter;
    filter.copyTo(gpuFilter);
    cv::cuda::GpuMat resizedFilter;
    cv::cuda::resize(gpuFilter, resizedFilter, complexI.size(), 0, 0, cv::INTER_LINEAR, stream);
    
    // Split complex result
    cv::cuda::GpuMat planes[2];
    cv::cuda::split(complexI, planes, stream);
    
    // Perform GPU-based FFT quadrant shift
    shiftDFTCuda(planes[0], stream);
    shiftDFTCuda(planes[1], stream);
    
    // Apply filter to both parts (GPU operations)
    cv::cuda::multiply(planes[0], resizedFilter, planes[0], 1.0, -1, stream);
    cv::cuda::multiply(planes[1], resizedFilter, planes[1], 1.0, -1, stream);
    
    // Shift back
    shiftDFTCuda(planes[0], stream);
    shiftDFTCuda(planes[1], stream);
    
    // Merge and apply inverse DFT
    cv::cuda::merge(planes, 2, complexI, stream);
    cv::cuda::GpuMat result;
    cv::cuda::dft(complexI, result, complexI.size(), cv::DFT_INVERSE | cv::DFT_SCALE, stream);
    
    // Split and get real part
    cv::cuda::split(result, planes, stream);
    
    // Crop to original size - return as float for exp operation
    cv::cuda::GpuMat cropped(planes[0], cv::Rect(0, 0, logInput.cols, logInput.rows));
    
    return cropped.clone();
}

} // anonymous namespace

// Ideal Low Pass Filter
IdealLowPassAlgorithm::IdealLowPassAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["radius"] = 30.0;
}

AlgorithmInfo IdealLowPassAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "frequency.ideal_lp";
    info.name = QCoreApplication::translate("Algorithms", "理想低通滤波");
    info.category = QCoreApplication::translate("Algorithms", "频域滤波");
    info.description = QCoreApplication::translate("Algorithms", "在频率域中去除高频分量，平滑图像");
    
    info.parameters.append(AlgorithmParameter(
        "radius",
        QCoreApplication::translate("Algorithms", "截止半径"),
        "double", 30.0, 1.0, 200.0, 5.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "低通滤波器的截止频率半径")
    ));
    
    return info;
}

cv::UMat IdealLowPassAlgorithm::processImpl(const cv::UMat &input)
{
    double radius = getParameter("radius").toDouble();
    if (radius <= 0) radius = 30.0;
    
    cv::Mat kernel = generateIdealLPKernel(cv::Size(input.cols, input.rows), static_cast<float>(radius));
    return applyFrequencyFilterUMat(input, kernel);
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat IdealLowPassAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double radius = getParameter("radius").toDouble();
    if (radius <= 0) radius = 30.0;

    cv::Mat kernel = generateIdealLPKernel(cv::Size(input.cols, input.rows), static_cast<float>(radius));
    return applyFrequencyFilterCuda(input, kernel, stream);
}
#endif

// Ideal High Pass Filter
IdealHighPassAlgorithm::IdealHighPassAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["radius"] = 30.0;
}

AlgorithmInfo IdealHighPassAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "frequency.ideal_hp";
    info.name = QCoreApplication::translate("Algorithms", "理想高通滤波");
    info.category = QCoreApplication::translate("Algorithms", "频域滤波");
    info.description = QCoreApplication::translate("Algorithms", "在频率域中去除低频分量，突出边缘");
    
    info.parameters.append(AlgorithmParameter(
        "radius",
        QCoreApplication::translate("Algorithms", "截止半径"),
        "double", 30.0, 1.0, 200.0, 5.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "高通滤波器的截止频率半径")
    ));
    
    return info;
}

cv::UMat IdealHighPassAlgorithm::processImpl(const cv::UMat &input)
{
    double radius = getParameter("radius").toDouble();
    if (radius <= 0) radius = 30.0;
    
    cv::Mat kernel = generateIdealHPKernel(cv::Size(input.cols, input.rows), static_cast<float>(radius));
    return applyFrequencyFilterUMat(input, kernel);
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat IdealHighPassAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double radius = getParameter("radius").toDouble();
    if (radius <= 0) radius = 30.0;

    cv::Mat kernel = generateIdealHPKernel(cv::Size(input.cols, input.rows), static_cast<float>(radius));
    return applyFrequencyFilterCuda(input, kernel, stream);
}
#endif

// Gaussian Low Pass Filter (Frequency Domain)
GaussianLowPassAlgorithm::GaussianLowPassAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["sigma"] = 30.0;
}

AlgorithmInfo GaussianLowPassAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "frequency.gaussian_lp";
    info.name = QCoreApplication::translate("Algorithms", "高斯低通滤波");
    info.category = QCoreApplication::translate("Algorithms", "频域滤波");
    info.description = QCoreApplication::translate("Algorithms", "使用高斯函数进行频域低通滤波，无振铃效应");
    
    info.parameters.append(AlgorithmParameter(
        "sigma",
        QCoreApplication::translate("Algorithms", "Sigma"),
        "double", 30.0, 1.0, 200.0, 5.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "高斯滤波器的标准差")
    ));
    
    return info;
}

cv::UMat GaussianLowPassAlgorithm::processImpl(const cv::UMat &input)
{
    double sigma = getParameter("sigma").toDouble();
    if (sigma <= 0) sigma = 30.0;
    
    cv::Mat kernel = generateGaussianLPKernel(cv::Size(input.cols, input.rows), static_cast<float>(sigma));
    return applyFrequencyFilterUMat(input, kernel);
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat GaussianLowPassAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double sigma = getParameter("sigma").toDouble();
    if (sigma <= 0) sigma = 30.0;

    cv::Mat kernel = generateGaussianLPKernel(cv::Size(input.cols, input.rows), static_cast<float>(sigma));
    return applyFrequencyFilterCuda(input, kernel, stream);
}
#endif

// Gaussian High Pass Filter (Frequency Domain)
GaussianHighPassAlgorithm::GaussianHighPassAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["sigma"] = 30.0;
}

AlgorithmInfo GaussianHighPassAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "frequency.gaussian_hp";
    info.name = QCoreApplication::translate("Algorithms", "高斯高通滤波");
    info.category = QCoreApplication::translate("Algorithms", "频域滤波");
    info.description = QCoreApplication::translate("Algorithms", "使用高斯函数进行频域高通滤波，突出边缘");
    
    info.parameters.append(AlgorithmParameter(
        "sigma",
        QCoreApplication::translate("Algorithms", "Sigma"),
        "double", 30.0, 1.0, 200.0, 5.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "高斯滤波器的标准差")
    ));
    
    return info;
}

cv::UMat GaussianHighPassAlgorithm::processImpl(const cv::UMat &input)
{
    double sigma = getParameter("sigma").toDouble();
    if (sigma <= 0) sigma = 30.0;
    
    cv::Mat kernel = generateGaussianHPKernel(cv::Size(input.cols, input.rows), static_cast<float>(sigma));
    return applyFrequencyFilterUMat(input, kernel);
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat GaussianHighPassAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double sigma = getParameter("sigma").toDouble();
    if (sigma <= 0) sigma = 30.0;

    cv::Mat kernel = generateGaussianHPKernel(cv::Size(input.cols, input.rows), static_cast<float>(sigma));
    return applyFrequencyFilterCuda(input, kernel, stream);
}
#endif

// Butterworth Low Pass Filter
ButterworthLowPassAlgorithm::ButterworthLowPassAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["cutoff"] = 30.0;
    m_parameters["order"] = 2;
}

AlgorithmInfo ButterworthLowPassAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "frequency.butterworth_lp";
    info.name = QCoreApplication::translate("Algorithms", "巴特沃斯低通滤波");
    info.category = QCoreApplication::translate("Algorithms", "频域滤波");
    info.description = QCoreApplication::translate("Algorithms", "使用巴特沃斯函数进行频域低通滤波，平滑过渡");
    
    info.parameters.append(AlgorithmParameter(
        "cutoff",
        QCoreApplication::translate("Algorithms", "截止频率"),
        "double", 30.0, 1.0, 200.0, 5.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "巴特沃斯滤波器的截止频率")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "order",
        QCoreApplication::translate("Algorithms", "阶数"),
        "int", 2, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "巴特沃斯滤波器的阶数")
    ));
    
    return info;
}

cv::UMat ButterworthLowPassAlgorithm::processImpl(const cv::UMat &input)
{
    double cutoff = getParameter("cutoff").toDouble();
    int order = getParameter("order").toInt();
    if (cutoff <= 0) cutoff = 30.0;
    if (order < 1) order = 2;
    
    cv::Mat kernel = generateButterworthLPKernel(cv::Size(input.cols, input.rows), static_cast<float>(cutoff), order);
    return applyFrequencyFilterUMat(input, kernel);
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat ButterworthLowPassAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double cutoff = getParameter("cutoff").toDouble();
    int order = getParameter("order").toInt();
    if (cutoff <= 0) cutoff = 30.0;
    if (order < 1) order = 2;

    cv::Mat kernel = generateButterworthLPKernel(cv::Size(input.cols, input.rows), static_cast<float>(cutoff), order);
    return applyFrequencyFilterCuda(input, kernel, stream);
}
#endif

// Butterworth High Pass Filter
ButterworthHighPassAlgorithm::ButterworthHighPassAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["cutoff"] = 30.0;
    m_parameters["order"] = 2;
}

AlgorithmInfo ButterworthHighPassAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "frequency.butterworth_hp";
    info.name = QCoreApplication::translate("Algorithms", "巴特沃斯高通滤波");
    info.category = QCoreApplication::translate("Algorithms", "频域滤波");
    info.description = QCoreApplication::translate("Algorithms", "使用巴特沃斯函数进行频域高通滤波，突出边缘");
    
    info.parameters.append(AlgorithmParameter(
        "cutoff",
        QCoreApplication::translate("Algorithms", "截止频率"),
        "double", 30.0, 1.0, 200.0, 5.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "巴特沃斯滤波器的截止频率")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "order",
        QCoreApplication::translate("Algorithms", "阶数"),
        "int", 2, 1, 10, 1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "巴特沃斯滤波器的阶数")
    ));
    
    return info;
}

cv::UMat ButterworthHighPassAlgorithm::processImpl(const cv::UMat &input)
{
    double cutoff = getParameter("cutoff").toDouble();
    int order = getParameter("order").toInt();
    if (cutoff <= 0) cutoff = 30.0;
    if (order < 1) order = 2;
    
    cv::Mat kernel = generateButterworthHPKernel(cv::Size(input.cols, input.rows), static_cast<float>(cutoff), order);
    return applyFrequencyFilterUMat(input, kernel);
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat ButterworthHighPassAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double cutoff = getParameter("cutoff").toDouble();
    int order = getParameter("order").toInt();
    if (cutoff <= 0) cutoff = 30.0;
    if (order < 1) order = 2;

    cv::Mat kernel = generateButterworthHPKernel(cv::Size(input.cols, input.rows), static_cast<float>(cutoff), order);
    return applyFrequencyFilterCuda(input, kernel, stream);
}
#endif

// Homomorphic Filter
HomomorphicFilterAlgorithm::HomomorphicFilterAlgorithm(QObject *parent)
    : ImageAlgorithmBase(parent)
{
    m_parameters["gammaH"] = 2.0;
    m_parameters["gammaL"] = 0.5;
    m_parameters["sigma"] = 30.0;
    m_parameters["c"] = 1.0;
}

AlgorithmInfo HomomorphicFilterAlgorithm::algorithmInfo() const
{
    AlgorithmInfo info;
    info.id = "frequency.homomorphic";
    info.name = QCoreApplication::translate("Algorithms", "同态滤波");
    info.category = QCoreApplication::translate("Algorithms", "频域滤波");
    info.description = QCoreApplication::translate("Algorithms", "同态滤波用于同时增强对比度和压缩动态范围");
    
    info.parameters.append(AlgorithmParameter(
        "gammaH",
        QCoreApplication::translate("Algorithms", "高频增益"),
        "double", 2.0, 1.0, 5.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "高频部分的增益系数 (>1)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "gammaL",
        QCoreApplication::translate("Algorithms", "低频增益"),
        "double", 0.5, 0.01, 0.99, 0.05,
        QStringList(),
        QCoreApplication::translate("Algorithms", "低频部分的增益系数 (<1)")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "sigma",
        QCoreApplication::translate("Algorithms", "截止频率"),
        "double", 30.0, 1.0, 200.0, 5.0,
        QStringList(),
        QCoreApplication::translate("Algorithms", "滤波器的截止频率")
    ));
    
    info.parameters.append(AlgorithmParameter(
        "c",
        QCoreApplication::translate("Algorithms", "斜率系数"),
        "double", 1.0, 0.1, 5.0, 0.1,
        QStringList(),
        QCoreApplication::translate("Algorithms", "控制滤波器的过渡斜率")
    ));
    
    return info;
}

cv::UMat HomomorphicFilterAlgorithm::processImpl(const cv::UMat &input)
{
    double gammaH = getParameter("gammaH").toDouble();
    double gammaL = getParameter("gammaL").toDouble();
    double sigma = getParameter("sigma").toDouble();
    double c = getParameter("c").toDouble();
    
    if (gammaH < 1.0) gammaH = 2.0;
    if (gammaL <= 0 || gammaL >= 1.0) gammaL = 0.5;
    if (sigma <= 0) sigma = 30.0;
    if (c <= 0) c = 1.0;
    
    // Convert to float and add 1 to avoid log(0) (OpenCL accelerated)
    cv::UMat floatImg;
    input.convertTo(floatImg, CV_32F);
    cv::add(floatImg, cv::Scalar(1.0f), floatImg);
    
    // Apply log (OpenCL accelerated)
    cv::UMat logImg;
    cv::log(floatImg, logImg);
    
    // Apply homomorphic frequency filter (preserves float precision)
    cv::Mat kernel = generateHomomorphicKernel(cv::Size(input.cols, input.rows), 
                                                static_cast<float>(gammaH), 
                                                static_cast<float>(gammaL), 
                                                static_cast<float>(sigma), 
                                                static_cast<float>(c));
    cv::UMat filtered = applyHomomorphicFilterUMat(logImg, kernel);
    
    // Apply exp and subtract 1 (OpenCL accelerated)
    cv::UMat expImg;
    cv::exp(filtered, expImg);
    cv::subtract(expImg, cv::Scalar(1.0f), expImg);
    
    // Normalize and convert to 8-bit (OpenCL accelerated)
    cv::UMat normalized;
    cv::normalize(expImg, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
    
    return normalized;
}

#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat HomomorphicFilterAlgorithm::processImplCuda(const cv::cuda::GpuMat &input, cv::cuda::Stream& stream)
{
    double gammaH = getParameter("gammaH").toDouble();
    double gammaL = getParameter("gammaL").toDouble();
    double sigma = getParameter("sigma").toDouble();
    double c = getParameter("c").toDouble();

    if (gammaH < 1.0) gammaH = 2.0;
    if (gammaL <= 0 || gammaL >= 1.0) gammaL = 0.5;
    if (sigma <= 0) sigma = 30.0;
    if (c <= 0) c = 1.0;

    // Convert to float and add 1 to avoid log(0)
    cv::cuda::GpuMat floatImg;
    input.convertTo(floatImg, CV_32F, stream);
    cv::cuda::add(floatImg, cv::Scalar(1.0f), floatImg, cv::cuda::GpuMat(), -1, stream);

    // Apply log
    cv::cuda::GpuMat logImg;
    cv::cuda::log(floatImg, logImg, stream);

    // Apply homomorphic frequency filter
    cv::Mat kernel = generateHomomorphicKernel(cv::Size(input.cols, input.rows),
                                                static_cast<float>(gammaH),
                                                static_cast<float>(gammaL),
                                                static_cast<float>(sigma),
                                                static_cast<float>(c));
    cv::cuda::GpuMat filtered = applyHomomorphicFilterCuda(logImg, kernel, stream);

    // Apply exp and subtract 1
    cv::cuda::GpuMat expImg;
    cv::cuda::exp(filtered, expImg, stream);
    cv::cuda::subtract(expImg, cv::Scalar(1.0f), expImg, cv::cuda::GpuMat(), -1, stream);

    // Normalize and convert to 8-bit
    cv::cuda::GpuMat result;
    cv::cuda::normalize(expImg, result, 0, 255, cv::NORM_MINMAX, CV_8U, cv::cuda::GpuMat(), stream);

    return result;
}
#endif

// Register frequency domain algorithms
void registerFrequencyAlgorithms()
{
    ImageAlgorithmFactory::instance().registerAlgorithm<IdealLowPassAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<IdealHighPassAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<GaussianLowPassAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<GaussianHighPassAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<ButterworthLowPassAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<ButterworthHighPassAlgorithm>();
    ImageAlgorithmFactory::instance().registerAlgorithm<HomomorphicFilterAlgorithm>();
}
