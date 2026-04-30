#include "image_algorithm_dock.h"
#include "thread_manager.h"
#include "noise_3d_surface_widget.h"
#include "image_algorithm_manager.h"
#include "image_processing_panel.h"
#include "image_depth_converter.h"
#include "opencv_qt_bridge.h"
#include "mixed_processing_panel.h"
#include "spot_detection_worker.h"
#include "detail_histogram_widget.h"
#include "noise_result_model.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QFrame>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QPushButton>
#include <QTabBar>
#include <QSpacerItem>
#include <QSplitter>
#include <QSettings>
#include <QTimer>
#include <QMessageBox>
#include <QTableView>
#include <QAbstractTableModel>
#include <QHeaderView>
#include <QDialog>
#include <QDialogButtonBox>
#include <climits>
#include <cmath>
#include <QFileDialog>
#include <QDateTime>
#include <xlsxdocument.h>
#include <QFile>
#include <QDir>
#include <QScrollArea>
#include <QPixmap>
#include <QImage>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QDoubleSpinBox>
#include <algorithm>
#include <memory>
#include <cstring>
#include "QFormLayout"

ImageAlgorithmWorker::ImageAlgorithmWorker(QObject *parent)
    : QObject(parent)
{
}

void ImageAlgorithmWorker::stop()
{
    emit finished();
}

ImageAlgorithmDock::ImageAlgorithmDock(ThreadManager *threadManager, QWidget *parent)
    : QDockWidget(tr("图像算法"), parent)
    , contentWidget_(new QWidget(this))
    , threadManager_(threadManager)
{
    setObjectName(QStringLiteral("imageAlgorithmDock"));
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    // 允许内容区域自由收缩，取消任何最小高度限制
    if (contentWidget_) {
        contentWidget_->setMinimumSize(0, 0);
        contentWidget_->setMinimumHeight(0);
    }


    setupUI();
    setupThread();
}

ImageAlgorithmDock::~ImageAlgorithmDock()
{
    saveUiState();

    // 取消正在进行的分析
    if (isAnalyzing_) {
        cancelAnalysis();
    }

    // 停止噪声分析工作对象（线程由ThreadManager管理）
    if (noiseAnalysisWorker_) {
        noiseAnalysisWorker_->stop();
        noiseAnalysisWorker_->cleanup();
    }

    if (worker_) {
        emit stopWorkerRequested();
    }

    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(500);
    }
}

void ImageAlgorithmDock::setBitDepth(int bitDepth)
{
    bitDepth_ = bitDepth;
    // Propagate a hint immediately so parameter mapping works even before first frame.
    if (imageAlgorithmManager_ && bitDepth_ > 0) {
        imageAlgorithmManager_->setCurrentBitDepth(bitDepth_);
    }
}

void ImageAlgorithmDock::setupUI()
{
    // 主水平布局：左侧导航，右侧内容区域
    mainLayout_ = new QHBoxLayout;
    mainLayout_->setContentsMargins(8, 8, 8, 8);
    mainLayout_->setSpacing(0);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, contentWidget_);
    splitter->setMinimumWidth(0);
    mainSplitter_ = splitter;

    // 导航列表
    navList_ = new QListWidget(splitter);
    navList_->setMinimumWidth(0);
    navList_->setMaximumWidth(16777215);  // 允许任意宽度
    navList_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);  // Ignored 允许完全缩小
    navList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    navList_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    navList_->addItem(tr("读出噪声"));
    // 新增：图像处理算法导航
    navList_->addItem(tr("图像处理"));
    // 新增：混合处理导航
    navList_->addItem(tr("混合处理"));
#if ENABLE_SPOT_DETECTION
    // 新增：光斑检测导航
    navList_->addItem(tr("光斑检测"));
#endif
    navList_->setCurrentRow(0);

    // 右侧堆栈页面
    stackedPages_ = new QStackedWidget(splitter);
    stackedPages_->setMinimumWidth(0);
    stackedPages_->setMaximumWidth(16777215);  // 允许任意宽度
    stackedPages_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);  // Ignored 允许完全缩小

    // 1. 读出噪声页面
    stackedPages_->addWidget(createReadoutNoisePage());

    // 图像处理算法页面
    stackedPages_->addWidget(createImageProcessingPage());

    // 混合处理页面
    stackedPages_->addWidget(createMixedProcessingPage());

#if ENABLE_SPOT_DETECTION
    // 光斑检测页面
    stackedPages_->addWidget(createSpotDetectionPage());
