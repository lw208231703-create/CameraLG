#include "image_processing_panel.h"
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QSizePolicy>
#include <QSplitter>
#include <QSignalBlocker>
#include <QTimer>
#include <QtGlobal>
#include <QSettings>

// ======================== AlgorithmParameterPanel ========================

AlgorithmParameterPanel::AlgorithmParameterPanel(QWidget *parent)
    : QWidget(parent)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // Title
    m_titleLabel = new QLabel(tr("选择一个算法"), this);
    m_titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    m_mainLayout->addWidget(m_titleLabel);
    
    // Description
    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setStyleSheet("color: gray;");
    m_mainLayout->addWidget(m_descriptionLabel);
    
    // Separator
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_mainLayout->addWidget(line);
    
    // Parameters container (inside a scroll area)
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    m_parametersContainer = new QWidget(scrollArea);
    m_parametersLayout = new QVBoxLayout(m_parametersContainer);
    m_parametersLayout->setContentsMargins(0, 10, 0, 0);
    m_parametersLayout->setSpacing(8);
    m_parametersLayout->addStretch();
    
    scrollArea->setWidget(m_parametersContainer);
    m_mainLayout->addWidget(scrollArea, 1);
    
    setLayout(m_mainLayout);
}

void AlgorithmParameterPanel::setAlgorithm(const AlgorithmInfo &info)
{
    m_currentAlgorithm = info;
    clearParameters();
    
    if (info.id.isEmpty()) {
        m_titleLabel->setText(tr("选择一个算法"));
        m_descriptionLabel->clear();
        return;
    }
    
    m_titleLabel->setText(info.name);
    m_descriptionLabel->setText(info.description);
    
    // Create parameter widgets
    for (const AlgorithmParameter &param : info.parameters) {
        QWidget *paramWidget = createParameterWidget(param);
        if (paramWidget) {
            // Insert before the stretch
            m_parametersLayout->insertWidget(m_parametersLayout->count() - 1, paramWidget);
        }
    }
}

QVariantMap AlgorithmParameterPanel::getParameters() const
{
    QVariantMap params;
    
    for (auto it = m_parameterWidgets.begin(); it != m_parameterWidgets.end(); ++it) {
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

void AlgorithmParameterPanel::setParameters(const QVariantMap &params)
{
    for (auto it = m_parameterWidgets.begin(); it != m_parameterWidgets.end(); ++it) {
        const QString &name = it.key();
        QWidget *widget = it.value();

        if (!params.contains(name)) {
            continue;
        }

        const QVariant &value = params.value(name);

        if (QSpinBox *spin = qobject_cast<QSpinBox*>(widget)) {
            const QSignalBlocker blocker(spin);
            spin->setValue(value.toInt());
        } else if (QDoubleSpinBox *dspin = qobject_cast<QDoubleSpinBox*>(widget)) {
            const QSignalBlocker blocker(dspin);
            dspin->setValue(value.toDouble());
        } else if (QCheckBox *check = qobject_cast<QCheckBox*>(widget)) {
            const QSignalBlocker blocker(check);
            check->setChecked(value.toBool());
        } else if (QComboBox *combo = qobject_cast<QComboBox*>(widget)) {
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(value.toInt());
        }
    }
}

void AlgorithmParameterPanel::onParameterChanged()
{
    emit parametersChanged(getParameters());
}

void AlgorithmParameterPanel::clearParameters()
{
    // Remove all parameter widgets
    for (auto widget : m_parameterWidgets) {
        widget->deleteLater();
    }
    m_parameterWidgets.clear();
    
    // Also remove any labels and layouts
    while (m_parametersLayout->count() > 1) { // Keep the stretch
        QLayoutItem *item = m_parametersLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

QWidget* AlgorithmParameterPanel::createParameterWidget(const AlgorithmParameter &param)
{
    QWidget *container = new QWidget(m_parametersContainer);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    
    QLabel *label = new QLabel(param.displayName + ":", container);
    label->setMinimumWidth(80);
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
                this, &AlgorithmParameterPanel::onParameterChanged);
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
                this, &AlgorithmParameterPanel::onParameterChanged);
        inputWidget = dspin;
        
    } else if (param.type == "bool") {
        QCheckBox *check = new QCheckBox(container);
        check->setChecked(param.defaultValue.toBool());
        check->setToolTip(param.tooltip);
        connect(check, &QCheckBox::toggled,
                this, &AlgorithmParameterPanel::onParameterChanged);
        inputWidget = check;
        
    } else if (param.type == "enum") {
        QComboBox *combo = new QComboBox(container);
        combo->addItems(param.enumValues);
        combo->setCurrentIndex(param.defaultValue.toInt());
        combo->setToolTip(param.tooltip);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &AlgorithmParameterPanel::onParameterChanged);
        inputWidget = combo;
    }
    
    if (inputWidget) {
        inputWidget->setFixedWidth(120);
        layout->addWidget(inputWidget);
        layout->addStretch();
        m_parameterWidgets[param.name] = inputWidget;
    }
    
    container->setLayout(layout);
    return container;
}

