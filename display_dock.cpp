#include "display_dock.h"
#include "bit_depth_slider.h"
#include "editable_number_label.h"
#include "image_info_worker.h"
#include "image_display_label.h"
#include "icon_cache.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QApplication>
#include <QPushButton>
#include <QToolButton>
#include <QSizePolicy>
#include <QLabel>
#include <QSlider>
#include <QWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QScrollArea>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QComboBox>
#include <QGroupBox>
#include <QMutexLocker>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <utility>
#include <QScrollBar>

// ============ DisplayDock 实现 ============

DisplayDock::DisplayDock(QWidget *parent)
    : QDockWidget(tr("图像显示"), parent)
    , contentWidget_(new QWidget(this))
    , contentLayout_(new QVBoxLayout)
    , bufferCount_(10)
    , currentBufferIndex_(0)
    , displayRefreshIntervalMs_(0) // 默认不限制显示刷新率
{
    setObjectName(QStringLiteral("displayDock"));
    contentLayout_->setContentsMargins(6, 6, 6, 6);
    contentLayout_->setSpacing(6);
    contentLayout_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    contentWidget_->setLayout(contentLayout_);
    // Remove inline background style to allow QSS theme to apply properly
    // contentWidget_->setStyleSheet("background: #efefef;");
    contentWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setWidget(contentWidget_);
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    // 注册自定义类型以便跨线程传递
    qRegisterMetaType<ImageInfo>("ImageInfo");
    
    // 初始化FPS计时器
    fpsTimer_.start();
    lastFpsUpdate_ = 0;

#if DISPLAYDOCK_ENABLE_RENDER_THROTTLE
    // 连续采集显示节流定时器（只在需要时启动）
    renderTimer_ = new QTimer(this);
    renderTimer_->setSingleShot(true);
    connect(renderTimer_, &QTimer::timeout, this, [this]() {
        renderPending_ = false;
        lastUiRenderMs_ = fpsTimer_.elapsed();
        updateDisplayedPixmap();
        requestImageInfoUpdate();
    });
#endif
    
    // 创建图像信息工作线程
    infoThread_ = new QThread(this);
    infoWorker_ = new ImageInfoWorker();
    infoWorker_->moveToThread(infoThread_);
    
    connect(infoWorker_, &ImageInfoWorker::infoUpdated,
            this, &DisplayDock::onImageInfoUpdated, Qt::QueuedConnection);
    
    infoThread_->start();

    setupUI();
    updateDisplayedPixmap(); // Initialize the display label with the placeholder state

    // 单/双击区分：短延迟定时器用于将单击与双击区分开
    clickTimer_ = new QTimer(this);
    clickTimer_->setSingleShot(true);
    // 使用系统双击间隔以便更可靠地区分单击/双击
    clickTimer_->setInterval(QApplication::doubleClickInterval());
    connect(clickTimer_, &QTimer::timeout, this, &DisplayDock::onClickTimeout);
}

DisplayDock::~DisplayDock()
{
    // 停止工作线程
    if (infoWorker_) {
        infoWorker_->stop();
    }
    
    if (infoThread_) {
        infoThread_->quit();
        if (!infoThread_->wait(1000)) {
            // 线程未能在超时时间内退出，强制终止
            infoThread_->terminate();
            infoThread_->wait();
        }
    }
    
    delete infoWorker_;

    if (clickTimer_) {
        clickTimer_->stop();
        delete clickTimer_;
        clickTimer_ = nullptr;
    }
}

void DisplayDock::setBadPixelPickMode(bool enabled)
{
    badPixelPickMode_ = enabled;

    if (enabled) {
        // 避免切换模式时残留单击定时器状态
        if (clickTimer_) {
            clickTimer_->stop();
        }
        hasPendingClick_ = false;
        ignorePendingClick_ = false;

        // 坏点拾取要求“只需点击即可取点”，因此进入模式时强制退出 ROI
        if (isRoiActive_) {
            isRoiActive_ = false;
            currentRoi_ = QRect();
            if (imageLabel_) {
                imageLabel_->setRoiRect(QRect(), false);
            }
            emit roiChanged(QRect(), false);
        }
    }
}

void DisplayDock::setBadPixelMarkers(const QVector<QPoint> &points)
{
    if (!imageLabel_) {
        return;
    }
    imageLabel_->setMarkerPoints(points, true);
}

void DisplayDock::clearBadPixelMarkers()
{
    if (!imageLabel_) {
        return;
    }
    imageLabel_->setMarkerPoints(QVector<QPoint>(), false);
}

