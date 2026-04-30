#include "mixed_processing_panel.h"
#include "image_algorithm_manager.h"
#include "opencv_qt_bridge.h"
#include "icon_cache.h"

#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QFrame>
#include <QGroupBox>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QDateTime>
#include <QMap>
// Forward declaration of helper functions
static cv::UMat processAlgorithmDirectly(const QString &algorithmId, const cv::UMat &input, const QVariantMap &params);
static cv::cuda::GpuMat processAlgorithmCuda(const QString &algorithmId, const cv::cuda::GpuMat &input, const QVariantMap &params);

// =========================== MixedProcessingWorker ===========================

MixedProcessingWorker::MixedProcessingWorker(QObject *parent)
    : QObject(parent)
{
    // Cache availability once; per-frame queries can cause jitter.
#ifdef CAMERUI_ENABLE_CUDA
    try {
        m_cudaAvailable = (cv::cuda::getCudaEnabledDeviceCount() > 0);
    } catch (...) {
        m_cudaAvailable = false;
    }

    if (m_cudaAvailable) {
        try {
            m_cudaStream = cv::makePtr<cv::cuda::Stream>();
        } catch (...) {
            m_cudaAvailable = false;
            m_cudaStream.release();
        }
    }
#else
    m_cudaAvailable = false;
#endif

    try {
        m_openclAvailable = cv::ocl::haveOpenCL();
        if (m_openclAvailable) {
            cv::ocl::setUseOpenCL(true);
        }
    } catch (...) {
        m_openclAvailable = false;
    }
}

MixedProcessingWorker::~MixedProcessingWorker()
{
    stop();
}

void MixedProcessingWorker::stop()
{
    m_shouldStop.store(true);
}

void MixedProcessingWorker::processImage(cv::Mat input, QVector<PipelineStep> steps)
{
    // Skip if already processing to avoid queue buildup
    if (m_isProcessing.load()) {
        return;
    }
    
    if (m_shouldStop.load()) {
        return;
    }
    
    m_isProcessing.store(true);
    
    QElapsedTimer timer;
    timer.start();
    
    try {
        cv::Mat result = runPipeline(input, steps);
        qint64 elapsed = timer.elapsed();
        
        if (!m_shouldStop.load()) {
            emit processingFinished(result, elapsed);
        }
    } catch (const cv::Exception &e) {
        emit processingError(QString("OpenCV error: %1").arg(e.what()));
    } catch (const std::exception &e) {
        emit processingError(QString("Error: %1").arg(e.what()));
    } catch (...) {
        emit processingError("Unknown error occurred");
    }
    
    m_isProcessing.store(false);
}

cv::Mat MixedProcessingWorker::runPipeline(cv::Mat input, const QVector<PipelineStep> &steps)
{
    if (input.empty()) {
        return cv::Mat();
    }

    // Resize cache if needed
    if (m_algorithmCache.size() != steps.size()) {
        m_algorithmCache.resize(steps.size());
        // Reset cache items
        for(int i=0; i<m_algorithmCache.size(); ++i) {
            m_algorithmCache[i] = CachedAlgorithm();
        }
    }

    bool cudaAvailable = m_cudaAvailable;

#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat gpuCurrent;
#endif
    cv::UMat umatCurrent;
    bool isDataOnGpu = false;
    bool isDataOnUmat = false;
    
    // Keep track of current data on CPU if not on GPU/UMat
    cv::Mat currentCpu = input;

    for (int i = 0; i < steps.size(); ++i) {
        const PipelineStep &step = steps[i];
        if (!step.enabled || m_shouldStop.load()) {
            continue;
        }

        updateAlgorithmCache(i, step, 0);
        CachedAlgorithm &cache = m_algorithmCache[i];
        
        if (!cache.instance) {
            continue;
        }

        bool processed = false;

        // Try CUDA with enhanced error handling
#ifdef CAMERUI_ENABLE_CUDA
        if (cudaAvailable && cache.instance->supportsCuda()) {
            try {
                if (!isDataOnGpu) {
                    if (isDataOnUmat) {
                        cv::Mat tmp;
                        umatCurrent.copyTo(tmp);
                        gpuCurrent.upload(tmp, *m_cudaStream);
                    } else {
                        gpuCurrent.upload(currentCpu, *m_cudaStream);
                    }
                    isDataOnGpu = true;
                    isDataOnUmat = false;
                }

                gpuCurrent = cache.instance->executeCuda(gpuCurrent, *m_cudaStream);
                processed = true;
            } catch (const cv::Exception &e) {
                qWarning() << "CUDA execution failed for" << step.algorithmId << ":" << e.what() << "- falling back to CPU/OpenCL";
                // Transfer data back from GPU for CPU fallback
                if (isDataOnGpu) {
                    try {
                        cv::Mat temp;
                        gpuCurrent.download(temp, *m_cudaStream);
                        m_cudaStream->waitForCompletion();
                        currentCpu = temp;
                        isDataOnGpu = false;
                    } catch (...) {
                        qWarning() << "Failed to download from GPU, using last known CPU state";
                    }
                }
            }
        }
#endif // CAMERUI_ENABLE_CUDA

        if (!processed) {
            // CPU/OpenCL path with error handling
#ifdef CAMERUI_ENABLE_CUDA
            if (isDataOnGpu) {
                try {
                    cv::Mat temp;
                    gpuCurrent.download(temp, *m_cudaStream);
                    m_cudaStream->waitForCompletion();
                    currentCpu = temp;
                    isDataOnGpu = false;
                } catch (const cv::Exception &e) {
                    qWarning() << "Failed to download from GPU:" << e.what() << "- skipping step" << step.algorithmId;
                    emit processingError(QString("Algorithm '%1': failed to transfer from GPU - skipping")
                                         .arg(step.algorithmId));
                    continue;
                }
            }
#endif // CAMERUI_ENABLE_CUDA
            
            if (!isDataOnUmat) {
                currentCpu.copyTo(umatCurrent);
                isDataOnUmat = true;
            }
            
            try {
                umatCurrent = cache.instance->execute(umatCurrent);
            } catch (const cv::Exception &e) {
                qWarning() << "CPU/OpenCL execution failed for" << step.algorithmId << ":" << e.what();
                // Keep previous result - don't modify umatCurrent to maintain pipeline state
                // Emit a warning but continue processing with last valid data
                emit processingError(QString("Algorithm '%1' failed: %2 - using previous result")
                                     .arg(step.algorithmId, e.what()));
                continue; // Skip to next step
            }
            processed = true; // Mark as successfully processed
        }
    }

#ifdef CAMERUI_ENABLE_CUDA
    if (isDataOnGpu) {
        cv::Mat result;
        gpuCurrent.download(result, *m_cudaStream);
        m_cudaStream->waitForCompletion();
        return result;
    } else
#endif // CAMERUI_ENABLE_CUDA
    if (isDataOnUmat) {
        cv::Mat result;
        umatCurrent.copyTo(result);
        return result;
    } else {
        return currentCpu;
    }
}

