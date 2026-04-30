#include "image_algorithm_display.h"
#include <QCloseEvent>
#include <QFrame>

ImageAlgorithmDisplay::ImageAlgorithmDisplay(const QString &algorithmName, QWidget *parent)
    : QWidget(parent), m_algorithmName(algorithmName)
{
    setupUI();
    setWindowTitle(tr("%1 - 实时显示").arg(algorithmName));
    resize(1400, 700);
    
    // 设置默认固定在顶层，与混合处理窗口一致
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
}

ImageAlgorithmDisplay::~ImageAlgorithmDisplay()
{
}

void ImageAlgorithmDisplay::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    
    // Image comparison display area
    imagesLayout_ = new QHBoxLayout();
    
    // Original image display
    QWidget *originalGroup = new QWidget(this);
    QVBoxLayout *originalLayout = new QVBoxLayout(originalGroup);
    originalLayout->setContentsMargins(0, 0, 0, 0);
    
    // Title
    QLabel *originalTitle = new QLabel(tr("原始图像"), originalGroup);
    originalTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    originalTitle->setAlignment(Qt::AlignCenter);
    originalLayout->addWidget(originalTitle);
    
    originalImageWidget_ = new ZoomableImageWidget(this);
    originalImageWidget_->setMinimumSize(400, 300);
    
    originalInfoLabel_ = new QLabel(tr("等待图像..."));
    originalInfoLabel_->setAlignment(Qt::AlignCenter);
    
    originalLayout->addWidget(originalImageWidget_, 1);
    originalLayout->addWidget(originalInfoLabel_);
    
    // Processed image display
    QWidget *processedGroup = new QWidget(this);
    QVBoxLayout *processedLayout = new QVBoxLayout(processedGroup);
    processedLayout->setContentsMargins(0, 0, 0, 0);
    
    // Title
    QLabel *processedTitle = new QLabel(tr("处理后图像 (%1)").arg(m_algorithmName), processedGroup);
    processedTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    processedTitle->setAlignment(Qt::AlignCenter);
    processedLayout->addWidget(processedTitle);
    
    processedImageWidget_ = new ZoomableImageWidget(this);
    processedImageWidget_->setMinimumSize(400, 300);
    
    processedInfoLabel_ = new QLabel(tr("等待图像..."));
    processedInfoLabel_->setAlignment(Qt::AlignCenter);
    
    processedLayout->addWidget(processedImageWidget_, 1);
    processedLayout->addWidget(processedInfoLabel_);
    
    imagesLayout_->addWidget(originalGroup);
    imagesLayout_->addWidget(processedGroup);
    
    // Synchronize zoom and pan
    connect(originalImageWidget_, &ZoomableImageWidget::transformChanged,
            processedImageWidget_, &ZoomableImageWidget::setTransform);
    connect(processedImageWidget_, &ZoomableImageWidget::transformChanged,
            originalImageWidget_, &ZoomableImageWidget::setTransform);
    
    mainLayout_->addLayout(imagesLayout_, 1);
    
    // Separator line
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout_->addWidget(line);
    
    // Statistics area
    QHBoxLayout *statisticsLayout = new QHBoxLayout();
    
    processingTimeLabel_ = new QLabel(tr("处理耗时: -- ms"));
    fpsLabel_ = new QLabel(tr("帧率: -- fps"));
    errorLabel_ = new QLabel();
    errorLabel_->setStyleSheet("QLabel { color: red; }");

    // Frame rate control (placed in statistics bar)
    frameRateLabel_ = new QLabel(tr("限速:"), this);
    frameRateCombo_ = new QComboBox(this);
    frameRateCombo_->addItem(tr("30 帧"), 30);
    frameRateCombo_->addItem(tr("60 帧"), 60);
    frameRateCombo_->addItem(tr("120 帧"), 120);
    frameRateCombo_->addItem(tr("自定义"), -1);
    frameRateCombo_->addItem(tr("无限制"), 0);
    frameRateCombo_->setCurrentIndex(0); // default 30

    customFpsSpin_ = new QSpinBox(this);
    customFpsSpin_->setRange(1, 1000);
    customFpsSpin_->setValue(30);
    customFpsSpin_->setSuffix(tr(" 帧"));
    customFpsSpin_->setVisible(false);
    
    statisticsLayout->addWidget(processingTimeLabel_);
    statisticsLayout->addWidget(fpsLabel_);
    statisticsLayout->addSpacing(8);
    statisticsLayout->addWidget(frameRateLabel_);
    statisticsLayout->addWidget(frameRateCombo_);
    statisticsLayout->addWidget(customFpsSpin_);
    statisticsLayout->addWidget(errorLabel_);
    statisticsLayout->addStretch();
    
    mainLayout_->addLayout(statisticsLayout);
    
    // Connect signals
    connect(frameRateCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        const int data = frameRateCombo_->itemData(idx).toInt();
        const bool isCustom = (data == -1);
        customFpsSpin_->setVisible(isCustom);
        emitCurrentFpsLimit();
    });
    connect(customFpsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        emitCurrentFpsLimit();
    });

    // Emit default limit once
    emitCurrentFpsLimit();
    
    setLayout(mainLayout_);
}

void ImageAlgorithmDisplay::emitCurrentFpsLimit()
{
    if (!frameRateCombo_) {
        return;
    }

    const int data = frameRateCombo_->currentData().toInt();
    int fpsLimit = 0;
    if (data == -1) {
        fpsLimit = customFpsSpin_ ? customFpsSpin_->value() : 30;
    } else {
        fpsLimit = data; // 0 means unlimited
    }
    emit frameRateLimitChanged(fpsLimit);
}

void ImageAlgorithmDisplay::updateImages(const QImage &processedImage, const QImage &originalImage)
{
    currentProcessedImage_ = processedImage;
    currentOriginalImage_ = originalImage;
    
    // Display original image
    if (!originalImage.isNull()) {
        originalImageWidget_->setImage(originalImage);
        originalInfoLabel_->setText(tr("尺寸: %1 x %2")
                                   .arg(originalImage.width())
                                   .arg(originalImage.height()));
    }
    
    // Display processed image
    if (!processedImage.isNull()) {
        processedImageWidget_->setImage(processedImage);
        processedInfoLabel_->setText(tr("尺寸: %1 x %2")
                                    .arg(processedImage.width())
                                    .arg(processedImage.height()));
    }
    
    // Clear error message
    errorLabel_->clear();
}

void ImageAlgorithmDisplay::updateStatistics(double processingTime, double fps)
{
    processingTimeLabel_->setText(tr("处理耗时: %1 ms").arg(processingTime, 0, 'f', 2));
    fpsLabel_->setText(tr("帧率: %1 fps").arg(fps, 0, 'f', 2));
}

void ImageAlgorithmDisplay::showError(const QString &error)
{
    errorLabel_->setText(tr("错误: %1").arg(error));
}

void ImageAlgorithmDisplay::closeEvent(QCloseEvent *event)
{
    emit windowClosed();
    event->accept();
}