#endif

    connect(navList_, &QListWidget::currentRowChanged, stackedPages_, &QStackedWidget::setCurrentIndex);

    splitter->addWidget(navList_);
    splitter->addWidget(stackedPages_);
    splitter->setStretchFactor(0, 0);  // 左栏不自动拉伸
    splitter->setStretchFactor(1, 1);  // 右栏自动拉伸填充空间
    splitter->setCollapsible(0, false);  // 禁止左栏折叠，允许无极缩小
    splitter->setCollapsible(1, false);  // 禁止右栏折叠，允许无极缩小
    splitter->setOpaqueResize(true);    // 拖动时实时刷新显示

    restoreUiState();
    if (splitter->sizes().isEmpty()) {
        splitter->setSizes(QList<int>() << 160 << 600);
    }

    mainLayout_->addWidget(splitter);

    contentWidget_->setLayout(mainLayout_);
    setWidget(contentWidget_);
}


void ImageAlgorithmDock::setupThread()
{
    workerThread_ = new QThread(this);
    worker_ = new ImageAlgorithmWorker();
    worker_->moveToThread(workerThread_);

    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(this, &ImageAlgorithmDock::stopWorkerRequested, worker_, &ImageAlgorithmWorker::stop);
    connect(worker_, &ImageAlgorithmWorker::finished, workerThread_, &QThread::quit);

    workerThread_->start();

    // 使用ThreadManager管理的噪声分析线程
    if (threadManager_) {
        noiseAnalysisThread_ = threadManager_->noiseAnalysisThread();
        noiseAnalysisWorker_ = threadManager_->noiseAnalysisWorker();

        // 连接噪声分析工作线程的信号
        connect(noiseAnalysisWorker_, &NoiseAnalysisWorker::gainAnalysisComplete,
                this, &ImageAlgorithmDock::onGainAnalysisComplete, Qt::QueuedConnection);
        connect(noiseAnalysisWorker_, &NoiseAnalysisWorker::allAnalysisComplete,
                this, &ImageAlgorithmDock::onAllAnalysisComplete, Qt::QueuedConnection);
        connect(noiseAnalysisWorker_, &NoiseAnalysisWorker::progressUpdate,
                this, &ImageAlgorithmDock::onAnalysisProgressUpdate, Qt::QueuedConnection);
        connect(noiseAnalysisWorker_, &NoiseAnalysisWorker::analysisError,
                this, [this](const QString &error) {
                    qWarning() << "NoiseAnalysisWorker error:" << error;
                    QMessageBox::warning(this, tr("分析错误"), error);
                    cancelAnalysis();
                }, Qt::QueuedConnection);
    } else {
        qWarning() << "ImageAlgorithmDock: ThreadManager is null, noise analysis functionality disabled";
    }
}


void ImageAlgorithmDock::onLocalAnalysisClicked()
{
    if (isAnalyzing_) {
        QMessageBox::warning(this, tr("分析中"), tr("已有分析任务在进行中，请等待完成。"));
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("选择模式"));
    msgBox.setText(tr("请选择要打开的内容："));
    QPushButton *btnFolder = msgBox.addButton(tr("文件夹"), QMessageBox::AcceptRole);
    QPushButton *btnFile = msgBox.addButton(tr("文件"), QMessageBox::RejectRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();

    QStringList filePaths;
    if (msgBox.clickedButton() == btnFolder) {
        QString dir = QFileDialog::getExistingDirectory(this, tr("选择文件夹"));
        if (dir.isEmpty()) return;
        QDir directory(dir);
        QStringList filters;
        filters << "*.png" << "*.jpg" << "*.bmp" << "*.tif" << "*.tiff";
        QStringList entries = directory.entryList(filters, QDir::Files);
        for (const QString &entry : entries) filePaths.append(directory.filePath(entry));
    } else if (msgBox.clickedButton() == btnFile) {
        filePaths = QFileDialog::getOpenFileNames(this, tr("选择图像文件"), "", tr("Images (*.png *.jpg *.bmp *.tif *.tiff)"));
    } else {
        return;
    }

    if (filePaths.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("未选择任何文件"));
        return;
    }

    // 设置状态
    isAnalyzing_ = true;
    btnLocalAnalysis_->setEnabled(false);
    btnLocalAnalysis_->setText(tr("分析中..."));

    // 准备配置
    gainConfigs_.clear();
    GainConfig config;
    config.gainType = 0; // 占位
    config.name = tr("本地文件");
    config.exposureTime = 0;
    gainConfigs_.append(config);

    gainResults_.clear();
    gainResults_.resize(1);

    currentGainIndex_ = 0;
    currentSampleIndex_ = 0;
    totalSamples_ = filePaths.size(); // 用于显示进度

    // 获取分析区域参数
    int startX = spinStartX_->value();
    int startY = spinStartY_->value();
    int width = spinWidth_->value();
    int height = spinHeight_->value();

    if (width <= 0 || height <= 0) {
        // 如果未设置有效区域，则使用空矩形（表示全图）
        analysisRegion_ = QRect();
    } else {
        analysisRegion_ = QRect(startX, startY, width, height);
    }

    // 调用 worker 的 startFileAnalysis
    QMetaObject::invokeMethod(noiseAnalysisWorker_, "startFileAnalysis", Qt::QueuedConnection,
                              Q_ARG(QStringList, filePaths),
                              Q_ARG(QRect, analysisRegion_));
}


