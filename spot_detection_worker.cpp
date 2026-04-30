#include "spot_detection_worker.h"
#include <QDebug>
#include <QElapsedTimer>
#include <cmath>

SpotDetectionWorker::SpotDetectionWorker(QObject *parent)
    : QObject(parent)
{
    // 注册自定义类型用于 Qt 信号/槽跨线程通信
    qRegisterMetaType<SpotDetectionResult>("SpotDetectionResult");
    qRegisterMetaType<SpotDetectionParams>("SpotDetectionParams");
    qRegisterMetaType<cv::Mat>("cv::Mat");
    
    // 初始化 Kalman 状态向量: [cx, cy, vx, vy]
    state_ = cv::Mat::zeros(4, 1, CV_64F);
    
    // 初始化协方差矩阵
    covariance_ = cv::Mat::eye(4, 4, CV_64F) * 100.0;
    
    // 初始化过程噪声协方差矩阵
    processNoiseCov_ = cv::Mat::eye(4, 4, CV_64F);
}

SpotDetectionWorker::~SpotDetectionWorker()
{
    stop();
}

void SpotDetectionWorker::setParams(const SpotDetectionParams &params)
{
    params_ = params;
    
    // 更新过程噪声协方差矩阵
    processNoiseCov_ = cv::Mat::eye(4, 4, CV_64F) * params_.processNoise;
}

void SpotDetectionWorker::reset()
{
    isInitialized_ = false;
    state_ = cv::Mat::zeros(4, 1, CV_64F);
    covariance_ = cv::Mat::eye(4, 4, CV_64F) * 100.0;
}

void SpotDetectionWorker::stop()
{
    shouldStop_.store(true);
}