void DisplayDock::setupUI()
{
    // 顶部按钮行
    auto *topButtons = new QWidget(contentWidget_);
    auto *topLayout = new QHBoxLayout(topButtons);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(6);

    // 单次捕获按钮
    singleCaptureBtn_ = new QToolButton(topButtons);
    singleCaptureBtn_->setIcon(QIcon(":/icons/Single capture.svg"));
    singleCaptureBtn_->setIconSize(QSize(DISPLAY_BUTTON_SIZE_X, DISPLAY_BUTTON_SIZE_Y));
    singleCaptureBtn_->setToolTip(tr("单次捕获"));
    connect(singleCaptureBtn_, &QToolButton::clicked, this, [this]() {
        emit singleCaptureRequested();
    });
    topLayout->addWidget(singleCaptureBtn_);

    // 连续/停止捕获按钮（切换按钮）
    captureToggleBtn_ = new QToolButton(topButtons);
    updateButtonState();
    connect(captureToggleBtn_, &QToolButton::clicked, this, &DisplayDock::onCaptureToggleClicked);
    topLayout->addWidget(captureToggleBtn_);

    // 保存单张图像按钮
    auto *saveSingleBtn = new QToolButton(topButtons);
    saveSingleBtn->setIcon(IconCache::saveSingleImageIcon());
    saveSingleBtn->setIconSize(QSize(DISPLAY_BUTTON_SIZE_X, DISPLAY_BUTTON_SIZE_Y));
    saveSingleBtn->setToolTip(tr("保存单张图像"));
    connect(saveSingleBtn, &QToolButton::clicked, this, [this]() {
        emit saveSingleImageRequested();
    });
    topLayout->addWidget(saveSingleBtn);

    // 保存多张图像按钮
    auto *saveMultipleBtn = new QToolButton(topButtons);
    saveMultipleBtn->setIcon(QIcon(":/icons/Save_multiple_images.svg"));
    saveMultipleBtn->setIconSize(QSize(DISPLAY_BUTTON_SIZE_X, DISPLAY_BUTTON_SIZE_Y));
    saveMultipleBtn->setToolTip(tr("保存多张图像"));
    connect(saveMultipleBtn, &QToolButton::clicked, this, [this]() {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("保存多张图像"));

        auto *formLayout = new QFormLayout(&dialog);
        formLayout->setLabelAlignment(Qt::AlignRight);

        auto *countSpin = new QSpinBox(&dialog);
        countSpin->setRange(1, 1000);
        countSpin->setValue(10);
        formLayout->addRow(tr("保存张数"), countSpin);

        auto *pathEdit = new QLineEdit(&dialog);
        pathEdit->setPlaceholderText(tr("选择存储路径"));
        pathEdit->setText(QDir::homePath());

        auto *browseBtn = new QPushButton(tr("浏览"), &dialog);
        auto *pathWidget = new QWidget(&dialog);
        auto *pathLayout = new QHBoxLayout(pathWidget);
        pathLayout->setContentsMargins(0, 0, 0, 0);
        pathLayout->setSpacing(6);
        pathLayout->addWidget(pathEdit);
        pathLayout->addWidget(browseBtn);
        formLayout->addRow(tr("存储路径"), pathWidget);

        connect(browseBtn, &QPushButton::clicked, &dialog, [pathEdit, this]() {
            const QString dir = QFileDialog::getExistingDirectory(this, tr("选择存储目录"));
            if (!dir.isEmpty()) {
                pathEdit->setText(dir);
            }
        });

        // 添加格式选择
        auto *formatCombo = new QComboBox(&dialog);
        formatCombo->addItem("PNG (*.png)", "png");
        formatCombo->addItem("TIFF (*.tiff)", "tiff");
        formatCombo->addItem("BMP (*.bmp)", "bmp");
        formatCombo->addItem("JPEG (*.jpg)", "jpg");
        formatCombo->addItem("RAW (*.raw)", "raw");
        formLayout->addRow(tr("保存格式"), formatCombo);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        formLayout->addRow(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const QString directory = pathEdit->text().trimmed();
        if (directory.isEmpty()) {
            QMessageBox::warning(this, tr("路径无效"), tr("请选择一个有效的存储目录。"));
            return;
        }

        QString format = formatCombo->currentData().toString();
        emit saveMultipleImagesRequested(countSpin->value(), directory, format);
    });
    topLayout->addWidget(saveMultipleBtn);

    // 1:1 按钮
    auto *oneToOneBtn = new QToolButton(topButtons);
    oneToOneBtn->setIcon(QIcon(":/icons/one-to-one.svg"));
    oneToOneBtn->setIconSize(QSize(DISPLAY_BUTTON_SIZE_X, DISPLAY_BUTTON_SIZE_Y));
    oneToOneBtn->setToolTip(tr("1:1 显示"));
    connect(oneToOneBtn, &QToolButton::clicked, this, [this]() {
        if (!currentImage_.isNull()) {
            customDisplaySize_ = currentImage_.size();
        } else {
            customDisplaySize_ = QSize();
        }
        scaleFactor_ = 1.0;
        updateDisplayedPixmap();
    });
    topLayout->addWidget(oneToOneBtn);

    // 视图设置按钮（原分辨率设置）
    auto *viewSettingsBtn = new QToolButton(topButtons);
    viewSettingsBtn->setIcon(IconCache::viewSettingsIcon());
    viewSettingsBtn->setIconSize(QSize(DISPLAY_BUTTON_SIZE_X, DISPLAY_BUTTON_SIZE_Y));
    viewSettingsBtn->setToolTip(tr("视图设置"));
    connect(viewSettingsBtn, &QToolButton::clicked, this, [this]() {
        onViewSettingsButtonClicked();
    });
    topLayout->addWidget(viewSettingsBtn);

    // FPS按钮
    auto *fpsBtn = new QToolButton(topButtons);
    fpsBtn->setIcon(QIcon(":/icons/FPS.svg"));
    fpsBtn->setIconSize(QSize(DISPLAY_BUTTON_SIZE_X, DISPLAY_BUTTON_SIZE_Y));
    fpsBtn->setToolTip(tr("FPS"));
    connect(fpsBtn, &QToolButton::clicked, this, [this]() {
        showFpsSettingsDialog();
    });
    topLayout->addWidget(fpsBtn);

    // GigE窗口切换按钮
    gigeToggleBtn_ = new QToolButton(topButtons);
    gigeToggleBtn_->setIcon(QIcon(":/icons/GigE.svg"));
    gigeToggleBtn_->setIconSize(QSize(DISPLAY_BUTTON_SIZE_X, DISPLAY_BUTTON_SIZE_Y));
    gigeToggleBtn_->setToolTip(tr("GigE窗口"));
    gigeToggleBtn_->setCheckable(true);
    connect(gigeToggleBtn_, &QToolButton::clicked, this, [this]() {
        gigeWindowVisible_ = !gigeWindowVisible_;
        emit gigeWindowToggled(gigeWindowVisible_);
    });
    topLayout->addWidget(gigeToggleBtn_);

    topLayout->addStretch();

    contentLayout_->addWidget(topButtons);
    contentLayout_->setAlignment(topButtons, Qt::AlignTop | Qt::AlignLeft);

    // ========== 图像信息状态栏 ==========
    imageInfoBar_ = new QWidget(contentWidget_);
    imageInfoBar_->setStyleSheet("background: transparent; border: none; padding: 0; margin: 0;");
    auto *infoLayout = new QHBoxLayout(imageInfoBar_);
    infoLayout->setContentsMargins(8, 4, 8, 4);
    infoLayout->setSpacing(12);
    
    // 第一个像素值
    auto *firstPixelGroup = new QWidget(imageInfoBar_);
    auto *firstPixelLayout = new QHBoxLayout(firstPixelGroup);
    firstPixelLayout->setContentsMargins(0, 0, 0, 0);
    firstPixelLayout->setSpacing(4);
    auto *firstPixelTitle = new QLabel(tr("首像素:"), firstPixelGroup);
    firstPixelTitle->setStyleSheet("color: #d4d4d4; font-size: 11px;");
    firstPixelLabel_ = new QLabel("--", firstPixelGroup);
    firstPixelLabel_->setStyleSheet("font-weight: bold; color: #d4d4d4; font-size: 11px;");
    firstPixelLabel_->setMinimumWidth(40);
    firstPixelLayout->addWidget(firstPixelTitle);
    firstPixelLayout->addWidget(firstPixelLabel_);
    infoLayout->addWidget(firstPixelGroup);
    
    // 添加分隔线
    auto addSeparator = [infoLayout, imageInfoBar_=imageInfoBar_]() {
        auto *sep = new QLabel("|", imageInfoBar_);
        sep->setStyleSheet(
            "color: #3e3e42;"
            "font-size: 16px;"
            "min-width: 8px;"
            "max-width: 12px;"
            "min-height: 32px;"
            "max-height: 40px;"
            "padding: 0;"
            "margin: 0;"
            "background: transparent;"
            "border: none;"
            "qproperty-alignment: AlignVCenter;"
        );
        sep->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        infoLayout->addWidget(sep);
    };
    
    addSeparator();
    
    // 鼠标坐标
    auto *mousePosGroup = new QWidget(imageInfoBar_);
    auto *mousePosLayout = new QHBoxLayout(mousePosGroup);
    mousePosLayout->setContentsMargins(0, 0, 0, 0);
    mousePosLayout->setSpacing(4);
    auto *mousePosTitle = new QLabel(tr("坐标:"), mousePosGroup);
    mousePosTitle->setStyleSheet("color: #d4d4d4; font-size: 11px;");
    mousePosLabel_ = new QLabel("(--, --)", mousePosGroup);
    mousePosLabel_->setStyleSheet("font-weight: bold; color: #d4d4d4; font-size: 11px;");
    mousePosLabel_->setMinimumWidth(70);
    mousePosLayout->addWidget(mousePosTitle);
    mousePosLayout->addWidget(mousePosLabel_);
    infoLayout->addWidget(mousePosGroup);
    
    addSeparator();
    
    // 像素值
    auto *pixelValueGroup = new QWidget(imageInfoBar_);
    auto *pixelValueLayout = new QHBoxLayout(pixelValueGroup);
    pixelValueLayout->setContentsMargins(0, 0, 0, 0);
    pixelValueLayout->setSpacing(4);
    auto *pixelValueTitle = new QLabel(tr("像素值:"), pixelValueGroup);
    pixelValueTitle->setStyleSheet("color: #d4d4d4; font-size: 11px;");
    pixelValueLabel_ = new QLabel("--", pixelValueGroup);
    pixelValueLabel_->setStyleSheet("font-weight: bold; color: #d4d4d4; font-size: 11px;");
    pixelValueLabel_->setMinimumWidth(40);
    pixelValueLayout->addWidget(pixelValueTitle);
    pixelValueLayout->addWidget(pixelValueLabel_);
    infoLayout->addWidget(pixelValueGroup);
    
    addSeparator();
    
    // 帧率
    auto *fpsGroup = new QWidget(imageInfoBar_);
    auto *fpsLayout = new QHBoxLayout(fpsGroup);
    fpsLayout->setContentsMargins(0, 0, 0, 0);
    fpsLayout->setSpacing(4);
    auto *fpsTitle = new QLabel(tr("帧率:"), fpsGroup);
    fpsTitle->setStyleSheet("color: #d4d4d4; font-size: 11px;");
    fpsLabel_ = new QLabel("-- fps", fpsGroup);
    fpsLabel_->setStyleSheet("font-weight: bold; color: #d4d4d4; font-size: 11px;");
    fpsLabel_->setMinimumWidth(55);
    fpsLayout->addWidget(fpsTitle);
    fpsLayout->addWidget(fpsLabel_);
    infoLayout->addWidget(fpsGroup);
    
    addSeparator();
    
    // 图像尺寸（行x列）
    auto *sizeGroup = new QWidget(imageInfoBar_);
    auto *sizeLayout = new QHBoxLayout(sizeGroup);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(4);
    auto *sizeTitle = new QLabel(tr("尺寸:"), sizeGroup);
    sizeTitle->setStyleSheet("color: #d4d4d4; font-size: 11px;");
    imageSizeLabel_ = new QLabel("--x--", sizeGroup);
    imageSizeLabel_->setStyleSheet("font-weight: bold; color: #d4d4d4; font-size: 11px;");
    imageSizeLabel_->setMinimumWidth(65);
    sizeLayout->addWidget(sizeTitle);
    sizeLayout->addWidget(imageSizeLabel_);
    infoLayout->addWidget(sizeGroup);
    
    addSeparator();
    
    // 色彩类型
    auto *colorGroup = new QWidget(imageInfoBar_);
    auto *colorLayout = new QHBoxLayout(colorGroup);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(4);
    auto *colorTitle = new QLabel(tr("类型:"), colorGroup);
    colorTitle->setStyleSheet("color: #d4d4d4; font-size: 11px;");
    colorTypeLabel_ = new QLabel("--", colorGroup);
    colorTypeLabel_->setStyleSheet("font-weight: bold; color: #d4d4d4; font-size: 11px;");
    colorTypeLabel_->setMinimumWidth(60);
    colorLayout->addWidget(colorTitle);
    colorLayout->addWidget(colorTypeLabel_);
    infoLayout->addWidget(colorGroup);
    
    addSeparator();
    
    // 色深
    auto *depthGroup = new QWidget(imageInfoBar_);
    auto *depthLayout = new QHBoxLayout(depthGroup);
    depthLayout->setContentsMargins(0, 0, 0, 0);
    depthLayout->setSpacing(4);
    auto *depthTitle = new QLabel(tr("色深:"), depthGroup);
    depthTitle->setStyleSheet("color: #d4d4d4; font-size: 11px;");
    bitDepthLabel_ = new QLabel("-- bit", depthGroup);
    bitDepthLabel_->setStyleSheet("font-weight: bold; color: #d4d4d4; font-size: 11px;");
    bitDepthLabel_->setMinimumWidth(45);
    depthLayout->addWidget(depthTitle);
    depthLayout->addWidget(bitDepthLabel_);
    infoLayout->addWidget(depthGroup);
    
    infoLayout->addStretch();
    contentLayout_->addWidget(imageInfoBar_);

    // Image display area with scrollbars when content exceeds viewport
    imageScrollArea_ = new QScrollArea(contentWidget_);
    imageScrollArea_->setWidgetResizable(false);
    imageScrollArea_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    imageScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    imageScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    imageScrollArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
#if DISPLAYDOCK_ENABLE_BACKGROUND
    // Prepare tiled background
    QPixmap bgPixmap(":/icons/Curtain_Fill.png");
    if (!bgPixmap.isNull()) {
        // Scale to 100x100
        backgroundPixmap_ = bgPixmap.scaled(DISPLAY_BG_FILL_X, DISPLAY_BG_FILL_Y, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        // Install event filter to draw background manually
        imageScrollArea_->viewport()->installEventFilter(this);
        // Ensure viewport doesn't paint its own background over ours
        imageScrollArea_->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
        imageScrollArea_->viewport()->setAutoFillBackground(false);
    }
#else
    // When background image is disabled, ensure viewport follows QSS theme
    imageScrollArea_->viewport()->setAutoFillBackground(true);
#endif

    imageLabel_ = new ImageDisplayLabel(contentWidget_);
    connect(imageLabel_, &ImageDisplayLabel::wheelZoomRequested, this, &DisplayDock::onWheelZoomRequested);
    connect(imageLabel_, &ImageDisplayLabel::mouseMoved, this, &DisplayDock::onMouseMoved);
    connect(imageLabel_, &ImageDisplayLabel::mouseLeft, this, &DisplayDock::onMouseLeft);
    connect(imageLabel_, &ImageDisplayLabel::mouseClicked, this, &DisplayDock::onMouseClicked);
    connect(imageLabel_, &ImageDisplayLabel::mouseDoubleClicked, this, &DisplayDock::onMouseDoubleClicked);
    connect(imageLabel_, &ImageDisplayLabel::roiChanged, this, &DisplayDock::onRoiChanged);
    connect(imageLabel_, &ImageDisplayLabel::panRequested, this, &DisplayDock::onPanRequested);
    imageLabel_->setMinimumSize(defaultDisplaySize_);
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setStyleSheet("background: black; color: white; border: 1px solid #ccc;");
    imageLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    imageLabel_->setFixedSize(defaultDisplaySize_);

    imageScrollArea_->setWidget(imageLabel_);
    contentLayout_->addWidget(imageScrollArea_, 1);

    // Buffer selection slider area
    auto *bufferControlWidget = new QWidget(contentWidget_);
    auto *bufferControlLayout = new QHBoxLayout(bufferControlWidget);
    bufferControlLayout->setContentsMargins(0, 6, 0, 0);
    bufferControlLayout->setSpacing(10);

    // 缓冲区索引滑块
    auto *bufferSliderLabel = new QLabel(tr("缓冲区:"), bufferControlWidget);
    bufferControlLayout->addWidget(bufferSliderLabel);
    bufferSlider_ = new QSlider(Qt::Horizontal, bufferControlWidget);
    bufferSlider_->setMinimum(0);
    bufferSlider_->setMaximum(bufferCount_ - 1);
    bufferSlider_->setValue(0);
    bufferSlider_->setTickPosition(QSlider::TicksBelow);
    bufferSlider_->setTickInterval(1);
    bufferSlider_->setMinimumWidth(200);
    connect(bufferSlider_, &QSlider::valueChanged, this, &DisplayDock::onBufferSliderChanged);
    bufferControlLayout->addWidget(bufferSlider_);
    bufferInfoLabel_ = new QLabel(QString("1/%1").arg(bufferCount_), bufferControlWidget);
    bufferInfoLabel_->setMinimumWidth(40);
    bufferControlLayout->addWidget(bufferInfoLabel_);
    bufferControlLayout->addSpacing(20);

    // 缓冲区数量滑块
    auto *countLabel = new QLabel(tr("缓冲区数量:"), bufferControlWidget);
    bufferControlLayout->addWidget(countLabel);
    bufferCountSlider_ = new QSlider(Qt::Horizontal, bufferControlWidget);
    bufferCountSlider_->setMinimum(1);
    bufferCountSlider_->setMaximum(10);
    bufferCountSlider_->setValue(bufferCount_);
    bufferCountSlider_->setTickPosition(QSlider::TicksBelow);
    bufferCountSlider_->setTickInterval(1);
    bufferCountSlider_->setMinimumWidth(150);
    connect(bufferCountSlider_, &QSlider::valueChanged, this, &DisplayDock::onBufferCountSliderChanged);
    bufferControlLayout->addWidget(bufferCountSlider_);
    
    // 改用 EditableNumberLabel - 支持双击修改缓冲区数量
    bufferCountLabel_ = new EditableNumberLabel(bufferControlWidget);
    bufferCountLabel_->setValue(bufferCount_, 1, 100);  // 允许 1-100 个缓冲区
    bufferCountLabel_->setMinimumWidth(30);
    bufferCountLabel_->setToolTip(tr("双击输入新的缓冲区数量"));
    connect(bufferCountLabel_, &EditableNumberLabel::valueChanged,
            this, [this](int newCount) {
        // 防止重复设置相同的值
        if (newCount == bufferCount_) {
            return;
        }
        
        // 当用户通过编辑框修改数量时，更新缓冲区数量
        setBufferCount(newCount);
        
        // 更新缓冲区数量滑块：无条件更新最大值（因为是用户主动改的）
        // 这与滑块拖动的情况不同，滑块拖动时不应该改变最大值
        bufferCountSlider_->blockSignals(true);
        bufferCountSlider_->setMaximum(newCount);  // 编辑框修改时，总是设置最大值
        bufferCountSlider_->setValue(newCount);
        bufferCountSlider_->blockSignals(false);
        
        emit bufferCountChanged(newCount);
    });
    bufferControlLayout->addWidget(bufferCountLabel_);
    bufferControlLayout->addSpacing(20);

    // 位深选择滑块
    QHBoxLayout *sliderLayout = new QHBoxLayout();
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *msbLabel = new QLabel("MSB", bufferControlWidget);
    sliderLayout->addWidget(msbLabel);

    bitShiftSlider_ = new BitDepthSlider(bufferControlWidget);
    bitShiftSlider_->setValue(6); // 默认值 6-13位
    bitShiftSlider_->setMinimumWidth(300);
    connect(bitShiftSlider_, &QSlider::valueChanged, this, &DisplayDock::onBitShiftSliderChanged);
    sliderLayout->addWidget(bitShiftSlider_);

    QLabel *lsbLabel = new QLabel("LSB", bufferControlWidget);
    sliderLayout->addWidget(lsbLabel);

    bufferControlLayout->addLayout(sliderLayout);
    bufferControlLayout->addStretch();
    contentLayout_->addWidget(bufferControlWidget);
}

QVBoxLayout *DisplayDock::contentLayout() const
{
    return contentLayout_;
}

void DisplayDock::displayImage(const QImage &image)
{
    if (image.isNull()) {
        currentPixmap_ = QPixmap();
        currentImage_ = QImage();
        scaleFactor_ = 1.0;
        updateDisplayedPixmap();
        return;
    }

    // 保存原始图像
    currentImage_ = image;

#if !DISPLAYDOCK_ENABLE_PARTIAL_PAINT
    // 旧渲染路径：依赖 QPixmap 进行 scaled
    currentPixmap_ = QPixmap::fromImage(image);
#endif
    
    // 计算FPS
    frameCount_++;
    qint64 elapsed = fpsTimer_.elapsed();
    if (elapsed - lastFpsUpdate_ >= 1000) {  // 每秒更新一次FPS
        currentFps_ = frameCount_ * 1000.0 / (elapsed - lastFpsUpdate_);
        frameCount_ = 0;
        lastFpsUpdate_ = elapsed;
    }
    
    // 获取第一个像素值（用于文件命名和显示）
    // 优先使用原始数据
    if (!currentRawData_.isEmpty() && currentRawWidth_ > 0 && currentRawHeight_ > 0) {
        if (currentRawData_.size() > 0) {
            firstPixelValue_ = currentRawData_[0];
        }
    } else if (image.width() > 0 && image.height() > 0) {
        if (image.format() == QImage::Format_Grayscale8) {
            firstPixelValue_ = qGray(image.pixel(0, 0));
        } else if (image.format() == QImage::Format_Grayscale16) {
            int pixelValue = getGrayscale16Pixel(image, 0, 0);
            if (pixelValue >= 0) {
                firstPixelValue_ = pixelValue;
            }
        } else {
            QRgb pixel = image.pixel(0, 0);
            firstPixelValue_ = qGray(pixel);
        }
    }

#if DISPLAYDOCK_ENABLE_RENDER_THROTTLE
    // 图像显示节流：如果每帧都更新UI，会导致事件队列堆积甚至卡死。
    // 使用用户设置的显示刷新间隔来节流显示，但不影响采集。
    if (displayRefreshIntervalMs_ > 0) {
        const qint64 now = fpsTimer_.elapsed();

        if ((now - lastUiRenderMs_) < displayRefreshIntervalMs_) {
            renderPending_ = true;
            if (renderTimer_ && !renderTimer_->isActive()) {
                int delay = static_cast<int>(displayRefreshIntervalMs_ - (now - lastUiRenderMs_));
                if (delay < 1) delay = 1;
                renderTimer_->start(delay);
            }
            return;
        }
    }
#endif

    lastUiRenderMs_ = fpsTimer_.elapsed();
    updateDisplayedPixmap();
    requestImageInfoUpdate();
}

void DisplayDock::setRawData(const QVector<uint16_t> &data, int width, int height, int bitDepth)
{
    currentRawData_ = data;
    currentRawWidth_ = width;
    currentRawHeight_ = height;
    currentBitDepth_ = bitDepth;

    if (bitShiftSlider_) {
        bitShiftSlider_->setMaxBitDepth(bitDepth);
    }
}

void DisplayDock::setTrackingCursors(int validity, float x1, float y1, float x2, float y2, float x3, float y3)
{
    if (imageLabel_) {
        imageLabel_->setTrackingCursors(validity, x1, y1, x2, y2, x3, y3);
    }
}

void DisplayDock::setDisplayRefreshRate(int fps)
{
    if (fps <= 0) {
        // 不限制显示刷新率
        displayRefreshIntervalMs_ = 0;
    } else {
        // 计算对应的时间间隔（毫秒）
        displayRefreshIntervalMs_ = qMax(MIN_DISPLAY_INTERVAL_MS, 1000 / fps);
    }
}

void DisplayDock::setBufferCount(int count)
{
    if (count < 1) count = 1;
    if (count > 100) count = 100;  // 允许最多100个缓冲区
    
    bufferCount_ = count;
    // cachedImages_.resize(count); // 移除
    
    bufferSlider_->setMaximum(count - 1);
    if (currentBufferIndex_ >= count) {
        currentBufferIndex_ = count - 1;
        bufferSlider_->setValue(currentBufferIndex_);
    }
    
    bufferInfoLabel_->setText(QString("%1/%2").arg(currentBufferIndex_ + 1).arg(bufferCount_));
    bufferCountLabel_->setText(QString::number(count));
    
    // 注意：这里不再更新 bufferCountSlider_ 的最大值
    // 最大值的更新应该由调用者决定：
    // - 编辑框修改时：在槽函数中无条件更新最大值
    // - 滑块拖动时：在 onBufferCountSliderChanged 中不更新最大值（保持现有范围）
    
    bool oldState = bufferCountSlider_->blockSignals(true);
    bufferCountSlider_->setValue(count);
    bufferCountSlider_->blockSignals(oldState);
}

int DisplayDock::getCurrentBufferIndex() const
{
    return currentBufferIndex_;
}

void DisplayDock::setCurrentBufferIndex(int index)
{
    if (index >= 0 && index < bufferCount_) {
        currentBufferIndex_ = index;
        
        // 阻断信号，防止循环调用或不必要的信号发射
        bool oldState = bufferSlider_->blockSignals(true);
        bufferSlider_->setValue(index);
        bufferSlider_->blockSignals(oldState);
        
        bufferInfoLabel_->setText(QString("%1/%2").arg(index + 1).arg(bufferCount_));
    }
}

// 移除 onImageReady 实现

void DisplayDock::onBufferSliderChanged(int value)
{
    currentBufferIndex_ = value;
    bufferInfoLabel_->setText(QString("%1/%2").arg(value + 1).arg(bufferCount_));
    
    // 不再直接从本地缓存获取图像，而是发射信号通知外部更新
    // if (value >= 0 && value < cachedImages_.size() && !cachedImages_[value].isNull()) {
    //     displayImage(cachedImages_[value]);
    // }
    
    emit bufferIndexChanged(value);
}

void DisplayDock::onBufferCountSliderChanged(int value)
{
    setBufferCount(value);
    emit bufferCountChanged(value);
}

void DisplayDock::onBitShiftSliderChanged(int value)
{
    emit bitShiftChanged(value);
}

void DisplayDock::onViewSettingsButtonClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("视图设置"));
    dialog.setModal(true);

    auto *formLayout = new QFormLayout(&dialog);

    // 获取原始图像尺寸
    int origWidth = currentImage_.isNull() ? defaultDisplaySize_.width() : currentImage_.width();
    int origHeight = currentImage_.isNull() ? defaultDisplaySize_.height() : currentImage_.height();

    const bool hasCustom = customDisplaySize_.isValid();
    QSize baseSize = hasCustom ? customDisplaySize_ : defaultDisplaySize_;

    auto *useCustomCheck = new QCheckBox(tr("启用自定义视图大小"), &dialog);
    useCustomCheck->setChecked(hasCustom);
    formLayout->addRow(useCustomCheck);

    // 显示原始图像尺寸信息
    auto *origSizeLabel = new QLabel(QString(tr("原始图像尺寸: %1 列 x %2 行")).arg(origWidth).arg(origHeight), &dialog);
    origSizeLabel->setStyleSheet("color: #d4d4d4;");
    formLayout->addRow(origSizeLabel);

    // 显示列数（宽度）
    auto *widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(64, 16384);
    widthSpin->setSingleStep(16);
    widthSpin->setValue(baseSize.width());

    // 宽度百分比
    auto *widthPercentSpin = new QDoubleSpinBox(&dialog);
    widthPercentSpin->setRange(1.0, 500.0);
    widthPercentSpin->setSingleStep(10.0);
    widthPercentSpin->setDecimals(1);
    widthPercentSpin->setSuffix("%");
    double currentWidthPercent = (origWidth > 0) ? (baseSize.width() * 100.0 / origWidth) : 100.0;
    widthPercentSpin->setValue(currentWidthPercent);

    // 显示行数（高度）
    auto *heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(64, 16384);
    heightSpin->setSingleStep(16);
    heightSpin->setValue(baseSize.height());

    // 高度百分比
    auto *heightPercentSpin = new QDoubleSpinBox(&dialog);
    heightPercentSpin->setRange(1.0, 500.0);
    heightPercentSpin->setSingleStep(10.0);
    heightPercentSpin->setDecimals(1);
    heightPercentSpin->setSuffix("%");
    double currentHeightPercent = (origHeight > 0) ? (baseSize.height() * 100.0 / origHeight) : 100.0;
    heightPercentSpin->setValue(currentHeightPercent);

    // 启用/禁用控制
    widthSpin->setEnabled(hasCustom);
    widthPercentSpin->setEnabled(hasCustom);
    heightSpin->setEnabled(hasCustom);
    heightPercentSpin->setEnabled(hasCustom);

    connect(useCustomCheck, &QCheckBox::toggled, widthSpin, &QWidget::setEnabled);
    connect(useCustomCheck, &QCheckBox::toggled, widthPercentSpin, &QWidget::setEnabled);
    connect(useCustomCheck, &QCheckBox::toggled, heightSpin, &QWidget::setEnabled);
    connect(useCustomCheck, &QCheckBox::toggled, heightPercentSpin, &QWidget::setEnabled);

    // 宽度联动
    connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), [=](int value) {
        if (origWidth > 0) {
            double percent = value * 100.0 / origWidth;
            widthPercentSpin->blockSignals(true);
            widthPercentSpin->setValue(percent);
            widthPercentSpin->blockSignals(false);
        }
    });

    connect(widthPercentSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double percent) {
        if (origWidth > 0) {
            int newWidth = static_cast<int>(origWidth * percent / 100.0);
            widthSpin->blockSignals(true);
            widthSpin->setValue(newWidth);
            widthSpin->blockSignals(false);
        }
    });

    // 高度联动
    connect(heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), [=](int value) {
        if (origHeight > 0) {
            double percent = value * 100.0 / origHeight;
            heightPercentSpin->blockSignals(true);
            heightPercentSpin->setValue(percent);
            heightPercentSpin->blockSignals(false);
        }
    });

    connect(heightPercentSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double percent) {
        if (origHeight > 0) {
            int newHeight = static_cast<int>(origHeight * percent / 100.0);
            heightSpin->blockSignals(true);
            heightSpin->setValue(newHeight);
            heightSpin->blockSignals(false);
        }
    });

    // 布局
    auto *widthWidget = new QWidget(&dialog);
    auto *widthLayout = new QHBoxLayout(widthWidget);
    widthLayout->setContentsMargins(0, 0, 0, 0);
    widthLayout->addWidget(widthSpin);
    widthLayout->addWidget(widthPercentSpin);
    
    auto *heightWidget = new QWidget(&dialog);
    auto *heightLayout = new QHBoxLayout(heightWidget);
    heightLayout->setContentsMargins(0, 0, 0, 0);
    heightLayout->addWidget(heightSpin);
    heightLayout->addWidget(heightPercentSpin);

    formLayout->addRow(tr("显示列数 (宽度)"), widthWidget);
    formLayout->addRow(tr("显示行数 (高度)"), heightWidget);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    formLayout->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (useCustomCheck->isChecked()) {
        customDisplaySize_.setWidth(widthSpin->value());
        customDisplaySize_.setHeight(heightSpin->value());
        displayPercentage_ = widthPercentSpin->value(); 
    } else {
        customDisplaySize_ = QSize();
        displayPercentage_ = 100.0;
    }

    scaleFactor_ = 1.0;
    updateDisplayedPixmap();
}