// 单个增益配置分析完成
void ImageAlgorithmDock::onGainAnalysisComplete(int gainIndex, const NoiseAnalysisResult &result)
{
    if (gainIndex >= 0 && gainIndex < gainResults_.size()) {
        // 保存结果并添加增益配置信息
        gainResults_[gainIndex] = result;
        gainResults_[gainIndex].gainName = gainConfigs_[gainIndex].name;
        gainResults_[gainIndex].exposureTime = gainConfigs_[gainIndex].exposureTime;

        // 如果是本地分析，更新分析区域大小
        if (analysisRegion_.isEmpty() && result.width > 0 && result.height > 0) {
            analysisRegion_ = QRect(0, 0, result.width, result.height);
        }

    }
}

// 所有分析完成
void ImageAlgorithmDock::onAllAnalysisComplete()
{
    calculateAndDisplayResults();
}

// 分析进度更新
void ImageAlgorithmDock::onAnalysisProgressUpdate(int gainIndex, int currentFrame, int totalFrames)
{
    // 可以在这里更新进度条或状态文本
    QString progressText = tr("分析中... (%1/%2) 帧 %3/%4")
                               .arg(gainIndex + 1)
                               .arg(gainConfigs_.size())
                               .arg(currentFrame)
                               .arg(totalFrames);

    btnLocalAnalysis_->setText(progressText);
}


// 取消分析
void ImageAlgorithmDock::cancelAnalysis()
{
    isAnalyzing_ = false;
    btnLocalAnalysis_->setEnabled(true);
    btnLocalAnalysis_->setText(tr("本地分析"));

    // 停止并清理工作线程
    if (noiseAnalysisWorker_) {
        QMetaObject::invokeMethod(noiseAnalysisWorker_, "stop", Qt::QueuedConnection);
        QMetaObject::invokeMethod(noiseAnalysisWorker_, "cleanup", Qt::QueuedConnection);
    }

    gainConfigs_.clear();
    gainResults_.clear();
    gainResults_.squeeze();  // 释放多余内存
    currentGainIndex_ = -1;
    currentSampleIndex_ = 0;

}