void MixedProcessingWorker::updateAlgorithmCache(int index, const PipelineStep &step, int inputType)
{
    if (index < 0 || index >= m_algorithmCache.size()) return;
    
    CachedAlgorithm &cache = m_algorithmCache[index];
    
    bool needsCreation = (cache.algorithmId != step.algorithmId) || !cache.instance;
    
    if (needsCreation) {
        cache.algorithmId = step.algorithmId;
        // Create new instance using factory
        ImageAlgorithmBase* rawPtr = ImageAlgorithmFactory::instance().createAlgorithm(step.algorithmId);
        if (rawPtr) {
            cache.instance.reset(rawPtr);
        } else {
            cache.instance.clear();
        }
    }
    
    if (cache.instance) {
        cache.instance->updateParameters(step.params);
    }
}

// executeAlgorithmCuda is no longer needed as we use ImageAlgorithmBase::executeCuda
#ifdef CAMERUI_ENABLE_CUDA
cv::cuda::GpuMat MixedProcessingWorker::executeAlgorithmCuda(int index, const cv::cuda::GpuMat &input, const PipelineStep &step)
{
    Q_UNUSED(index);
    Q_UNUSED(input);
    Q_UNUSED(step);
    return cv::cuda::GpuMat();
}
#endif // CAMERUI_ENABLE_CUDA

// =========================== DraggableAlgorithmItem ===========================

DraggableAlgorithmItem::DraggableAlgorithmItem(const AlgorithmInfo &info, QWidget *parent)
    : QWidget(parent), m_info(info)
{
    setFixedHeight(60);
    setMinimumWidth(120);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setCursor(Qt::OpenHandCursor);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(2);
    
    QLabel *nameLabel = new QLabel(info.name, this);
    nameLabel->setStyleSheet("font-weight: bold; font-size: 11px;");
    nameLabel->setWordWrap(true);
    layout->addWidget(nameLabel);
    
    setStyleSheet(
        "DraggableAlgorithmItem {"
        "  background-color: #f0f4f8;"
        "  border: 1px solid #ccc;"
        "  border-radius: 4px;"
        "}"
        "DraggableAlgorithmItem:hover {"
        "  background-color: #e0e8f0;"
        "  border-color: #999;"
        "}"
    );
}

void DraggableAlgorithmItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = event->pos();
    }
    QWidget::mousePressEvent(event);
}

void DraggableAlgorithmItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        return;
    }
    
    if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }
    
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    mimeData->setData("application/x-algorithm-id", m_info.id.toUtf8());
    drag->setMimeData(mimeData);
    
    // Create a preview pixmap
    QPixmap pixmap(size());
    render(&pixmap);
    drag->setPixmap(pixmap.scaled(100, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    drag->setHotSpot(QPoint(50, 25));
    
    setCursor(Qt::ClosedHandCursor);
    drag->exec(Qt::CopyAction);
    setCursor(Qt::OpenHandCursor);
}

// =========================== AlgorithmBlockWidget ===========================

AlgorithmBlockWidget::AlgorithmBlockWidget(const AlgorithmInfo &info, QWidget *parent)
    : QWidget(parent), m_info(info)
{
    setupUI();
}

AlgorithmBlockWidget::~AlgorithmBlockWidget()
{
}

void AlgorithmBlockWidget::setupUI()
{
    setMinimumWidth(180);
    setMaximumWidth(250);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(4);
    
    // Header with title and controls
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(4);
    
    m_enableCheckBox = new QCheckBox(this);
    m_enableCheckBox->setChecked(true);
    m_enableCheckBox->setToolTip(tr("启用/禁用此算法"));
    connect(m_enableCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_enabled = checked;
        emit enabledChanged(checked);
        emit parametersChanged();
    });
    headerLayout->addWidget(m_enableCheckBox);
    
    m_titleLabel = new QLabel(m_info.name, this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 11px;");
    m_titleLabel->setWordWrap(true);
    headerLayout->addWidget(m_titleLabel, 1);
    
    // Move buttons
    m_moveUpButton = new QPushButton(this);
    m_moveUpButton->setFixedSize(20, 20);
    m_moveUpButton->setToolTip(tr("前移"));
    m_moveUpButton->setIcon(QIcon(":/icons/left.svg"));
    m_moveUpButton->setIconSize(QSize(16, 16));
    m_moveUpButton->setStyleSheet("QPushButton { border: none; padding: 0px; }");
    connect(m_moveUpButton, &QPushButton::clicked, this, &AlgorithmBlockWidget::moveUpRequested);
    headerLayout->addWidget(m_moveUpButton);
    
    m_moveDownButton = new QPushButton(this);
    m_moveDownButton->setFixedSize(20, 20);
    m_moveDownButton->setToolTip(tr("后移"));
    QPixmap rightPixmap = IconCache::leftIcon().pixmap(QSize(16, 16)).transformed(QTransform().rotate(180));
    m_moveDownButton->setIcon(QIcon(rightPixmap));
    m_moveDownButton->setIconSize(QSize(16, 16));
    m_moveDownButton->setStyleSheet("QPushButton { border: none; padding: 0px; }");
    connect(m_moveDownButton, &QPushButton::clicked, this, &AlgorithmBlockWidget::moveDownRequested);
    headerLayout->addWidget(m_moveDownButton);
    
    m_removeButton = new QPushButton(this);
    m_removeButton->setFixedSize(20, 20);
    m_removeButton->setToolTip(tr("移除"));
    
    QPixmap closePixmap(":/icons/close.svg");
    QPainter painter(&closePixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(closePixmap.rect(), QColor(255, 0, 0));
    painter.end();
    
    m_removeButton->setIcon(QIcon(closePixmap));
    m_removeButton->setIconSize(QSize(16, 16));
    m_removeButton->setStyleSheet("QPushButton { border: none; padding: 0px; }");
    connect(m_removeButton, &QPushButton::clicked, this, &AlgorithmBlockWidget::removeRequested);
    headerLayout->addWidget(m_removeButton);
    
    m_mainLayout->addLayout(headerLayout);
    
    // Separator
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_mainLayout->addWidget(line);
    
    // Parameters container
    m_paramsContainer = new QWidget(this);
    m_paramsLayout = new QVBoxLayout(m_paramsContainer);
    m_paramsLayout->setContentsMargins(0, 0, 0, 0);
    m_paramsLayout->setSpacing(4);
    
    for (const AlgorithmParameter &param : m_info.parameters) {
        QWidget *paramWidget = createParameterWidget(param);
        if (paramWidget) {
            m_paramsLayout->addWidget(paramWidget);
        }
    }
    
    m_mainLayout->addWidget(m_paramsContainer);
    m_mainLayout->addStretch();
    
    // Block styling
    setStyleSheet(
        "AlgorithmBlockWidget {"
        "  background-color: #ffffff;"
        "  border: 2px solid #3498db;"
        "  border-radius: 6px;"
        "}"
    );
}

QWidget* AlgorithmBlockWidget::createParameterWidget(const AlgorithmParameter &param)
{
    QWidget *container = new QWidget(m_paramsContainer);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    
    QLabel *label = new QLabel(param.displayName + ":", container);
    label->setStyleSheet("font-size: 10px;");
    label->setMinimumWidth(60);
    layout->addWidget(label);
    
    QWidget *inputWidget = nullptr;
    
    if (param.type == "int") {
        QSpinBox *spin = new QSpinBox(container);
        spin->setRange(param.minValue.toInt(), param.maxValue.toInt());
        spin->setValue(param.defaultValue.toInt());
        if (param.step.isValid()) {
            spin->setSingleStep(param.step.toInt());
        }
        spin->setToolTip(param.tooltip);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &AlgorithmBlockWidget::onParameterChanged);
        inputWidget = spin;
        
    } else if (param.type == "double") {
        QDoubleSpinBox *dspin = new QDoubleSpinBox(container);
        dspin->setRange(param.minValue.toDouble(), param.maxValue.toDouble());
        dspin->setValue(param.defaultValue.toDouble());
        if (param.step.isValid()) {
            dspin->setSingleStep(param.step.toDouble());
        }
        dspin->setDecimals(2);
        dspin->setToolTip(param.tooltip);
        connect(dspin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &AlgorithmBlockWidget::onParameterChanged);
        inputWidget = dspin;
        
    } else if (param.type == "bool") {
        QCheckBox *check = new QCheckBox(container);
        check->setChecked(param.defaultValue.toBool());
        check->setToolTip(param.tooltip);
        connect(check, &QCheckBox::toggled,
                this, &AlgorithmBlockWidget::onParameterChanged);
        inputWidget = check;
        
    } else if (param.type == "enum") {
        QComboBox *combo = new QComboBox(container);
        combo->addItems(param.enumValues);
        combo->setCurrentIndex(param.defaultValue.toInt());
        combo->setToolTip(param.tooltip);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &AlgorithmBlockWidget::onParameterChanged);
        inputWidget = combo;
    }
    
    if (inputWidget) {
        inputWidget->setMaximumWidth(80);
        layout->addWidget(inputWidget, 1);
        m_paramWidgets[param.name] = inputWidget;
    }
    
    return container;
}

void AlgorithmBlockWidget::onParameterChanged()
{
    emit parametersChanged();
}

QVariantMap AlgorithmBlockWidget::getParameters() const
{
    QVariantMap params;
    
    for (auto it = m_paramWidgets.begin(); it != m_paramWidgets.end(); ++it) {
        const QString &name = it.key();
        QWidget *widget = it.value();
        
        if (QSpinBox *spin = qobject_cast<QSpinBox*>(widget)) {
            params[name] = spin->value();
        } else if (QDoubleSpinBox *dspin = qobject_cast<QDoubleSpinBox*>(widget)) {
            params[name] = dspin->value();
        } else if (QCheckBox *check = qobject_cast<QCheckBox*>(widget)) {
            params[name] = check->isChecked();
        } else if (QComboBox *combo = qobject_cast<QComboBox*>(widget)) {
            params[name] = combo->currentIndex();
        }
    }
    
    return params;
}