void DisplayDock::onMouseMoved(const QPoint &pos)
{
    currentMousePos_ = pos;
    requestImageInfoUpdate();
}

void DisplayDock::onMouseLeft()
{
    currentMousePos_ = QPoint(-1, -1);
    
    // 更新显示为无效坐标
    if (mousePosLabel_) {
        mousePosLabel_->setText("(--, --)");
    }
    if (pixelValueLabel_) {
        pixelValueLabel_->setText("--");
    }
}

void DisplayDock::onImageInfoUpdated(const ImageInfo &info)
{
    if (!info.valid) {
        return;
    }
    
    // 更新第一个像素值
    if (firstPixelLabel_) {
        firstPixelLabel_->setText(QString::number(info.firstPixelValue));
    }
    
    // 更新鼠标坐标
    if (mousePosLabel_) {
        if (info.mouseX >= 0 && info.mouseY >= 0) {
            mousePosLabel_->setText(QString("(%1, %2)").arg(info.mouseX).arg(info.mouseY));
        } else {
            mousePosLabel_->setText("(--, --)");
        }
    }
    
    // 更新像素值
    if (pixelValueLabel_) {
        if (info.pixelValue >= 0) {
            pixelValueLabel_->setText(QString::number(info.pixelValue));
        } else {
            pixelValueLabel_->setText("--");
        }
    }
    
    // 更新帧率
    if (fpsLabel_) {
        fpsLabel_->setText(QString("%1 fps").arg(info.fps, 0, 'f', 1));
    }
    
    // 更新图像尺寸（列x行）
    if (imageSizeLabel_) {
        imageSizeLabel_->setText(QString("%1x%2").arg(info.imageWidth).arg(info.imageHeight));
    }
    
    // 更新色彩类型
    if (colorTypeLabel_) {
        colorTypeLabel_->setText(info.colorType);
    }
    
    // 更新色深
    if (bitDepthLabel_) {
        bitDepthLabel_->setText(QString("%1 bit").arg(info.bitDepth));
    }
}