// 计算并显示结果
void ImageAlgorithmDock::calculateAndDisplayResults()
{
    if (gainResults_.isEmpty()) {
        QMessageBox::warning(this, tr("分析失败"), tr("没有采集到有效数据。"));
        cancelAnalysis();
        return;
    }

    int width = analysisRegion_.width();
    int height = analysisRegion_.height();

    // 创建结果对话框 - 增大尺寸以容纳3D图表
    QDialog *resultDialog = new QDialog(this);
    resultDialog->setWindowTitle(tr("读出噪声分析结果 - 3D可视化"));
    resultDialog->resize(1800, 1000);  // 增大尺寸

    QVBoxLayout *dialogLayout = new QVBoxLayout(resultDialog);

    QLabel *infoLabel = new QLabel(tr("区域: (%1,%2) 大小: %3x%4  采样数: %5  增益配置数: %6")
                                       .arg(analysisRegion_.x())
                                       .arg(analysisRegion_.y())
                                       .arg(width)
                                       .arg(height)
                                       .arg(totalSamples_)
                                       .arg(gainResults_.size()), resultDialog);
    dialogLayout->addWidget(infoLabel);

    // 为每个增益配置创建一个表格
    for (int gainIdx = 0; gainIdx < gainResults_.size(); ++gainIdx) {
        const NoiseAnalysisResult &result = gainResults_[gainIdx];

        if (!result.valid) {
            continue;
        }

        // 计算平均STD
        double avgStd = 0;
        for (double s : result.stdDevs) avgStd += s;
        avgStd /= result.stdDevs.size();

        QLabel *gainLabel = new QLabel(tr("%1 (曝光时间: %2 μs) STD: %3")
                                           .arg(result.gainName)
                                           .arg(result.exposureTime)
                                           .arg(avgStd, 0, 'f', 6), resultDialog);
        QFont font = gainLabel->font();
        font.setBold(true);
        gainLabel->setFont(font);
        dialogLayout->addWidget(gainLabel);

        // 创建水平布局，三列：左侧表格，中间采样图像，右侧3D可视化
        QHBoxLayout *resultLayout = new QHBoxLayout();

        // 左侧表格区域布局
        QVBoxLayout *tableLayout = new QVBoxLayout();
        QLabel *tableTitle = new QLabel(tr("数据统计:"), resultDialog);
        tableLayout->addWidget(tableTitle);

        // 创建表格显示结果 - 使用 QTableView 和自定义 Model 优化性能
        QTableView *table = new QTableView(resultDialog);
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        table->setMaximumHeight(400);  // 增加表格高度
        table->setMinimumWidth(300);

        // 创建并设置数据模型
        // 注意：Model 会复制一份数据，所以即使 gainResults_ 被清理，Model 中的数据仍然有效
        NoiseResultModel *model = new NoiseResultModel(
            result.stdDevs,
            width,
            height,
            analysisRegion_.x(),
            analysisRegion_.y(),
            table
            );
        table->setModel(model);

        // 优化列宽
        table->horizontalHeader()->setDefaultSectionSize(60);

        tableLayout->addWidget(table);
        resultLayout->addLayout(tableLayout, 2);  // 左侧表格占2份

        // 中间显示采样图像
        if (!result.sampleImages.isEmpty()) {
            QVBoxLayout *imageLayout = new QVBoxLayout();
            QLabel *imageTitle = new QLabel(tr("采样图像 (共 %1 张):").arg(result.sampleImages.size()), resultDialog);
            imageLayout->addWidget(imageTitle);

            // 创建一个滚动区域显示所有采样图像
            QScrollArea *scrollArea = new QScrollArea(resultDialog);
            scrollArea->setWidgetResizable(true);
            scrollArea->setMaximumHeight(400);
            scrollArea->setMinimumWidth(300);
            scrollArea->setMaximumWidth(350);

            QWidget *scrollWidget = new QWidget();
            QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

            // 显示所有采样图像
            for (int imgIdx = 0; imgIdx < result.sampleImages.size(); ++imgIdx) {
                const cv::Mat &img = result.sampleImages[imgIdx];

                // 转换为QImage显示
                cv::Mat displayMat;
                // 归一化到0-255以便显示
                cv::normalize(img, displayMat, 0, 255, cv::NORM_MINMAX, CV_8U);

                QImage qImg(displayMat.data, displayMat.cols, displayMat.rows, displayMat.step, QImage::Format_Grayscale8);
                QPixmap pixmap = QPixmap::fromImage(qImg.copy());

                // 缩放图像以便显示
                pixmap = pixmap.scaled(280, 280, Qt::KeepAspectRatio, Qt::SmoothTransformation);

                QLabel *imgLabel = new QLabel(scrollWidget);
                imgLabel->setPixmap(pixmap);
                // imgLabel->setFrameStyle(QFrame::Box); // 移除边框

                // 设置属性以便在双击时识别图像
                imgLabel->setProperty("gainIndex", gainIdx);
                imgLabel->setProperty("imgIndex", imgIdx);
                imgLabel->installEventFilter(this);
                imgLabel->setCursor(Qt::PointingHandCursor); // 设置鼠标手势提示可点击

                QLabel *imgInfo = new QLabel(tr("样本 %1 (%2x%3)")
                                                 .arg(imgIdx + 1)
                                                 .arg(img.cols)
                                                 .arg(img.rows), scrollWidget);

                scrollLayout->addWidget(imgInfo);
                scrollLayout->addWidget(imgLabel);
            }

            scrollLayout->addStretch();
            scrollWidget->setLayout(scrollLayout);
            scrollArea->setWidget(scrollWidget);

            imageLayout->addWidget(scrollArea);
            resultLayout->addLayout(imageLayout, 2);  // 中间图像区域占2份
        }

        // 右侧添加3D可视化图表
        QVBoxLayout *chart3DLayout = new QVBoxLayout();
        QLabel *chart3DTitle = new QLabel(tr("噪声分布:"), resultDialog);
        QFont titleFont = chart3DTitle->font();
        titleFont.setBold(true);
        chart3DTitle->setFont(titleFont);
        chart3DLayout->addWidget(chart3DTitle);

        // 创建3D表面图小部件
        Noise3DSurfaceWidget *surface3D = new Noise3DSurfaceWidget(resultDialog);
        // 使用采样图像栈进行3D点云可视化：X=帧数(帧序号)，Y/Z=ROI坐标，颜色=像素值
        if (!result.sampleImages.isEmpty()) {
            QVector<QVector<uint16_t>> frames;
            frames.reserve(result.sampleImages.size());
            for (const cv::Mat &img : result.sampleImages) {
                cv::Mat img16;
                if (img.type() == CV_16UC1) {
                    img16 = img;
                } else {
                    img.convertTo(img16, CV_16U);
                }

                QVector<uint16_t> frame;
                frame.resize(img16.cols * img16.rows);
                if (img16.isContinuous()) {
                    std::copy(img16.ptr<uint16_t>(0), img16.ptr<uint16_t>(0) + img16.cols * img16.rows, frame.begin());
                } else {
                    for (int r = 0; r < img16.rows; ++r) {
                        const uint16_t *row = img16.ptr<uint16_t>(r);
                        std::copy(row, row + img16.cols, frame.begin() + r * img16.cols);
                    }
                }
                frames.push_back(std::move(frame));
            }
            surface3D->setStackData(frames, width, height, analysisRegion_.x(), analysisRegion_.y());
        } else {
            // 兼容：若无采样图，则回退到标准差表面
            surface3D->setData(result.stdDevs, width, height);
        }
        // 移除固定的最小尺寸，让3D视图与右侧保持同样大小

        // 添加双击打开新窗口的功能
        // 将数据存储在堆上以避免在lambda中复制大量数据
        auto sharedStdDevs = std::make_shared<QVector<double>>(result.stdDevs);
        auto sharedFrames = std::make_shared<QVector<QVector<uint16_t>>>();
        int roiStartX = analysisRegion_.x();
        int roiStartY = analysisRegion_.y();
        if (!result.sampleImages.isEmpty()) {
            sharedFrames->reserve(result.sampleImages.size());
            for (const cv::Mat &img : result.sampleImages) {
                cv::Mat img16;
                if (img.type() == CV_16UC1) {
                    img16 = img;
                } else {
                    img.convertTo(img16, CV_16U);
                }
                QVector<uint16_t> frame;
                frame.resize(img16.cols * img16.rows);
                if (img16.isContinuous()) {
                    std::copy(img16.ptr<uint16_t>(0), img16.ptr<uint16_t>(0) + img16.cols * img16.rows, frame.begin());
                } else {
                    for (int r = 0; r < img16.rows; ++r) {
                        const uint16_t *row = img16.ptr<uint16_t>(r);
                        std::copy(row, row + img16.cols, frame.begin() + r * img16.cols);
                    }
                }
                sharedFrames->push_back(std::move(frame));
            }
        }
        auto openNoiseDialog = [resultDialog, sharedStdDevs, sharedFrames, width, height, roiStartX, roiStartY](Noise3DSurfaceWidget::ViewMode mode, const QString &title) {
            QDialog *dialog = new QDialog(resultDialog);
            dialog->setWindowTitle(title);
            dialog->resize(1000, 800);

            QVBoxLayout *layout = new QVBoxLayout(dialog);
            layout->setContentsMargins(5, 5, 5, 5);

            Noise3DSurfaceWidget *view = new Noise3DSurfaceWidget(dialog);
            if (sharedFrames && !sharedFrames->isEmpty()) {
                view->setStackData(*sharedFrames, width, height, roiStartX, roiStartY);
            } else {
                view->setData(*sharedStdDevs, width, height);
            }
            view->setViewMode(mode);
            layout->addWidget(view);

            QHBoxLayout *buttonLayout = new QHBoxLayout();
            buttonLayout->addStretch();
            QPushButton *closeBtn = new QPushButton(QObject::tr("关闭"), dialog);
            connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
            buttonLayout->addWidget(closeBtn);
            layout->addLayout(buttonLayout);

            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
        };

        connect(surface3D, &Noise3DSurfaceWidget::surfaceViewDoubleClicked,
                resultDialog, [openNoiseDialog]() {
                    openNoiseDialog(Noise3DSurfaceWidget::SurfaceOnly, QObject::tr("噪声分布 - 3D"));
                });

        connect(surface3D, &Noise3DSurfaceWidget::pixelViewDoubleClicked,
                resultDialog, [openNoiseDialog]() {
                    openNoiseDialog(Noise3DSurfaceWidget::PixelOnly, QObject::tr("噪声分布 - 曲线"));
                });

        chart3DLayout->addWidget(surface3D);
        resultLayout->addLayout(chart3DLayout, 3);  // 右侧3D图表占3份（最大）

        dialogLayout->addLayout(resultLayout);
    }

    dialogLayout->addStretch();

    QDialogButtonBox *buttonBox = new QDialogButtonBox(resultDialog);
    QPushButton *saveButton = buttonBox->addButton(tr("保存"), QDialogButtonBox::ActionRole);
    QPushButton *saveAsButton = buttonBox->addButton(tr("另存为"), QDialogButtonBox::ActionRole);
    connect(saveButton, &QPushButton::clicked, this, &ImageAlgorithmDock::onSaveResults);
    connect(saveAsButton, &QPushButton::clicked, this, &ImageAlgorithmDock::onSaveNew);
    dialogLayout->addWidget(buttonBox);

    resultDialog->setLayout(dialogLayout);

    // 清理状态（在显示对话框之前，避免对话框阻塞导致状态未清理）
    // 注意：cancelAnalysis 会清空 gainResults_，所以我们需要先复制一份数据或者修改 cancelAnalysis 逻辑
    // 这里我们手动重置 UI 状态，保留数据给对话框使用，对话框关闭后再完全清理
    isAnalyzing_ = false;
    isCapturing_ = false;
    btnLocalAnalysis_->setEnabled(true);
    btnLocalAnalysis_->setText(tr("本地分析"));

    // 非模态显示对话框，避免阻塞主线程
    resultDialog->setAttribute(Qt::WA_DeleteOnClose);
    resultDialog->show();

    // 连接对话框关闭信号进行最终清理
    connect(resultDialog, &QDialog::finished, this, [this]() {
        // 停止并清理工作线程
        if (noiseAnalysisWorker_) {
            QMetaObject::invokeMethod(noiseAnalysisWorker_, "stop", Qt::QueuedConnection);
            QMetaObject::invokeMethod(noiseAnalysisWorker_, "cleanup", Qt::QueuedConnection);
        }

        gainConfigs_.clear();
        gainResults_.clear();
        gainResults_.squeeze();
        currentGainIndex_ = -1;
        currentSampleIndex_ = 0;
    });
}