void AlgorithmBlockWidget::setParameters(const QVariantMap &params)
{
    for (auto it = m_paramWidgets.begin(); it != m_paramWidgets.end(); ++it) {
        const QString &name = it.key();
        QWidget *widget = it.value();
        
        if (!params.contains(name)) {
            continue;
        }
        
        const QVariant &value = params.value(name);
        
        if (QSpinBox *spin = qobject_cast<QSpinBox*>(widget)) {
            spin->blockSignals(true);
            spin->setValue(value.toInt());
            spin->blockSignals(false);
        } else if (QDoubleSpinBox *dspin = qobject_cast<QDoubleSpinBox*>(widget)) {
            dspin->blockSignals(true);
            dspin->setValue(value.toDouble());
            dspin->blockSignals(false);
        } else if (QCheckBox *check = qobject_cast<QCheckBox*>(widget)) {
            check->blockSignals(true);
            check->setChecked(value.toBool());
            check->blockSignals(false);
        } else if (QComboBox *combo = qobject_cast<QComboBox*>(widget)) {
            combo->blockSignals(true);
            combo->setCurrentIndex(value.toInt());
            combo->blockSignals(false);
        }
    }
}

void AlgorithmBlockWidget::setBlockEnabled(bool enabled)
{
    m_enabled = enabled;
    m_enableCheckBox->setChecked(enabled);
}

// =========================== PipelineDropArea ===========================

PipelineDropArea::PipelineDropArea(QWidget *parent)
    : QScrollArea(parent)
{
    setAcceptDrops(true);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setMinimumHeight(200);
    
    m_container = new QWidget(this);
    m_layout = new QHBoxLayout(m_container);
    m_layout->setContentsMargins(10, 10, 10, 10);
    m_layout->setSpacing(10);
    m_layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    // Input arrow
    m_inputArrow = new QLabel("输入", m_container);
    m_inputArrow->setStyleSheet("font-size: 20px; color: #d4d4d4;");
    m_inputArrow->setAlignment(Qt::AlignCenter);
    m_inputArrow->setFixedWidth(50);
    m_layout->addWidget(m_inputArrow);
    
    // Placeholder text
    m_placeholderLabel = new QLabel(tr("← 将算法拖放到此处 →"), m_container);
    m_placeholderLabel->setStyleSheet("color: #888; font-size: 14px; padding: 20px;");
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_placeholderLabel, 1);
    
    // Output arrow
    m_outputArrow = new QLabel("\n输出", m_container);
    m_outputArrow->setStyleSheet("font-size: 20px; color: #d4d4d4;");
    m_outputArrow->setAlignment(Qt::AlignCenter);
    m_outputArrow->setFixedWidth(50);
    m_layout->addWidget(m_outputArrow);
    
    setWidget(m_container);
    
    setStyleSheet(
        "PipelineDropArea {"
        "  background-color: #f8f9fa;"
        "  border: 2px dashed #bbb;"
        "  border-radius: 8px;"
        "}"
    );
}

void PipelineDropArea::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-algorithm-id")) {
        event->acceptProposedAction();
        setStyleSheet(
            "PipelineDropArea {"
            "  background-color: #e8f5e9;"
            "  border: 2px dashed #4caf50;"
            "  border-radius: 8px;"
            "}"
        );
    }
}

void PipelineDropArea::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-algorithm-id")) {
        event->acceptProposedAction();
    }
}

void PipelineDropArea::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-algorithm-id")) {
        QString algorithmId = QString::fromUtf8(event->mimeData()->data("application/x-algorithm-id"));
        
        // Find the correct algorithm info
        const QVector<AlgorithmInfo> allInfos = ImageAlgorithmFactory::instance().allAlgorithmInfos();
        AlgorithmInfo info;
        bool found = false;
        for (const AlgorithmInfo &i : allInfos) {
            if (i.id == algorithmId) {
                info = i;
                found = true;
                break;
            }
        }
        
        // Only create block if algorithm was found
        if (found) {
            AlgorithmBlockWidget *block = new AlgorithmBlockWidget(info, m_container);
            addAlgorithmBlock(block);
        }
        
        event->acceptProposedAction();
    }
    
    setStyleSheet(
        "PipelineDropArea {"
        "  background-color: #f8f9fa;"
        "  border: 2px dashed #bbb;"
        "  border-radius: 8px;"
        "}"
    );
}

void PipelineDropArea::addAlgorithmBlock(AlgorithmBlockWidget *block)
{
    // Hide placeholder when we have blocks
    m_placeholderLabel->setVisible(false);
    
    // Insert before output arrow
    int insertIndex = m_layout->indexOf(m_outputArrow);
    m_layout->insertWidget(insertIndex, block);
    
    m_blocks.append(block);
    
    // Connect block signals
    connect(block, &AlgorithmBlockWidget::removeRequested, this, [this, block]() {
        removeAlgorithmBlock(block);
    });
    
    connect(block, &AlgorithmBlockWidget::moveUpRequested, this, [this, block]() {
        moveBlockUp(block);
    });
    
    connect(block, &AlgorithmBlockWidget::moveDownRequested, this, [this, block]() {
        moveBlockDown(block);
    });
    
    connect(block, &AlgorithmBlockWidget::parametersChanged, this, &PipelineDropArea::pipelineChanged);
    
    emit blockAdded(block);
    emit pipelineChanged();
}

void PipelineDropArea::removeAlgorithmBlock(AlgorithmBlockWidget *block)
{
    int index = m_blocks.indexOf(block);
    if (index >= 0) {
        m_blocks.remove(index);
        m_layout->removeWidget(block);
        block->deleteLater();
        
        // Show placeholder if no blocks
        m_placeholderLabel->setVisible(m_blocks.isEmpty());
        
        emit blockRemoved(block);
        emit pipelineChanged();
    }
}

void PipelineDropArea::moveBlockUp(AlgorithmBlockWidget *block)
{
    int index = m_blocks.indexOf(block);
    if (index > 0) {
        m_blocks.swapItemsAt(index, index - 1);
        updateLayout();
        emit pipelineChanged();
    }
}

void PipelineDropArea::moveBlockDown(AlgorithmBlockWidget *block)
{
    int index = m_blocks.indexOf(block);
    if (index >= 0 && index < m_blocks.size() - 1) {
        m_blocks.swapItemsAt(index, index + 1);
        updateLayout();
        emit pipelineChanged();
    }
}