void DisplayDock::requestImageInfoUpdate()
{
    if (!infoWorker_) {
        return;
    }
    
    QPoint targetPos;
    bool useImageCoords = false;

    if (isPinned_) {
        targetPos = pinnedImagePos_;
        useImageCoords = true;
    } else {
        targetPos = currentMousePos_;
        useImageCoords = false;
    }

    // 在工作线程中更新图像信息
    QMetaObject::invokeMethod(infoWorker_, "updateImageInfo",
                              Qt::QueuedConnection,
                              Q_ARG(QImage, currentImage_),
                              Q_ARG(QVector<uint16_t>, currentRawData_),
                              Q_ARG(int, currentRawWidth_),
                              Q_ARG(int, currentRawHeight_),
                              Q_ARG(int, currentBitDepth_),
                              Q_ARG(QPoint, targetPos),
                              Q_ARG(QSize, imageLabel_ ? imageLabel_->size() : QSize()),
                              Q_ARG(double, currentFps_),
                              Q_ARG(bool, useImageCoords));
}

QString DisplayDock::getColorTypeString(const QImage &image) const
{
    switch (image.format()) {
        case QImage::Format_Grayscale8:
        case QImage::Format_Grayscale16:
            return "Grayscale";
        case QImage::Format_RGB888:
            return "RGB";
        case QImage::Format_ARGB32:
        case QImage::Format_ARGB32_Premultiplied:
            return "ARGB";
        case QImage::Format_RGB32:
            return "RGB32";
        default:
            return QString("Format_%1").arg(static_cast<int>(image.format()));
    }
}