bool ImageAlgorithmDock::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        QLabel *label = qobject_cast<QLabel*>(watched);
        if (label) {
            QVariant gainIdxVar = label->property("gainIndex");
            QVariant imgIdxVar = label->property("imgIndex");

            if (gainIdxVar.isValid() && imgIdxVar.isValid()) {
                int gainIdx = gainIdxVar.toInt();
                int imgIdx = imgIdxVar.toInt();

                if (gainIdx >= 0 && gainIdx < gainResults_.size()) {
                    const NoiseAnalysisResult &result = gainResults_[gainIdx];
                    if (imgIdx >= 0 && imgIdx < result.sampleImages.size()) {
                        const cv::Mat &rawImg = result.sampleImages[imgIdx];

                        // 使用 OpenCV 计算统计信息
                        double minVal, maxVal;
                        cv::Point minLoc, maxLoc;
                        cv::minMaxLoc(rawImg, &minVal, &maxVal, &minLoc, &maxLoc);

                        cv::Scalar mean, stddev;
                        cv::meanStdDev(rawImg, mean, stddev);

                        // 计算色深和极差
                        int storageBitDepth = rawImg.elemSize1() * 8;
                        int currentImageBitDepth = 0;
                        if (maxVal > 0) {
                            currentImageBitDepth = std::ceil(std::log2(maxVal + 1));
                        }
                        double range = maxVal - minVal;

                        // 创建详细信息对话框
                        QDialog *detailDialog = new QDialog(this);
                        detailDialog->setWindowTitle(tr("图像详情 - %1 - 样本 %2").arg(result.gainName).arg(imgIdx + 1));
                        detailDialog->resize(1000, 700);

                        QVBoxLayout *mainLayout = new QVBoxLayout(detailDialog);

                        // 显示统计信息
                        QString infoText = tr(
                                               "原始图像信息:\n"
                                               "尺寸: %1 x %2\n"
                                               "类型: %3 (CV_16U)\n"
                                               "相机位深: %13 bit\n"
                                               "统计信息 (OpenCV):\n"
                                               "最小值: %4 (at %5, %6)\n"
                                               "最大值: %7 (at %8, %9)\n"
                                               "极差: %12\n"
                                               "均值: %10\n"
                                               "标准差: %11"
                                               ).arg(rawImg.cols).arg(rawImg.rows)
                                               .arg(rawImg.type() == CV_16U ? "16-bit Unsigned" : QString::number(rawImg.type()))
                                               .arg(minVal).arg(minLoc.x).arg(minLoc.y)
                                               .arg(maxVal).arg(maxLoc.x).arg(maxLoc.y)
                                               .arg(mean[0], 0, 'f', 4)
                                               .arg(stddev[0], 0, 'f', 4)
                                               .arg(range)
                                               .arg(bitDepth_);

                        QLabel *infoLabel = new QLabel(infoText, detailDialog);
                        infoLabel->setStyleSheet("font-weight: bold; color: #ffffff; padding: 10px; background-color: #2d2d2d; border: 1px solid #555;");
                        mainLayout->addWidget(infoLabel);

                        // 内容区域：左侧图像，右侧直方图
                        QHBoxLayout *contentLayout = new QHBoxLayout();

                        // 左侧：显示大图
                        QScrollArea *scrollArea = new QScrollArea(detailDialog);
                        scrollArea->setWidgetResizable(true);

                        QLabel *imageLabel = new QLabel(scrollArea);
                        imageLabel->setAlignment(Qt::AlignCenter);

                        // 转换图像用于显示
                        cv::Mat displayMat;
                        cv::normalize(rawImg, displayMat, 0, 255, cv::NORM_MINMAX, CV_8U);
                        QImage qImg(displayMat.data, displayMat.cols, displayMat.rows, displayMat.step, QImage::Format_Grayscale8);

                        QPixmap pixmap = QPixmap::fromImage(qImg.copy());

                        // 如果图像较小，自动放大以便观察 (使用最近邻插值保持像素清晰度)
                        if (pixmap.width() < 600 && pixmap.height() < 600) {
                            pixmap = pixmap.scaled(600, 600, Qt::KeepAspectRatio, Qt::FastTransformation);
                        }

                        imageLabel->setPixmap(pixmap);

                        scrollArea->setWidget(imageLabel);
                        contentLayout->addWidget(scrollArea, 2); // 图像占2份

                        // 右侧：直方图
                        QVBoxLayout *histLayout = new QVBoxLayout();

                        // 计算直方图
                        int histSize = 65536;
                        float range_vals[] = { 0, 65536 };
                        const float* histRange = { range_vals };
                        cv::Mat hist;
                        cv::calcHist(&rawImg, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange, true, false);

                        // 使用交互式直方图控件
                        DetailHistogramWidget *histWidget = new DetailHistogramWidget(detailDialog);
                        histWidget->setData(hist, minVal, maxVal);

                        histLayout->addWidget(histWidget);

                        contentLayout->addLayout(histLayout, 1); // 直方图占1份
                        mainLayout->addLayout(contentLayout);

                        detailDialog->setAttribute(Qt::WA_DeleteOnClose);
                        detailDialog->show();

                        return true; // 事件已处理
                    }
                }
            }
        }
    }

    return QDockWidget::eventFilter(watched, event);
}