void SpotDetectionWorker::processImage(cv::Mat image, double initialX, double initialY)
{
    if (shouldStop_.load()) {
        return;
    }
    
    if (image.empty()) {
        emit detectionError("Input image is empty");
        return;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    try {
        imageWidth_ = image.cols;
        imageHeight_ = image.rows;
        
        // 初始化 Kalman 滤波器 (仅首次或重置后)
        if (!isInitialized_ && initialX >= 0 && initialY >= 0) {
            state_.at<double>(0, 0) = initialX;  // cx
            state_.at<double>(1, 0) = initialY;  // cy
            state_.at<double>(2, 0) = 0.0;       // vx
            state_.at<double>(3, 0) = 0.0;       // vy
            isInitialized_ = true;
        }
        
        if (!isInitialized_) {
            emit detectionError("Kalman filter not initialized. Provide initial position.");
            return;
        }
        
        // 步骤 1: Kalman 预测
        kalmanPredict();
        
        // 步骤 2: 提取预测驱动的 ROI
        QRect roiRect;
        cv::Mat roi = extractPredictedROI(image, roiRect);
        
        if (roi.empty()) {
            emit detectionError("Failed to extract ROI");
            return;
        }
        
        // 步骤 3: 光斑预处理
        cv::Mat preprocessed;
        double background = preprocessSpot(roi, preprocessed);
        
        // 步骤 4: 计算加权质心
        double cx, cy, energy, variance;
        int validPixels;
        bool centroidOk = calculateWeightedCentroid(preprocessed, roiRect, 
                                                     cx, cy, energy, validPixels, variance);
        
        if (!centroidOk) {
            emit detectionError("Failed to calculate centroid");
            return;
        }
        
        // 步骤 5: 测量质量评估
        double measurementR = assessMeasurementQuality(energy, validPixels, variance);
        
        // 步骤 6: Kalman 更新（如果测量质量足够好）
        // 如果能量过低或有效像素太少，跳过更新，仅使用预测结果
        if (energy >= params_.minEnergy * 0.5 && validPixels >= params_.minValidPixels) {
            kalmanUpdate(cx, cy, measurementR);
        }
        
        // 步骤 7: 输出结果
        SpotDetectionResult result;
        result.centerX = state_.at<double>(0, 0);
        result.centerY = state_.at<double>(1, 0);
        result.velocityX = state_.at<double>(2, 0);
        result.velocityY = state_.at<double>(3, 0);
        result.energy = energy;
        result.validPixelCount = validPixels;
        result.variance = variance;
        result.measurementR = measurementR;
        result.roi = roiRect;
        result.isValid = true;
        result.processingTimeUs = timer.nsecsElapsed() / 1000;
        
        emit detectionFinished(result);
        
    } catch (const cv::Exception &e) {
        emit detectionError(QString("OpenCV error: %1").arg(e.what()));
    } catch (const std::exception &e) {
        emit detectionError(QString("Error: %1").arg(e.what()));
    }
}

void SpotDetectionWorker::kalmanPredict()
{
    // 状态转移矩阵 F
    // cx_k = cx_{k-1} + vx_{k-1} * dt
    // cy_k = cy_{k-1} + vy_{k-1} * dt
    // vx_k = vx_{k-1}
    // vy_k = vy_{k-1}
    
    double dt = params_.dt;
    
    cv::Mat F = cv::Mat::eye(4, 4, CV_64F);
    F.at<double>(0, 2) = dt;  // cx 受 vx 影响
    F.at<double>(1, 3) = dt;  // cy 受 vy 影响
    
    // 预测状态: x_pred = F * x
    state_ = F * state_;
    
    // 预测协方差: P_pred = F * P * F^T + Q
    covariance_ = F * covariance_ * F.t() + processNoiseCov_;
}

cv::Mat SpotDetectionWorker::extractPredictedROI(const cv::Mat &image, QRect &roiRect)
{
    // 从 Kalman 预测获取中心点
    double predictedCx = state_.at<double>(0, 0);
    double predictedCy = state_.at<double>(1, 0);
    
    int halfRoi = params_.roiSize / 2;
    
    // 计算 ROI 边界 (整数像素对齐)
    int roiX = static_cast<int>(std::round(predictedCx)) - halfRoi;
    int roiY = static_cast<int>(std::round(predictedCy)) - halfRoi;
    
    // 边界检查
    roiX = std::max(0, std::min(roiX, imageWidth_ - params_.roiSize));
    roiY = std::max(0, std::min(roiY, imageHeight_ - params_.roiSize));
    
    roiRect = QRect(roiX, roiY, params_.roiSize, params_.roiSize);
    
    // 提取 ROI
    cv::Rect cvRect(roiX, roiY, params_.roiSize, params_.roiSize);
    return image(cvRect).clone();
}

double SpotDetectionWorker::preprocessSpot(const cv::Mat &roi, cv::Mat &preprocessed)
{
    // 转换为 double 以提高精度
    cv::Mat roiDouble;
    roi.convertTo(roiDouble, CV_64F);
    
    double background = 0.0;
    
    if (params_.useBackgroundRemoval) {
        // 方法：使用 ROI 边缘像素均值作为背景
        // 边缘像素：第一行、最后一行、第一列、最后一列
        int rows = roiDouble.rows;
        int cols = roiDouble.cols;
        
        double sum = 0.0;
        int count = 0;
        
        // 第一行和最后一行
        for (int x = 0; x < cols; ++x) {
            sum += roiDouble.at<double>(0, x);
            sum += roiDouble.at<double>(rows - 1, x);
            count += 2;
        }
        
        // 第一列和最后一列 (排除角落，避免重复计数)
        for (int y = 1; y < rows - 1; ++y) {
            sum += roiDouble.at<double>(y, 0);
            sum += roiDouble.at<double>(y, cols - 1);
            count += 2;
        }
        
        background = sum / count;
    }
    
    // 去背景
    preprocessed = cv::max(roiDouble - background, 0.0);
    
    // 应用阈值
    double maxVal;
    cv::minMaxLoc(preprocessed, nullptr, &maxVal);
    double threshold = maxVal * params_.thresholdRatio;
    
    // 二值化处理：低于阈值的设为 0
    cv::Mat mask = preprocessed >= threshold;
    preprocessed.setTo(0.0, ~mask);
    
    // 饱和像素处理
    if (roi.depth() == CV_8U) {
        double saturationLevel = 255.0 * params_.saturationRatio;
        cv::Mat satMask = roiDouble >= saturationLevel;
        preprocessed.setTo(0.0, satMask);
    } else if (roi.depth() == CV_16U) {
        double saturationLevel = 65535.0 * params_.saturationRatio;
        cv::Mat satMask = roiDouble >= saturationLevel;
        preprocessed.setTo(0.0, satMask);
    }
    
    return background;
}

bool SpotDetectionWorker::calculateWeightedCentroid(const cv::Mat &preprocessed, 
                                                     const QRect &roiRect,
                                                     double &cx, double &cy,
                                                     double &energy, int &validPixels,
                                                     double &variance)
{
    double sumX = 0.0;
    double sumY = 0.0;
    double sumWeights = 0.0;
    energy = 0.0;
    validPixels = 0;
    
    int rows = preprocessed.rows;
    int cols = preprocessed.cols;
    
    // 计算加权质心和能量
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            double intensity = preprocessed.at<double>(y, x);
            
            if (intensity > 0.0) {
                validPixels++;
                energy += intensity;
                
                // 权重：使用平方权重 (I^2) 更抗噪
                double weight = params_.useSquareWeights ? (intensity * intensity) : intensity;
                
                // 全局坐标
                double globalX = roiRect.x() + x;
                double globalY = roiRect.y() + y;
                
                sumX += globalX * weight;
                sumY += globalY * weight;
                sumWeights += weight;
            }
        }
    }
    
    if (sumWeights <= 0.0 || validPixels < params_.minValidPixels) {
        return false;
    }
    
    // 计算质心
    cx = sumX / sumWeights;
    cy = sumY / sumWeights;
    
    // 计算二阶矩 (方差)
    double sumVariance = 0.0;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            double intensity = preprocessed.at<double>(y, x);
            
            if (intensity > 0.0) {
                double globalX = roiRect.x() + x;
                double globalY = roiRect.y() + y;
                
                double dx = globalX - cx;
                double dy = globalY - cy;
                
                sumVariance += (dx * dx + dy * dy) * intensity;
            }
        }
    }
    
    variance = (energy > 0.0) ? (sumVariance / energy) : 0.0;
    
    return true;
}