void DisplayDock::onWheelZoomRequested(int delta)
{
    if (delta == 0) {
        return;
    }

    if (currentImage_.isNull() && currentRawWidth_ == 0) {
        return;
    }

    const int steps = delta / 120;
    if (steps == 0) {
        return;
    }

    const double stepFactor = delta > 0 ? 1.1 : 1.0 / 1.1;
    double newScale = scaleFactor_;
    for (int i = 0; i < std::abs(steps); ++i) {
        newScale *= stepFactor;
    }

    newScale = std::clamp(newScale, 0.01, 10000.0);

    if (std::abs(newScale - scaleFactor_) > 1e-3) {
        scaleFactor_ = newScale;
        updateDisplayedPixmap();
        // 缩放后立即请求更新图像信息（鼠标坐标/像素值），避免需要移动鼠标才能刷新
        requestImageInfoUpdate();

        // 缩放后如果ROI处于激活状态，需要更新ROI对应的原始图像区域并触发分析
        if (isRoiActive_) {
            // 获取当前控件上的ROI区域（控件坐标）
            QRect widgetRoi = imageLabel_->getRoiRect();
            // 映射回原始图像坐标
            QPoint topLeft = mapToImage(widgetRoi.topLeft());
            QPoint bottomRight = mapToImage(widgetRoi.bottomRight());
            // 发送更新信号
            emit roiChanged(QRect(topLeft, bottomRight), true);
        }
    }
}