void ImageAlgorithmDock::onRawImageReceived(const QVector<uint16_t> &rawData, int width, int height, int bitDepth)
{
    // 自动识别并兼容：
    // - 识别有效位深（例如上游报16但实际只有14）
    // - 识别对齐方式：若是 MSB 左移对齐，则右移还原为原生值
    // 统一后：所有后续 raw16 算法都按 native 值 + effectiveBitDepth 工作，缩放范围自动正确。
    static bool s_hasFormat = false;
    static int s_lastW = 0;
    static int s_lastH = 0;
    static int s_lastBitDepthHint = 0;
    static OpenCvQtBridge::Raw16AlignmentStats s_format;

    const bool needAnalyze = (!s_hasFormat || s_lastW != width || s_lastH != height || s_lastBitDepthHint != bitDepth);
    if (needAnalyze) {
        s_hasFormat = false;
        s_format = OpenCvQtBridge::Raw16AlignmentStats();

        if (!rawData.isEmpty() && width > 0 && height > 0 && bitDepth > 8) {
            const cv::Mat mat16 = OpenCvQtBridge::raw16VectorToMatView(rawData, width, height);
            s_format = OpenCvQtBridge::analyzeRaw16Alignment16U(mat16, bitDepth);
            s_hasFormat = true;
            s_lastW = width;
            s_lastH = height;
            s_lastBitDepthHint = bitDepth;

#if OPENCVQTBRIDGE_ENABLE_RAW16_ALIGNMENT_QDEBUG
            OpenCvQtBridge::qDebugRaw16AlignmentStats(s_format, "Raw16Align@ImageAlgorithmDock");
#endif
        }
    }

    int effectiveBitDepth = bitDepth;
    if (s_hasFormat && s_format.effectiveBitDepth > 0) {
        effectiveBitDepth = s_format.effectiveBitDepth;
    }

    // Keep the algorithm manager informed so parameter mapping (UI 0..255 -> native)
    // is correct immediately.
    if (imageAlgorithmManager_ && effectiveBitDepth > 0) {
        imageAlgorithmManager_->setCurrentBitDepth(effectiveBitDepth);
    }

    const QVector<uint16_t>* dataNativePtr = &rawData;
    QVector<uint16_t> normalized;
    if (s_hasFormat && s_format.alignment == OpenCvQtBridge::Raw16Alignment::MsbAlignedShifted && s_format.assumedShift > 0) {
        normalized = rawData;
        const int shift = s_format.assumedShift;
        for (int i = 0; i < normalized.size(); ++i) {
            normalized[i] = static_cast<uint16_t>(normalized[i] >> shift);
        }
        dataNativePtr = &normalized;
    }
    const QVector<uint16_t>& dataNative = *dataNativePtr;

    // 转发原始数据到图像处理算法管理器
    if (imageAlgorithmManager_) {
        if (processingInputMode_ == ProcessingInputMode::Raw16Bit) {
            imageAlgorithmManager_->processRawImage(dataNative, width, height, effectiveBitDepth);
        } else {
            // OpenCV 标准：算法输入统一使用 8-bit 灰度 (0..255)
            // 不使用位段提取：改为基于 bitDepth 的固定线性缩放（0..maxVal -> 0..255）。
            // 注意：不建议使用每帧 min/max 的 normalize（会导致阈值类参数语义不稳定、画面“跳亮度”）。
            if (!dataNative.isEmpty() && width > 0 && height > 0) {
                const int pitchBytes = width * static_cast<int>(sizeof(uint16_t));
                QImage img8 = ImageDepthConverter::convertWithMethod(DepthConversionMethod::LinearScaling,
                                                                     dataNative.constData(), width, height, pitchBytes, effectiveBitDepth);
                imageAlgorithmManager_->processImage(img8);
            }
        }
    }

    // 转发原始数据到混合处理对话框
    if (mixedProcessingDialog_ && mixedProcessingDialog_->isVisible()) {
        mixedProcessingDialog_->processRawImage(dataNative, width, height, effectiveBitDepth);
    }
    
#if ENABLE_SPOT_DETECTION
    // 处理光斑检测
    if (spotDetectionRunning_ && threadManager_ && threadManager_->spotDetectionWorker()) {
        // 将 16-bit raw 数据转换为 OpenCV Mat
        cv::Mat rawMat;
        if (effectiveBitDepth <= 8) {
            // 转换为 8-bit
            rawMat = cv::Mat(height, width, CV_8UC1);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int idx = y * width + x;
                    rawMat.at<uint8_t>(y, x) = static_cast<uint8_t>(dataNative[idx] & 0xFF);
                }
            }
        } else {
            // 保留 16-bit
            rawMat = cv::Mat(height, width, CV_16UC1);
            std::memcpy(rawMat.data, dataNative.constData(), dataNative.size() * sizeof(uint16_t));
        }
        
        // 发送到光斑检测工作线程
        // 注意：首次需要提供初始位置，这里使用图像中心作为初始点
        if (spotDetectionFirstFrame_) {
            QMetaObject::invokeMethod(threadManager_->spotDetectionWorker(), 
                                      "processImage",
                                      Qt::QueuedConnection,
                                      Q_ARG(cv::Mat, rawMat),
                                      Q_ARG(double, width / 2.0),
                                      Q_ARG(double, height / 2.0));
            spotDetectionFirstFrame_ = false;
        } else {
            QMetaObject::invokeMethod(threadManager_->spotDetectionWorker(), 
                                      "processImage",
                                      Qt::QueuedConnection,
                                      Q_ARG(cv::Mat, rawMat),
                                      Q_ARG(double, -1),
                                      Q_ARG(double, -1));
        }
    }
#endif
}