// ======================== ImageProcessingPanel ========================

ImageProcessingPanel::ImageProcessingPanel(QWidget *parent)
    : QWidget(parent)
    , m_algorithmManager(nullptr)
{
    setupUI();
}

ImageProcessingPanel::~ImageProcessingPanel()
{
    if (m_splitter) {
        const QList<int> sizes = m_splitter->sizes();
        if (!sizes.isEmpty()) {
            QVariantList list;
            list.reserve(sizes.size());
            for (int s : sizes) list.push_back(s);
            QSettings settings("CameraUI", "MainWindow");
            settings.setValue("splitters/imageProcessingPanel", list);
        }
    }
}

void ImageProcessingPanel::setHighPrecisionMode(bool enabled)
{
    if (m_highPrecisionMode == enabled) {
        return;
    }
    m_highPrecisionMode = enabled;

    if (m_algorithmManager && m_highPrecisionMode) {
        // If an 8-bit-only algorithm is currently enabled, disable it so it doesn't
        // keep running while being hidden (and avoids confusion).
        const QStringList categories = m_algorithmManager->getCategories();
        for (const QString &category : categories) {
            const QVector<AlgorithmInfo> algorithms = m_algorithmManager->getAlgorithmsInCategory(category);
            for (const AlgorithmInfo &info : algorithms) {
                if (AlgorithmPrecisionUtils::shouldHideInHighPrecision(info.id) && m_algorithmManager->isAlgorithmEnabled(info.id)) {
                    m_algorithmManager->disableAlgorithm(info.id);
                }
            }
        }

        if (!m_currentAlgorithmId.isEmpty() && AlgorithmPrecisionUtils::shouldHideInHighPrecision(m_currentAlgorithmId)) {
            m_parameterPanel->setAlgorithm(AlgorithmInfo());
            m_currentAlgorithmId.clear();
        }
    }

    populateTree();
}

void ImageProcessingPanel::setAlgorithmManager(ImageAlgorithmManager *manager)
{
    m_algorithmManager = manager;
    
    if (m_algorithmManager) {
        // Register all algorithms first
        m_algorithmManager->registerAllAlgorithms();
        
        // Connect signals
        connect(m_algorithmManager, &ImageAlgorithmManager::algorithmEnabled,
                this, &ImageProcessingPanel::onAlgorithmEnabled);
        connect(m_algorithmManager, &ImageAlgorithmManager::algorithmDisabled,
                this, &ImageProcessingPanel::onAlgorithmDisabled);
        
        // Populate tree with algorithms
        populateTree();
    }
}

void ImageProcessingPanel::setupUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Make the 2nd/3rd columns resizable via a splitter.
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setOpaqueResize(true);
    m_splitter = splitter;
    
    // Left side: Tree widget for navigation
    m_treeWidget = new QTreeWidget(splitter);
    m_treeWidget->setHeaderLabel(tr("图像处理算法"));
    m_treeWidget->setIndentation(0); // 设置较小的缩进，使类型和算法更趋于左对齐
    // Ensure the tree is usable on narrow screens by setting a reasonable minimum width
    m_treeWidget->setMinimumWidth(160);
    m_treeWidget->setMaximumWidth(16777215);
    m_treeWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    connect(m_treeWidget, &QTreeWidget::itemClicked,
            this, &ImageProcessingPanel::onTreeItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemChanged,
            this, &ImageProcessingPanel::onTreeItemChanged);
    
    // Right side: Parameter panel
    m_parameterPanel = new AlgorithmParameterPanel(splitter);
    m_parameterPanel->setMinimumWidth(0);
    m_parameterPanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    connect(m_parameterPanel, &AlgorithmParameterPanel::parametersChanged,
            this, &ImageProcessingPanel::onParametersChanged);

    splitter->addWidget(m_treeWidget);
    splitter->addWidget(m_parameterPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    // Restore last splitter sizes if available; otherwise apply a reasonable default ratio.
    {
        QSettings settings("CameraUI", "MainWindow");
        const QVariant v = settings.value("splitters/imageProcessingPanel");
        const QVariantList list = v.toList();
        if (list.size() >= 2) {
            QList<int> sizes;
            sizes.reserve(list.size());
            for (const QVariant &it : list) sizes.push_back(it.toInt());
            splitter->setSizes(sizes);
        } else {
            QTimer::singleShot(0, this, [splitter]() {
                const int w = splitter->width();
                if (w <= 0) {
                    splitter->setSizes({220, 600});
                    return;
                }
                const int left = qBound(160, static_cast<int>(w * 0.28), 420);
                splitter->setSizes({left, qMax(1, w - left)});
            });
        }
    }

    m_mainLayout->addWidget(splitter);
    
    setLayout(m_mainLayout);
}

