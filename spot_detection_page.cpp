#include "image_algorithm_dock.h"
#include "spot_detection_worker.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QHBoxLayout>

#if ENABLE_SPOT_DETECTION
QWidget* ImageAlgorithmDock::createSpotDetectionPage()
{
    QWidget *page = new QWidget(stackedPages_);
    page->setMinimumWidth(0);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(15);
    layout->setContentsMargins(10, 10, 10, 10);

    QLabel *descLabel = new QLabel(tr("光斑检测使用 Kalman 滤波进行高精度中心点追踪。\n"
                                      "实现预测驱动 ROI、加权质心计算和动态测量质量评估。"), page);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #b4b4b4; padding: 10px; background-color: #2d2d30; border-radius: 4px; border: 1px solid #3e3e42;");
    layout->addWidget(descLabel);

    QGroupBox *roiGroup = new QGroupBox(tr("ROI 参数"), page);
    QFormLayout *roiLayout = new QFormLayout(roiGroup);
    
    spinRoiSize_ = new QSpinBox(roiGroup);
    spinRoiSize_->setRange(8, 32);
    spinRoiSize_->setSingleStep(4);
    spinRoiSize_->setValue(12);
    spinRoiSize_->setToolTip(tr("ROI 尺寸 (8x8, 12x12, 16x16 等)"));
    roiLayout->addRow(tr("ROI 尺寸:"), spinRoiSize_);
    
    layout->addWidget(roiGroup);

    QGroupBox *preprocessGroup = new QGroupBox(tr("预处理参数"), page);
    QFormLayout *preprocessLayout = new QFormLayout(preprocessGroup);
    
    spinThresholdRatio_ = new QDoubleSpinBox(preprocessGroup);
    spinThresholdRatio_->setRange(0.1, 0.5);
    spinThresholdRatio_->setSingleStep(0.05);
    spinThresholdRatio_->setDecimals(2);
    spinThresholdRatio_->setValue(0.25);
    spinThresholdRatio_->setToolTip(tr("阈值比例 (0.2~0.3 推荐)"));
    preprocessLayout->addRow(tr("阈值比例:"), spinThresholdRatio_);
    
    spinSaturationRatio_ = new QDoubleSpinBox(preprocessGroup);
    spinSaturationRatio_->setRange(0.8, 1.0);
    spinSaturationRatio_->setSingleStep(0.05);
    spinSaturationRatio_->setDecimals(2);
    spinSaturationRatio_->setValue(0.95);
    spinSaturationRatio_->setToolTip(tr("饱和像素判定比例"));
    preprocessLayout->addRow(tr("饱和比例:"), spinSaturationRatio_);
    
    chkBackgroundRemoval_ = new QCheckBox(tr("启用背景去除"), preprocessGroup);
    chkBackgroundRemoval_->setChecked(true);
    preprocessLayout->addRow(QString(), chkBackgroundRemoval_);
    
    chkSquareWeights_ = new QCheckBox(tr("使用平方权重 (I²)"), preprocessGroup);
    chkSquareWeights_->setChecked(true);
    preprocessLayout->addRow(QString(), chkSquareWeights_);
    
    layout->addWidget(preprocessGroup);

    QGroupBox *kalmanGroup = new QGroupBox(tr("Kalman 滤波参数"), page);
    QFormLayout *kalmanLayout = new QFormLayout(kalmanGroup);
    
    spinDt_ = new QDoubleSpinBox(kalmanGroup);
    spinDt_->setRange(0.001, 1.0);
    spinDt_->setSingleStep(0.01);
    spinDt_->setDecimals(3);
    spinDt_->setValue(0.033);
    spinDt_->setToolTip(tr("时间间隔 (秒), 默认 30Hz = 0.033s"));
    kalmanLayout->addRow(tr("时间间隔 (dt):"), spinDt_);
    
    spinProcessNoise_ = new QDoubleSpinBox(kalmanGroup);
    spinProcessNoise_->setRange(0.1, 100.0);
    spinProcessNoise_->setSingleStep(0.1);
    spinProcessNoise_->setDecimals(1);
    spinProcessNoise_->setValue(1.0);
    spinProcessNoise_->setToolTip(tr("过程噪声"));
    kalmanLayout->addRow(tr("过程噪声:"), spinProcessNoise_);
    
    spinBaseR_ = new QDoubleSpinBox(kalmanGroup);
    spinBaseR_->setRange(0.1, 100.0);
    spinBaseR_->setSingleStep(0.1);
    spinBaseR_->setDecimals(1);
    spinBaseR_->setValue(1.0);
    spinBaseR_->setToolTip(tr("基准测量噪声"));
    kalmanLayout->addRow(tr("基准 R:"), spinBaseR_);
    
    layout->addWidget(kalmanGroup);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    btnStartSpotDetection_ = new QPushButton(tr("开始检测"), page);
    btnStartSpotDetection_->setMinimumHeight(40);
    btnStartSpotDetection_->setStyleSheet("QPushButton { font-weight: bold; background-color: #0e7a0d; color: white; } QPushButton:hover { background-color: #10a80d; }");
    btnLayout->addWidget(btnStartSpotDetection_);
    
    btnStopSpotDetection_ = new QPushButton(tr("停止检测"), page);
    btnStopSpotDetection_->setMinimumHeight(40);
    btnStopSpotDetection_->setEnabled(false);
    btnStopSpotDetection_->setStyleSheet("QPushButton { font-weight: bold; }");
    btnLayout->addWidget(btnStopSpotDetection_);
    
    btnResetKalman_ = new QPushButton(tr("重置 Kalman"), page);
    btnResetKalman_->setMinimumHeight(40);
    btnLayout->addWidget(btnResetKalman_);
    
    layout->addLayout(btnLayout);

    QGroupBox *resultGroup = new QGroupBox(tr("检测结果"), page);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    
    lblSpotResult_ = new QLabel(tr("等待开始检测..."), resultGroup);
    lblSpotResult_->setStyleSheet("font-family: monospace; padding: 5px;");
    lblSpotResult_->setWordWrap(true);
    resultLayout->addWidget(lblSpotResult_);
    
    lblSpotStats_ = new QLabel(QString(), resultGroup);
    lblSpotStats_->setStyleSheet("color: #a0a0a0; font-size: 10px; padding: 5px;");
    lblSpotStats_->setWordWrap(true);
    resultLayout->addWidget(lblSpotStats_);
    
    layout->addWidget(resultGroup);

    connect(btnStartSpotDetection_, &QPushButton::clicked, this, [this]() {
        if (!threadManager_) return;
        
        SpotDetectionParams params;
        params.roiSize = spinRoiSize_->value();
        params.thresholdRatio = spinThresholdRatio_->value();
        params.saturationRatio = spinSaturationRatio_->value();
        params.useBackgroundRemoval = chkBackgroundRemoval_->isChecked();
        params.useSquareWeights = chkSquareWeights_->isChecked();
        params.dt = spinDt_->value();
        params.processNoise = spinProcessNoise_->value();
        params.baseR = spinBaseR_->value();
        
        threadManager_->spotDetectionWorker()->setParams(params);
        
        spotDetectionRunning_ = true;
        spotDetectionFirstFrame_ = true;
        btnStartSpotDetection_->setEnabled(false);
        btnStopSpotDetection_->setEnabled(true);
        lblSpotResult_->setText(tr("检测运行中..."));
    });
    
    connect(btnStopSpotDetection_, &QPushButton::clicked, this, [this]() {
        spotDetectionRunning_ = false;
        spotDetectionFirstFrame_ = true;
        btnStartSpotDetection_->setEnabled(true);
        btnStopSpotDetection_->setEnabled(false);
        lblSpotResult_->setText(tr("检测已停止"));
    });
    
    connect(btnResetKalman_, &QPushButton::clicked, this, [this]() {
        if (threadManager_) {
            threadManager_->spotDetectionWorker()->reset();
            spotDetectionFirstFrame_ = true;
            lblSpotResult_->setText(tr("Kalman 滤波器已重置"));
        }
    });
    
    if (threadManager_) {
        connect(threadManager_->spotDetectionWorker(), &SpotDetectionWorker::detectionFinished,
                this, [this](SpotDetectionResult result) {
            if (!spotDetectionRunning_) return;
            
            QString resultText = QString(
                "中心点: (%.2f, %.2f)\n"
                "速度: (%.3f, %.3f) px/frame\n"
                "ROI: [%1, %2, %3×%4]\n"
                "能量: %.1f\n"
                "有效像素: %5\n"
                "方差: %.2f\n"
                "测量 R: %.2f"
            ).arg(result.centerX, 0, 'f', 2)
             .arg(result.centerY, 0, 'f', 2)
             .arg(result.velocityX, 0, 'f', 3)
             .arg(result.velocityY, 0, 'f', 3)
             .arg(result.roi.x())
             .arg(result.roi.y())
             .arg(result.roi.width())
             .arg(result.roi.height())
             .arg(result.energy, 0, 'f', 1)
             .arg(result.validPixelCount)
             .arg(result.variance, 0, 'f', 2)
             .arg(result.measurementR, 0, 'f', 2);
            
            lblSpotResult_->setText(resultText);
            
            QString statsText = QString("处理时间: %1 µs").arg(result.processingTimeUs);
            lblSpotStats_->setText(statsText);
        });
        
        connect(threadManager_->spotDetectionWorker(), &SpotDetectionWorker::detectionError,
                this, [this](QString error) {
            lblSpotResult_->setText(tr("错误: %1").arg(error));
            lblSpotStats_->setText(QString());
        });
    }

    layout->addStretch();
    page->setLayout(layout);
    return page;
}
#endif