double SpotDetectionWorker::assessMeasurementQuality(double energy, int validPixels, double variance)
{
    // 基准测量噪声
    double R = params_.baseR;
    
    // 能量低 → R 增大
    if (energy < params_.minEnergy) {
        R *= (params_.minEnergy / std::max(energy, 1.0));
    }
    
    // 有效像素少 → R 增大
    if (validPixels < params_.minValidPixels * 2) {
        R *= 2.0;
    }
    
    // 方差异常 → R 增大
    if (variance > params_.maxVariance) {
        R *= (variance / params_.maxVariance);
    }
    
    // 确保 R 不会过小或过大
    R = std::max(R, params_.baseR * 0.1);
    R = std::min(R, params_.baseR * 100.0);
    
    return R;
}

void SpotDetectionWorker::kalmanUpdate(double measuredX, double measuredY, double R)
{
    // 测量矩阵 H: 只观测位置 [cx, cy]
    cv::Mat H = cv::Mat::zeros(2, 4, CV_64F);
    H.at<double>(0, 0) = 1.0;  // 观测 cx
    H.at<double>(1, 1) = 1.0;  // 观测 cy
    
    // 测量噪声协方差矩阵
    cv::Mat R_mat = cv::Mat::eye(2, 2, CV_64F) * R;
    
    // 测量值
    cv::Mat z = (cv::Mat_<double>(2, 1) << measuredX, measuredY);
    
    // 计算 Kalman 增益: K = P * H^T * (H * P * H^T + R)^{-1}
    cv::Mat S = H * covariance_ * H.t() + R_mat;
    cv::Mat K = covariance_ * H.t() * S.inv();
    
    // 更新状态: x = x + K * (z - H * x)
    cv::Mat innovation = z - H * state_;
    state_ = state_ + K * innovation;
    
    // 更新协方差: P = (I - K * H) * P
    cv::Mat I = cv::Mat::eye(4, 4, CV_64F);
    covariance_ = (I - K * H) * covariance_;
}