void DisplayDock::onPanRequested(const QPoint &delta)
{
    if (imageScrollArea_) {
        imageScrollArea_->horizontalScrollBar()->setValue(imageScrollArea_->horizontalScrollBar()->value() - delta.x());
        imageScrollArea_->verticalScrollBar()->setValue(imageScrollArea_->verticalScrollBar()->value() - delta.y());
    }
}

void DisplayDock::updateDisplayedPixmap()
{
    const QSize baseSize = activeBaseSize();
    QSize scaledSize(qMax(1, static_cast<int>(std::round(baseSize.width() * scaleFactor_))),
                     qMax(1, static_cast<int>(std::round(baseSize.height() * scaleFactor_))));

    if (imageScrollArea_) {
        imageScrollArea_->setMinimumSize(baseSize);
    }

    imageLabel_->setFixedSize(scaledSize);

    if (currentImage_.isNull()) {
        imageLabel_->setSourceImage(QImage());
        imageLabel_->setPixmap(QPixmap());
        imageLabel_->clear();
        imageLabel_->setText(tr("无采集图像"));
        imageLabel_->setAlignment(Qt::AlignCenter);
        return;
    }

#if DISPLAYDOCK_ENABLE_PARTIAL_PAINT
    // 新路径：不生成 scaledPixmap，交给 ImageDisplayLabel::paintEvent 按需绘制
    imageLabel_->setText(QString());
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setPixmap(QPixmap());
    imageLabel_->setSourceImage(currentImage_);
#else
    // 旧路径：生成整幅 scaledPixmap（用于复现/对比卡顿问题）
    imageLabel_->setSourceImage(QImage());
    imageLabel_->setText(QString());
    imageLabel_->setAlignment(Qt::AlignCenter);

    if (currentPixmap_.isNull()) {
        imageLabel_->setPixmap(QPixmap());
        imageLabel_->clear();
        imageLabel_->setText(tr("No Image"));
        imageLabel_->setAlignment(Qt::AlignCenter);
        return;
    }

    const QPixmap scaledPixmap = currentPixmap_.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    imageLabel_->setPixmap(scaledPixmap);
#endif

    // 如果有已固定的图像坐标点，计算其在 widget 上的位置并更新显示
    if (isPinned_ && pinnedImagePos_.x() >= 0 && pinnedImagePos_.y() >= 0) {
        // 直接传递图像坐标给 ImageDisplayLabel
        imageLabel_->setPinnedPoint(pinnedImagePos_, true);
    } else {
        imageLabel_->setPinnedPoint(QPoint(), false);
    }
}

