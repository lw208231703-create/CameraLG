#include "histogram_equalization_display.h"
#include <QCloseEvent>

HistogramEqualizationDisplay::HistogramEqualizationDisplay(QWidget *parent)
    : QWidget(parent), isPinned_(false)
{
    setupUI();
    setWindowTitle(tr("直方图均衡化 - 实时显示"));
    resize(1400, 700);
    
   
}

HistogramEqualizationDisplay::~HistogramEqualizationDisplay()
{
}

void HistogramEqualizationDisplay::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    
    // 图像对比显示区域
    imagesLayout_ = new QHBoxLayout();
    
    // 原始图像显示
    QWidget *originalGroup = new QWidget(this);
    QVBoxLayout *originalLayout = new QVBoxLayout(originalGroup);
    originalLayout->setContentsMargins(0, 0, 0, 0);
    
    // 标题
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
    
    // 均衡化图像显示
    QWidget *equalizedGroup = new QWidget(this);
    QVBoxLayout *equalizedLayout = new QVBoxLayout(equalizedGroup);
    equalizedLayout->setContentsMargins(0, 0, 0, 0);
    
    // 标题
    QLabel *equalizedTitle = new QLabel(tr("均衡化后图像"), equalizedGroup);
    equalizedTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    equalizedTitle->setAlignment(Qt::AlignCenter);
    equalizedLayout->addWidget(equalizedTitle);
    
    equalizedImageWidget_ = new ZoomableImageWidget(this);
    equalizedImageWidget_->setMinimumSize(400, 300);
    
    equalizedInfoLabel_ = new QLabel(tr("等待图像..."));
    equalizedInfoLabel_->setAlignment(Qt::AlignCenter);
    
    equalizedLayout->addWidget(equalizedImageWidget_, 1);
    equalizedLayout->addWidget(equalizedInfoLabel_);
    
    imagesLayout_->addWidget(originalGroup);
    imagesLayout_->addWidget(equalizedGroup);
    
    // 同步缩放和平移
    connect(originalImageWidget_, &ZoomableImageWidget::transformChanged,
            equalizedImageWidget_, &ZoomableImageWidget::setTransform);
    connect(equalizedImageWidget_, &ZoomableImageWidget::transformChanged,
            originalImageWidget_, &ZoomableImageWidget::setTransform);
    
    mainLayout_->addLayout(imagesLayout_, 1);
    
    // 分隔线
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout_->addWidget(line);
    
    // 统计信息区域
    QHBoxLayout *statisticsLayout = new QHBoxLayout();
    
    processingTimeLabel_ = new QLabel(tr("处理耗时: -- ms"));
    fpsLabel_ = new QLabel(tr("帧率: -- fps"));
    errorLabel_ = new QLabel();
    errorLabel_->setStyleSheet("QLabel { color: red; }");
    
    statisticsLayout->addWidget(processingTimeLabel_);
    statisticsLayout->addWidget(fpsLabel_);
    statisticsLayout->addWidget(errorLabel_);
    statisticsLayout->addStretch();
    
    mainLayout_->addLayout(statisticsLayout);
    
    // 控制按钮
    buttonLayout_ = new QHBoxLayout();
    
    pinButton_ = new QPushButton(tr("固定"), this);
    closeButton_ = new QPushButton(tr("关闭"), this);
    
    buttonLayout_->addStretch();
    buttonLayout_->addWidget(pinButton_);
    buttonLayout_->addWidget(closeButton_);
    
    mainLayout_->addLayout(buttonLayout_);
    
    // 连接信号槽
    connect(closeButton_, &QPushButton::clicked, this, &QWidget::close);
    connect(pinButton_, &QPushButton::clicked, this, &HistogramEqualizationDisplay::togglePin);
    
    setLayout(mainLayout_);
}

void HistogramEqualizationDisplay::updateImages(const QImage &equalizedImage, const QImage &originalImage)
{
    currentEqualizedImage_ = equalizedImage;
    currentOriginalImage_ = originalImage;
    
    // 显示原始图像
    if (!originalImage.isNull()) {
        originalImageWidget_->setImage(originalImage);
        originalInfoLabel_->setText(tr("尺寸: %1 x %2")
                                   .arg(originalImage.width())
                                   .arg(originalImage.height()));
    }
    
    // 显示均衡化图像
    if (!equalizedImage.isNull()) {
        equalizedImageWidget_->setImage(equalizedImage);
        equalizedInfoLabel_->setText(tr("尺寸: %1 x %2")
                                    .arg(equalizedImage.width())
                                    .arg(equalizedImage.height()));
    }
    
    // 清除错误信息
    errorLabel_->clear();
}

void HistogramEqualizationDisplay::updateStatistics(double processingTime, double fps)
{
    processingTimeLabel_->setText(tr("处理耗时: %1 ms").arg(processingTime, 0, 'f', 2));
    fpsLabel_->setText(tr("帧率: %1 fps").arg(fps, 0, 'f', 2));
}

void HistogramEqualizationDisplay::showError(const QString &error)
{
    errorLabel_->setText(tr("错误: %1").arg(error));
}

void HistogramEqualizationDisplay::togglePin()
{
    if (isPinned_) {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        isPinned_ = false;
        pinButton_->setText(tr("固定"));
    } else {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        isPinned_ = true;
        pinButton_->setText(tr("取消固定"));
    }
    show(); // 重新显示以应用窗口标志
}

void HistogramEqualizationDisplay::closeEvent(QCloseEvent *event)
{
    emit windowClosed();
    event->accept();
}