void PipelineDropArea::updateLayout()
{
    // Temporarily remove all blocks
    for (AlgorithmBlockWidget *block : m_blocks) {
        m_layout->removeWidget(block);
    }
    
    // Re-add in correct order (after input arrow, before output arrow)
    int insertIndex = m_layout->indexOf(m_placeholderLabel);
    if (insertIndex < 0) {
        insertIndex = 1; // After input arrow
    }
    
    for (AlgorithmBlockWidget *block : m_blocks) {
        m_layout->insertWidget(insertIndex++, block);
    }
}

void PipelineDropArea::clear()
{
    while (!m_blocks.isEmpty()) {
        AlgorithmBlockWidget *block = m_blocks.takeLast();
        m_layout->removeWidget(block);
        block->deleteLater();
    }
    
    m_placeholderLabel->setVisible(true);
    emit pipelineChanged();
}

// =========================== AlgorithmListWidget ===========================

AlgorithmListWidget::AlgorithmListWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void AlgorithmListWidget::setupUI()
{
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(4);
    
    QLabel *title = new QLabel(tr("算法模块 (拖放到上方处理区)"), this);
    title->setStyleSheet("font-weight: bold; font-size: 12px; padding: 4px;");
    outerLayout->addWidget(title);
    
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    m_container = new QWidget(m_scrollArea);
    m_mainLayout = new QVBoxLayout(m_container);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setAlignment(Qt::AlignTop);
    
    m_scrollArea->setWidget(m_container);
    outerLayout->addWidget(m_scrollArea);
}