void ImageProcessingPanel::populateTree()
{
    m_treeWidget->blockSignals(true);

    m_treeWidget->clear();
    m_itemToAlgorithmId.clear();
    
    if (!m_algorithmManager) {
        m_treeWidget->blockSignals(false);
        return;
    }
    
    QStringList categories = m_algorithmManager->getCategories();

    for (const QString &category : categories) {
        const QVector<AlgorithmInfo> algorithms = m_algorithmManager->getAlgorithmsInCategory(category);

        QVector<AlgorithmInfo> visible;
        visible.reserve(algorithms.size());

        for (const AlgorithmInfo &info : algorithms) {
            if (m_highPrecisionMode && AlgorithmPrecisionUtils::shouldHideInHighPrecision(info.id)) {
                continue;
            }
            visible.push_back(info);
        }

        if (visible.isEmpty()) {
            continue;
        }

        QTreeWidgetItem *categoryItem = new QTreeWidgetItem(m_treeWidget);
        categoryItem->setText(0, category);
        categoryItem->setExpanded(true);

        QFont font = categoryItem->font(0);
        font.setBold(true);
        categoryItem->setFont(0, font);

        for (const AlgorithmInfo &info : visible) {
            QTreeWidgetItem *algorithmItem = new QTreeWidgetItem(categoryItem);
            algorithmItem->setText(0, info.name);
            algorithmItem->setToolTip(0, info.description);

            // Make item explicitly user-checkable/selectable
            algorithmItem->setFlags(algorithmItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);

            m_itemToAlgorithmId[algorithmItem] = info.id;

            // Update state if already enabled
            if (m_algorithmManager->isAlgorithmEnabled(info.id)) {
                algorithmItem->setCheckState(0, Qt::Checked);
            } else {
                algorithmItem->setCheckState(0, Qt::Unchecked);
            }
        }
    }

    m_treeWidget->blockSignals(false);

    // If there are algorithms but the tree pane is extremely small (e.g., restored from a different machine), expand it to a sensible default
    if (m_treeWidget->topLevelItemCount() > 0 && m_splitter) {
        QList<int> sizes = m_splitter->sizes();
        if (sizes.size() >= 2 && sizes[0] < 100) {
            int total = sizes[0] + sizes[1];
            int left = qBound(160, static_cast<int>(total * 0.28), 420);
            int right = qMax(1, total - left);
            m_splitter->setSizes({left, right});
        }
    }
}

void ImageProcessingPanel::onTreeItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    
    if (!item || !m_algorithmManager) {
        return;
    }
    
    // Check if this is an algorithm item (has an ID)
    auto it = m_itemToAlgorithmId.find(item);
    if (it == m_itemToAlgorithmId.end()) {
        // This is a category item
        m_parameterPanel->setAlgorithm(AlgorithmInfo());
        m_currentAlgorithmId.clear();
        return;
    }
    
    m_currentAlgorithmId = it.value();
    AlgorithmInfo info = m_algorithmManager->getAlgorithmInfo(m_currentAlgorithmId);
    m_parameterPanel->setAlgorithm(info);

    // Sync UI with last-used (cached) parameters so it matches the actual processing.
    const QVariantMap cached = m_algorithmManager->getCachedOrDefaultUiParameters(m_currentAlgorithmId);
    m_parameterPanel->setParameters(cached);
}

void ImageProcessingPanel::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (!item || column != 0 || !m_algorithmManager) {
        return;
    }
    
    auto it = m_itemToAlgorithmId.find(item);
    if (it == m_itemToAlgorithmId.end()) {
        return;
    }
    
    QString algorithmId = it.value();
    bool enabled = (item->checkState(0) == Qt::Checked);
    
    if (enabled) {
        // If enabling, we might need parameters. 
        // If it's the currently selected algorithm, get from panel.
        // Otherwise, use defaults (handled by manager).
        QVariantMap params;
        if (algorithmId == m_currentAlgorithmId) {
            params = m_parameterPanel->getParameters();
        }
        m_algorithmManager->enableAlgorithm(algorithmId, params);
    } else {
        m_algorithmManager->disableAlgorithm(algorithmId);
    }
}

void ImageProcessingPanel::onParametersChanged(const QVariantMap &params)
{
    if (m_currentAlgorithmId.isEmpty() || !m_algorithmManager) {
        return;
    }
    
    if (m_algorithmManager->isAlgorithmEnabled(m_currentAlgorithmId)) {
        m_algorithmManager->updateAlgorithmParameters(m_currentAlgorithmId, params);
    }
}

void ImageProcessingPanel::updateTreeItemState(const QString &algorithmId, bool enabled)
{
    m_treeWidget->blockSignals(true);
    for (auto it = m_itemToAlgorithmId.begin(); it != m_itemToAlgorithmId.end(); ++it) {
        if (it.value() == algorithmId) {
            it.key()->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
            break;
        }
    }
    m_treeWidget->blockSignals(false);
}

void ImageProcessingPanel::onAlgorithmEnabled(const QString &algorithmId)
{
    updateTreeItemState(algorithmId, true);
}

void ImageProcessingPanel::onAlgorithmDisabled(const QString &algorithmId)
{
    updateTreeItemState(algorithmId, false);
}