void DisplayDock::onClickTimeout()
{
    if (!hasPendingClick_ || ignorePendingClick_) {
        hasPendingClick_ = false;
        ignorePendingClick_ = false;
        return;
    }

    QPoint pos = pendingClickPos_;
    hasPendingClick_ = false;

    // 坏点拾取模式：单击仅输出图像坐标，不干扰 Pin/ROI。
    if (badPixelPickMode_) {
        if (currentImage_.isNull() && currentRawWidth_ == 0) {
            return;
        }
        const QPoint imagePos = mapToImage(pos);
        if (imagePos.x() >= 0 && imagePos.y() >= 0) {
            emit badPixelPointPicked(imagePos);
        }
        return;
    }

    // 如果已固定且在附近点击则取消固定
    if (isPinned_) {
        QPoint pinnedWidgetPos = mapFromImage(pinnedImagePos_);
        if ((pos - pinnedWidgetPos).manhattanLength() < 10) {
            isPinned_ = false;
            pinnedImagePos_ = QPoint(-1, -1);
            imageLabel_->setPinnedPoint(QPoint(), false);
            requestImageInfoUpdate();
            emit pinnedPointChanged(QPoint(), false);
            return;
        }
    }

    // Pin new point (only if there is an image)
    if (currentImage_.isNull() && currentRawWidth_ == 0) {
        return;
    }

    isPinned_ = true;
    pinnedImagePos_ = mapToImage(pos);
    // 传递图像坐标
    imageLabel_->setPinnedPoint(pinnedImagePos_, true);
    requestImageInfoUpdate();
    emit pinnedPointChanged(pinnedImagePos_, true);
}

QSize DisplayDock::activeBaseSize() const
{
    return customDisplaySize_.isValid() ? customDisplaySize_ : defaultDisplaySize_;
}

void DisplayDock::setContinuousCaptureState(bool capturing)
{
    isContinuousCapturing_ = capturing;

    // While continuous capturing, forbid changing buffer count.
    // (Buffer count affects acquisition buffers and should not be changed mid-grab.)
    const bool canEditBufferCount = !capturing;
    if (bufferCountSlider_) {
        bufferCountSlider_->setEnabled(canEditBufferCount);
    }
    if (bufferCountLabel_) {
        bufferCountLabel_->setEnabled(canEditBufferCount);
    }

    updateButtonState();
}

void DisplayDock::updateButtonState()
{
    if (!captureToggleBtn_) return;
    
    if (isContinuousCapturing_) {
        captureToggleBtn_->setIcon(QIcon(":/icons/Stop_capture.svg"));
        captureToggleBtn_->setToolTip(tr("停止连续捕获"));
        // 连续采集时禁用单次采集按钮
        if (singleCaptureBtn_) {
            singleCaptureBtn_->setEnabled(false);
        }
    } else {
        captureToggleBtn_->setIcon(QIcon(":/icons/Continuous_Capture.svg"));
        captureToggleBtn_->setToolTip(tr("开始连续捕获"));
        // 非连续采集时启用单次采集按钮
        if (singleCaptureBtn_) {
            singleCaptureBtn_->setEnabled(true);
        }
    }
    captureToggleBtn_->setIconSize(QSize(DISPLAY_BUTTON_SIZE_X, DISPLAY_BUTTON_SIZE_Y));
}

void DisplayDock::showFpsSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("帧率设置"));
    dialog.resize(350, 250);
    
    auto *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);
    
    // 脱靶量刷新帧率设置
    auto *trackingGroup = new QGroupBox(tr("脱靶量刷新帧率"), &dialog);
    auto *trackingLayout = new QVBoxLayout(trackingGroup);
    
    auto *trackingFpsCombo = new QComboBox(&dialog);
    trackingFpsCombo->addItem(tr("不限制"), 0);
    trackingFpsCombo->addItem(tr("30 FPS"), 30);
    trackingFpsCombo->addItem(tr("60 FPS"), 60);
    trackingFpsCombo->addItem(tr("120 FPS"), 120);
    trackingFpsCombo->addItem(tr("240 FPS"), 240);
    trackingFpsCombo->addItem(tr("自定义"), -1);
    trackingFpsCombo->setCurrentIndex(3); // 默认120 FPS
    trackingLayout->addWidget(trackingFpsCombo);
    
    // 自定义帧率输入框（默认隐藏）
    auto *trackingCustomWidget = new QWidget(&dialog);
    auto *trackingCustomLayout = new QHBoxLayout(trackingCustomWidget);
    trackingCustomLayout->setContentsMargins(0, 0, 0, 0);
    auto *trackingCustomLabel = new QLabel(tr("自定义帧率:"), &dialog);
    auto *trackingCustomSpin = new QSpinBox(&dialog);
    trackingCustomSpin->setRange(1, 360);
    trackingCustomSpin->setValue(120);
    trackingCustomLayout->addWidget(trackingCustomLabel);
    trackingCustomLayout->addWidget(trackingCustomSpin);
    trackingLayout->addWidget(trackingCustomWidget);
    trackingCustomWidget->hide(); // 默认隐藏
    
    connect(trackingFpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [trackingFpsCombo, trackingCustomWidget](int index) {
        trackingCustomWidget->setVisible(index == trackingFpsCombo->count() - 1);
    });
    
    layout->addWidget(trackingGroup);
    
    // 图像显示刷新帧率设置
    auto *displayGroup = new QGroupBox(tr("图像显示刷新帧率"), &dialog);
    auto *displayLayout = new QVBoxLayout(displayGroup);
    
    auto *displayFpsCombo = new QComboBox(&dialog);
    displayFpsCombo->addItem(tr("不限制"), 0);
    displayFpsCombo->addItem(tr("30 FPS"), 30);
    displayFpsCombo->addItem(tr("60 FPS"), 60);
    displayFpsCombo->addItem(tr("120 FPS"), 120);
    displayFpsCombo->addItem(tr("240 FPS"), 240);
    displayFpsCombo->addItem(tr("自定义"), -1);
    displayFpsCombo->setCurrentIndex(0); // 默认不限制
    displayLayout->addWidget(displayFpsCombo);
    
    // 自定义帧率输入框（默认隐藏）
    auto *displayCustomWidget = new QWidget(&dialog);
    auto *displayCustomLayout = new QHBoxLayout(displayCustomWidget);
    displayCustomLayout->setContentsMargins(0, 0, 0, 0);
    auto *displayCustomLabel = new QLabel(tr("自定义帧率:"), &dialog);
    auto *displayCustomSpin = new QSpinBox(&dialog);
    displayCustomSpin->setRange(1, 360);
    displayCustomSpin->setValue(60);
    displayCustomLayout->addWidget(displayCustomLabel);
    displayCustomLayout->addWidget(displayCustomSpin);
    displayLayout->addWidget(displayCustomWidget);
    displayCustomWidget->hide(); // 默认隐藏
    
    connect(displayFpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [displayFpsCombo, displayCustomWidget](int index) {
        displayCustomWidget->setVisible(index == displayFpsCombo->count() - 1);
    });
    
    layout->addWidget(displayGroup);
    
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        int trackingFps = trackingFpsCombo->currentData().toInt();
        if (trackingFps == -1) {
            trackingFps = trackingCustomSpin->value();
        }
        emit trackingRefreshRateChanged(trackingFps);
        
        int displayFps = displayFpsCombo->currentData().toInt();
        if (displayFps == -1) {
            displayFps = displayCustomSpin->value();
        }
        emit displayRefreshRateChanged(displayFps);
    }
}