void AlgorithmListWidget::setAlgorithmInfos(const QVector<AlgorithmInfo> &infos)
{
    // Clear existing items
    QLayoutItem *item;
    while ((item = m_mainLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    
    // Group algorithms by category
    QMap<QString, QVector<AlgorithmInfo>> categoryMap;
    for (const AlgorithmInfo &info : infos) {
        QString category = info.category.isEmpty() ? tr("未分类") : info.category;
        categoryMap[category].append(info);
    }
    
    // Add items by category
    for (auto it = categoryMap.constBegin(); it != categoryMap.constEnd(); ++it) {
        const QString &category = it.key();
        const QVector<AlgorithmInfo> &categoryInfos = it.value();
        
        // Create category label
        QLabel *categoryLabel = new QLabel(category, m_container);
        categoryLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: #666; padding: 2px 0;");
        m_mainLayout->addWidget(categoryLabel);
        
        // Create horizontal layout for this category
        QWidget *categoryWidget = new QWidget(m_container);
        QHBoxLayout *categoryLayout = new QHBoxLayout(categoryWidget);
        categoryLayout->setContentsMargins(0, 0, 0, 0);
        categoryLayout->setSpacing(8);
        categoryLayout->setAlignment(Qt::AlignLeft);
        
        // Add algorithm items for this category
        for (const AlgorithmInfo &info : categoryInfos) {
            DraggableAlgorithmItem *item = new DraggableAlgorithmItem(info, categoryWidget);
            categoryLayout->addWidget(item);
        }
        
        categoryLayout->addStretch();
        m_mainLayout->addWidget(categoryWidget);
    }
    
    m_mainLayout->addStretch();
}

// =========================== MixedProcessingDialog ===========================

MixedProcessingDialog::MixedProcessingDialog(QWidget *parent)
    : QDialog(parent)
    , m_frameCount(0)
    , m_lastFpsUpdateTime(0)
{
    setWindowTitle(tr("混合处理"));
    setMinimumSize(1200, 800);
    resize(1400, 900);
    
    m_lastFpsUpdateTime = QDateTime::currentMSecsSinceEpoch();

    m_processTimer = new QTimer(this);
    m_processTimer->setSingleShot(true);
    m_processTimer->setInterval(0); // updated dynamically by frame rate limit
    connect(m_processTimer, &QTimer::timeout, this, &MixedProcessingDialog::onProcessingTimerTimeout);
    
    // Start throttle clock
    m_throttleClock.start();
    
    // Create dedicated worker thread for processing
    m_workerThread = new QThread(this);
    m_workerThread->setObjectName("MixedProcessingWorkerThread");
    
    m_worker = new MixedProcessingWorker();
    m_worker->moveToThread(m_workerThread);
    
    // Connect worker signals
    connect(m_worker, &MixedProcessingWorker::processingFinished,
            this, &MixedProcessingDialog::onProcessingFinished, Qt::QueuedConnection);
    connect(m_worker, &MixedProcessingWorker::processingError,
            this, &MixedProcessingDialog::onProcessingError, Qt::QueuedConnection);
    
    // Clean up worker when thread finishes
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    
    // Start worker thread
    m_workerThread->start();

    setupUI();
}

MixedProcessingDialog::~MixedProcessingDialog()
{
    // Stop worker and thread
    if (m_worker) {
        m_worker->stop();
    }
    
    if (m_workerThread) {
        m_workerThread->quit();
        if (!m_workerThread->wait(2000)) {
            qWarning() << "MixedProcessing worker thread did not finish in time";
            m_workerThread->terminate();
            m_workerThread->wait();
        }
    }
}

void MixedProcessingDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // === Top: Precision mode selection ===
    QHBoxLayout *controlLayout = new QHBoxLayout();
    
    QLabel *precisionLabel = new QLabel(tr("精度模式:"), this);
    m_precisionCombo = new QComboBox(this);
    m_precisionCombo->addItem(tr("标准 (8位)"), static_cast<int>(PrecisionMode::Standard8Bit));
    m_precisionCombo->addItem(tr("高精度 (16位)"), static_cast<int>(PrecisionMode::HighPrecision16Bit));
    connect(m_precisionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MixedProcessingDialog::onPrecisionModeChanged);
    
    // Frame rate throttling controls
    m_frameRateLabel = new QLabel(tr("帧率限速:"), this);
    m_frameRateCombo = new QComboBox(this);
    m_frameRateCombo->addItem(tr("30 帧"), 30);
    m_frameRateCombo->addItem(tr("60 帧"), 60);
    m_frameRateCombo->addItem(tr("120 帧"), 120);
    m_frameRateCombo->addItem(tr("自定义"), -1);
    m_frameRateCombo->addItem(tr("无限制"), 0);
    m_frameRateCombo->setCurrentIndex(0); // default 30
    connect(m_frameRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MixedProcessingDialog::onFrameRateLimitChanged);
    
    m_customFpsSpin = new QSpinBox(this);
    m_customFpsSpin->setRange(1, 1000);
    m_customFpsSpin->setValue(30);
    m_customFpsSpin->setSuffix(tr(" 帧"));
    m_customFpsSpin->setVisible(false);
    connect(m_customFpsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MixedProcessingDialog::onFrameRateLimitChanged);
    
    m_fpsLabel = new QLabel(tr("FPS: --"), this);
    m_fpsLabel->setStyleSheet("color: #b4b4b4; font-weight: bold;");

    m_statsLabel = new QLabel(tr("处理耗时: -- ms"), this);
    m_statsLabel->setStyleSheet("color: #b4b4b4;");
    
    m_clearButton = new QPushButton(tr("清空管道"), this);
    connect(m_clearButton, &QPushButton::clicked, this, &MixedProcessingDialog::onClearClicked);
    
    m_closeButton = new QPushButton(tr("关闭"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);
    
    controlLayout->addWidget(precisionLabel);
    controlLayout->addWidget(m_precisionCombo);
    controlLayout->addSpacing(20);
    controlLayout->addWidget(m_frameRateLabel);
    controlLayout->addWidget(m_frameRateCombo);
    controlLayout->addWidget(m_customFpsSpin);
    controlLayout->addSpacing(20);
    controlLayout->addWidget(m_fpsLabel);
    controlLayout->addSpacing(10);
    controlLayout->addWidget(m_statsLabel);
    controlLayout->addStretch();
    controlLayout->addWidget(m_clearButton);
    controlLayout->addWidget(m_closeButton);
    
    m_mainLayout->addLayout(controlLayout);
    
    // === Image comparison area ===
    m_imagesLayout = new QHBoxLayout();
    
    // Original image
    QWidget *originalGroup = new QWidget(this);
    QVBoxLayout *originalLayout = new QVBoxLayout(originalGroup);
    originalLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *originalTitle = new QLabel(tr("原始图像"), originalGroup);
    originalTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    originalTitle->setAlignment(Qt::AlignCenter);
    originalLayout->addWidget(originalTitle);
    
    m_originalImageWidget = new ZoomableImageWidget(this);
    m_originalImageWidget->setMinimumSize(400, 300);
    originalLayout->addWidget(m_originalImageWidget, 1);
    
    m_originalInfoLabel = new QLabel(tr("等待图像..."), originalGroup);
    m_originalInfoLabel->setAlignment(Qt::AlignCenter);
    originalLayout->addWidget(m_originalInfoLabel);
    
    // Processed image
    QWidget *processedGroup = new QWidget(this);
    QVBoxLayout *processedLayout = new QVBoxLayout(processedGroup);
    processedLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *processedTitle = new QLabel(tr("处理后图像"), processedGroup);
    processedTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    processedTitle->setAlignment(Qt::AlignCenter);
    processedLayout->addWidget(processedTitle);
    
    m_processedImageWidget = new ZoomableImageWidget(this);
    m_processedImageWidget->setMinimumSize(400, 300);
    processedLayout->addWidget(m_processedImageWidget, 1);
    
    m_processedInfoLabel = new QLabel(tr("等待处理..."), processedGroup);
    m_processedInfoLabel->setAlignment(Qt::AlignCenter);
    processedLayout->addWidget(m_processedInfoLabel);
    
    m_imagesLayout->addWidget(originalGroup);
    m_imagesLayout->addWidget(processedGroup);
    
    // Synchronize zoom and pan
    connect(m_originalImageWidget, &ZoomableImageWidget::transformChanged,
            m_processedImageWidget, &ZoomableImageWidget::setTransform);
    connect(m_processedImageWidget, &ZoomableImageWidget::transformChanged,
            m_originalImageWidget, &ZoomableImageWidget::setTransform);
    
    m_mainLayout->addLayout(m_imagesLayout, 2);
    
    // === Middle: Pipeline workspace ===
    QLabel *pipelineTitle = new QLabel(tr("处理管道 (从左到右依次处理)"), this);
    pipelineTitle->setStyleSheet("font-weight: bold; font-size: 12px;");
    m_mainLayout->addWidget(pipelineTitle);
    
    m_pipelineArea = new PipelineDropArea(this);
    connect(m_pipelineArea, &PipelineDropArea::pipelineChanged, this, &MixedProcessingDialog::onPipelineChanged);
    m_mainLayout->addWidget(m_pipelineArea);
    
    // === Bottom: Algorithm list ===
    m_algorithmList = new AlgorithmListWidget(this);
    m_mainLayout->addWidget(m_algorithmList);
}

void MixedProcessingDialog::setAlgorithmManager(ImageAlgorithmManager *manager)
{
    m_algorithmManager = manager;
    populateAlgorithms();
}

void MixedProcessingDialog::populateAlgorithms()
{
    QVector<AlgorithmInfo> allInfos = ImageAlgorithmFactory::instance().allAlgorithmInfos();
    
    // Filter algorithms based on precision mode
    if (m_precisionMode == PrecisionMode::HighPrecision16Bit) {
        QVector<AlgorithmInfo> filtered;
        filtered.reserve(allInfos.size());
        for (const AlgorithmInfo &info : allInfos) {
            if (!AlgorithmPrecisionUtils::shouldHideInHighPrecision(info.id)) {
                filtered.append(info);
            }
        }
        m_algorithmList->setAlgorithmInfos(filtered);
    } else {
        m_algorithmList->setAlgorithmInfos(allInfos);
    }
}

void MixedProcessingDialog::setPrecisionMode(PrecisionMode mode)
{
    if (m_precisionMode != mode) {
        m_precisionMode = mode;
        m_precisionCombo->setCurrentIndex(static_cast<int>(mode));
        emit precisionModeChanged(mode);
        
        // Re-populate algorithms based on new precision mode
        populateAlgorithms();
        
        // Re-process with new mode
        m_needsProcessing = true;
        m_processTimer->start();
    }
}

void MixedProcessingDialog::onPrecisionModeChanged(int index)
{
    PrecisionMode mode = static_cast<PrecisionMode>(m_precisionCombo->itemData(index).toInt());
    if (m_precisionMode != mode) {
        m_precisionMode = mode;
        emit precisionModeChanged(mode);
        
        // Re-populate algorithms based on new precision mode
        populateAlgorithms();
        
        m_needsProcessing = true;
        m_processTimer->start();
    }
}

void MixedProcessingDialog::onPipelineChanged()
{
    m_needsProcessing = true;
    m_processTimer->start();
}

void MixedProcessingDialog::onProcessingTimerTimeout()
{
    if (m_needsProcessing) {
        processCurrentImage();
        m_needsProcessing = false;
    }
}

void MixedProcessingDialog::onApplyClicked()
{
    processCurrentImage();
}

void MixedProcessingDialog::onClearClicked()
{
    m_pipelineArea->clear();
}

void MixedProcessingDialog::onFrameRateLimitChanged()
{
    if (!m_frameRateCombo) {
        return;
    }
    
    const int data = m_frameRateCombo->currentData().toInt();
    bool isCustom = (data == -1);
    
    if (m_customFpsSpin) {
        m_customFpsSpin->setVisible(isCustom);
    }
    
    if (isCustom) {
        m_fpsLimit = m_customFpsSpin ? m_customFpsSpin->value() : 30;
    } else {
        m_fpsLimit = data; // 0 means unlimited
    }

    // Keep processing scheduling interval aligned with the selected fps limit.
    // Note: input throttling still exists; this mainly affects pacing/jitter.
    if (m_processTimer) {
        // We set interval to 0 to process immediately after throttling check passes.
        // The throttling logic in processRawImage/processImage already ensures we don't exceed the limit.
        // Adding extra delay here causes double-throttling and frame rate drops.
        m_processTimer->setInterval(0);
    }
}

void MixedProcessingDialog::processRawImage(const QVector<uint16_t> &rawData, int width, int height, int bitDepth)
{
    if (rawData.isEmpty() || width <= 0 || height <= 0) {
        return;
    }
    
    // Frame rate throttling
    if (m_fpsLimit > 0) {
        const qint64 nowNs = m_throttleClock.nsecsElapsed();
        const qint64 intervalNs = 1000000000LL / qMax(1, m_fpsLimit);
        
        if (nowNs - m_lastProcessedNs < intervalNs) {
            // Skip this frame - too soon
            return;
        }
        m_lastProcessedNs = nowNs;
    }
    
    QMutexLocker locker(&m_imageMutex);

    // Buffer latest raw frame; defer conversion/clone until we actually process.
    m_latestRawData = rawData;
    m_latestRawWidth = width;
    m_latestRawHeight = height;
    m_latestRawBitDepth = bitDepth;
    m_hasRawInput = true;

    // Prefer raw input when available
    m_hasQImageInput = false;
    
    m_needsProcessing = true;
    if (!m_processTimer->isActive()) {
        m_processTimer->start();
    }
}

void MixedProcessingDialog::processImage(const QImage &image)
{
    if (image.isNull()) {
        return;
    }
    
    // Frame rate throttling
    if (m_fpsLimit > 0) {
        const qint64 nowNs = m_throttleClock.nsecsElapsed();
        const qint64 intervalNs = 1000000000LL / qMax(1, m_fpsLimit);
        
        if (nowNs - m_lastProcessedNs < intervalNs) {
            // Skip this frame - too soon
            return;
        }
        m_lastProcessedNs = nowNs;
    }
    
    QMutexLocker locker(&m_imageMutex);

    // Buffer latest image; defer grayscale conversion/clone until we actually process.
    m_latestImage = image;
    m_hasQImageInput = true;

    // Prefer raw input when available
    m_hasRawInput = false;
    
    m_needsProcessing = true;
    if (!m_processTimer->isActive()) {
        m_processTimer->start();
    }
}

void MixedProcessingDialog::processCurrentImage()
{
    // If already processing, schedule next run
    if (m_isProcessing.load()) {
        m_needsProcessing = true;
        return;
    }

    cv::Mat input;
    int bitDepthForDisplay = 0;
    {
        QMutexLocker locker(&m_imageMutex);

        if (m_hasRawInput) {
            if (m_latestRawData.isEmpty() || m_latestRawWidth <= 0 || m_latestRawHeight <= 0) {
                return;
            }

            bitDepthForDisplay = m_latestRawBitDepth;
            const int maxVal = (bitDepthForDisplay > 0 && bitDepthForDisplay < 31)
                                   ? ((1 << bitDepthForDisplay) - 1)
                                   : 65535;

            cv::Mat mat16(m_latestRawHeight, m_latestRawWidth, CV_16UC1,
                          const_cast<uint16_t*>(m_latestRawData.constData()));

            if (m_precisionMode == PrecisionMode::HighPrecision16Bit) {
                // Keep as 16-bit to maximize CUDA/OpenCL support and avoid CPU fallback churn.
                input = mat16.clone();
            } else {
                // Standard 8-bit mode: scale to 0-255
                mat16.convertTo(input, CV_8U, 255.0 / qMax(1, maxVal));
            }
        } else if (m_hasQImageInput) {
            if (m_latestImage.isNull()) {
                return;
            }

            bitDepthForDisplay = 8;
            QImage gray = m_latestImage;
            if (gray.format() != QImage::Format_Grayscale8) {
                gray = gray.convertToFormat(QImage::Format_Grayscale8);
            }
            cv::Mat mat(gray.height(), gray.width(), CV_8UC1, const_cast<uchar*>(gray.bits()), gray.bytesPerLine());
            input = mat.clone();
        } else {
            return;
        }

        m_lastSubmittedInputMat = input;
        m_lastSubmittedBitDepth = bitDepthForDisplay;
    }
    
    // Collect pipeline steps from UI
    // In 16-bit high precision mode, scale parameters from UI range (0-255) to native range
    QVector<PipelineStep> steps;
    const QVector<AlgorithmBlockWidget*> &blocks = m_pipelineArea->getBlocks();
    for (AlgorithmBlockWidget *block : blocks) {
        if (block->isEnabled()) {
            QVariantMap params = block->getParameters();
            
            // Scale parameters for 16-bit mode
            if (m_precisionMode == PrecisionMode::HighPrecision16Bit && bitDepthForDisplay > 8) {
                params = AlgorithmPrecisionUtils::mapUiParamsToNativeRange(params, bitDepthForDisplay);
            }
            
            steps.append({block->algorithmId(), params, true});
        }
    }
    
    m_isProcessing.store(true);
    
    // Invoke processing on worker thread
    QMetaObject::invokeMethod(m_worker, "processImage", Qt::QueuedConnection,
                             Q_ARG(cv::Mat, input),
                             Q_ARG(QVector<PipelineStep>, steps));
}

void MixedProcessingDialog::onProcessingFinished(cv::Mat result, qint64 elapsedMs)
{
    m_isProcessing.store(false);
    
    // Update FPS
    m_frameCount++;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 timeDiff = currentTime - m_lastFpsUpdateTime;
    if (timeDiff >= 1000) {
        double fps = m_frameCount * 1000.0 / timeDiff;
        m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
        m_frameCount = 0;
        m_lastFpsUpdateTime = currentTime;
    }

    // Update statistics
    m_statsLabel->setText(tr("处理耗时: %1 ms | 管道步骤: %2").arg(elapsedMs).arg(m_pipelineArea->blockCount()));
    
    // Get original image for display update (use the exact submitted input)
    cv::Mat original;
    int bitDepthForDisplay = 0;
    {
        QMutexLocker locker(&m_imageMutex);
        if (!m_lastSubmittedInputMat.empty()) {
            original = m_lastSubmittedInputMat;
            bitDepthForDisplay = m_lastSubmittedBitDepth;
        }
    }
    
    if (!original.empty()) {
        // Store bit depth for scaling inside updateDisplay via member
        // (updateDisplay reads it from m_lastSubmittedBitDepth through the mutex-protected copy above)
        updateDisplay(original, result);
    }
    
    // If there was a pending request while we were processing, trigger it now
    if (m_needsProcessing) {
        m_needsProcessing = false;
        m_processTimer->start();
    }
}

void MixedProcessingDialog::onProcessingError(QString error)
{
    m_isProcessing.store(false);
    qWarning() << "Mixed processing error:" << error;
    m_statsLabel->setText(tr("处理错误: %1").arg(error));
    
    // If there was a pending request, trigger it
    if (m_needsProcessing) {
        m_needsProcessing = false;
        m_processTimer->start();
    }
}





void MixedProcessingDialog::updateDisplay(const cv::Mat &original, const cv::Mat &processed)
{
    // Convert original to QImage
    cv::Mat display8;
    if (original.type() == CV_32F) {
        // Legacy path (should be rare after input buffering fix)
        original.convertTo(display8, CV_8U);
    } else if (original.type() == CV_16U) {
        int bitDepth = 16;
        {
            QMutexLocker locker(&m_imageMutex);
            if (m_lastSubmittedBitDepth > 0) {
                bitDepth = m_lastSubmittedBitDepth;
            }
        }
        const int maxVal = (bitDepth > 0 && bitDepth < 31) ? ((1 << bitDepth) - 1) : 65535;
        original.convertTo(display8, CV_8U, 255.0 / qMax(1, maxVal));
    } else {
        display8 = original;
    }
    
    QImage origImage(display8.data, display8.cols, display8.rows, display8.step, QImage::Format_Grayscale8);
    m_originalImageWidget->setImage(origImage.copy());
    m_originalInfoLabel->setText(tr("尺寸: %1 x %2").arg(original.cols).arg(original.rows));
    
    // Convert processed to QImage
    cv::Mat procDisplay8;
    if (processed.type() == CV_32F) {
        processed.convertTo(procDisplay8, CV_8U);
    } else if (processed.type() == CV_16U) {
        int bitDepth = 16;
        {
            QMutexLocker locker(&m_imageMutex);
            if (m_lastSubmittedBitDepth > 0) {
                bitDepth = m_lastSubmittedBitDepth;
            }
        }
        const int maxVal = (bitDepth > 0 && bitDepth < 31) ? ((1 << bitDepth) - 1) : 65535;
        processed.convertTo(procDisplay8, CV_8U, 255.0 / qMax(1, maxVal));
    } else {
        procDisplay8 = processed;
    }
    
    QImage procImage(procDisplay8.data, procDisplay8.cols, procDisplay8.rows, procDisplay8.step, QImage::Format_Grayscale8);
    m_processedImageWidget->setImage(procImage.copy());
    m_processedInfoLabel->setText(tr("尺寸: %1 x %2").arg(processed.cols).arg(processed.rows));
}

void MixedProcessingDialog::closeEvent(QCloseEvent *event)
{
    emit closed();
    event->accept();
}