bool DisplayDock::eventFilter(QObject *watched, QEvent *event)
{
#if DISPLAYDOCK_ENABLE_BACKGROUND
    if (watched == imageScrollArea_->viewport() && event->type() == QEvent::Paint) {
        if (!backgroundPixmap_.isNull()) {
            QPainter painter(imageScrollArea_->viewport());
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.setOpacity(DISPLAY_BG_TRANSPARENCY);
            
            QRect viewportRect = imageScrollArea_->viewport()->rect();
            
            QTransform transform;
            transform.rotate(DISPLAY_BG_ROTATION_ANGLE);
            
            // Calculate the bounding box in the rotated coordinate system
            QTransform inverse = transform.inverted();
            QRectF mappedRect = inverse.mapRect(QRectF(viewportRect));
            QRect drawRect = mappedRect.toAlignedRect();
            // Expand to ensure coverage
            drawRect.adjust(-2, -2, 2, 2);

            painter.setTransform(transform);
            painter.drawTiledPixmap(drawRect, backgroundPixmap_, QPoint(0, 0));
        }
        // Return false to allow normal processing (drawing children etc.)
        return false;
    }
#endif
    return QDockWidget::eventFilter(watched, event);
}

void DisplayDock::onCaptureToggleClicked()
{
    if (isContinuousCapturing_) {
        emit stopCaptureRequested();
    } else {
        emit continuousCaptureRequested();
    }
}

QPoint DisplayDock::mapToImage(const QPoint &widgetPos) const
{
    if (currentImage_.isNull() && currentRawWidth_ == 0) return QPoint(-1, -1);
    
    int width = (currentRawWidth_ > 0) ? currentRawWidth_ : currentImage_.width();
    int height = (currentRawHeight_ > 0) ? currentRawHeight_ : currentImage_.height();
    QSize displaySize = imageLabel_->size();
    
    if (displaySize.isEmpty()) return QPoint(-1, -1);

    double scaleX = static_cast<double>(width) / static_cast<double>(displaySize.width());
    double scaleY = static_cast<double>(height) / static_cast<double>(displaySize.height());

    int x = static_cast<int>(std::floor(widgetPos.x() * scaleX));
    int y = static_cast<int>(std::floor(widgetPos.y() * scaleY));
    
    return QPoint(x, y);
}

QPoint DisplayDock::mapFromImage(const QPoint &imagePos) const
{
    if (currentImage_.isNull() && currentRawWidth_ == 0) return QPoint(-1, -1);
    
    int width = (currentRawWidth_ > 0) ? currentRawWidth_ : currentImage_.width();
    int height = (currentRawHeight_ > 0) ? currentRawHeight_ : currentImage_.height();
    QSize displaySize = imageLabel_->size();
    
    if (width == 0 || height == 0) return QPoint(-1, -1);

    double scaleX = static_cast<double>(displaySize.width()) / static_cast<double>(width);
    double scaleY = static_cast<double>(displaySize.height()) / static_cast<double>(height);

    // Use center of the pixel for display to avoid drift during zoom
    int x = static_cast<int>(std::floor((imagePos.x() + 0.5) * scaleX));
    int y = static_cast<int>(std::floor((imagePos.y() + 0.5) * scaleY));
    
    return QPoint(x, y);
}

void DisplayDock::onMouseClicked(const QPoint &pos)
{
    // 使用定时器将单击与双击区分：延迟处理单击，若在延迟内发生双击则取消
    // 如果当前没有图像则不允许创建 pin
    if (currentImage_.isNull() && currentRawWidth_ == 0) {
        return;
    }

    // 如果已经有 pending click（例如双击过程中的第二个点击），则忽略，避免重复处理
    if (hasPendingClick_) {
        return;
    }

    pendingClickPos_ = pos;
    hasPendingClick_ = true;
    if (clickTimer_) {
        clickTimer_->start();
    }
}

void DisplayDock::onMouseDoubleClicked(const QPoint &pos)
{
    // 双击优先：取消单击定时器，避免同时出现红点
    if (clickTimer_) {
        clickTimer_->stop();
    }
    hasPendingClick_ = false;
    ignorePendingClick_ = true;

    // 坏点拾取模式下禁用双击 ROI 切换
    if (badPixelPickMode_) {
        return;
    }

    if (isRoiActive_) {
        isRoiActive_ = false;
        currentRoi_ = QRect();
        imageLabel_->setRoiRect(QRect(), false);
        emit roiChanged(QRect(), false);
    } else {
        isRoiActive_ = true;
        QRect rect(pos.x() - 50, pos.y() - 50, 100, 100);
        rect = rect.intersected(imageLabel_->rect());
        currentRoi_ = rect;
        imageLabel_->setRoiRect(rect, true);
        
        QPoint topLeft = mapToImage(rect.topLeft());
        QPoint bottomRight = mapToImage(rect.bottomRight());
        emit roiChanged(QRect(topLeft, bottomRight), true);
    }
}

void DisplayDock::onRoiChanged(const QRect &rect)
{
    currentRoi_ = rect;
    QPoint topLeft = mapToImage(rect.topLeft());
    QPoint bottomRight = mapToImage(rect.bottomRight());
    emit roiChanged(QRect(topLeft, bottomRight), true);
}

