#include "device_selector_widget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QWidget>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>

DeviceSelectorWidget::DeviceSelectorWidget(QWidget *parent)
    : QGroupBox(parent)
    , deviceCombo_(new QComboBox(this))
    , configCombo_(new QComboBox(this))
    , browseConfigBtn_(new QPushButton(tr("..."), this))
{
    setTitle(tr("驱动配置"));

        auto *grid = new QGridLayout(this);
        grid->setContentsMargins(10, 10, 10, 10);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(8);
        grid->setColumnStretch(1, 1);

        auto *deviceLabel = new QLabel(tr("采集卡:"), this);
        deviceLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        deviceCombo_->setMinimumContentsLength(20);
        deviceCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto *deviceRow = new QWidget(this);
        auto *deviceRowLayout = new QHBoxLayout(deviceRow);
        deviceRowLayout->setContentsMargins(0, 0, 0, 0);
        deviceRowLayout->setSpacing(6);

        connect(deviceCombo_, &QComboBox::currentIndexChanged,
            this, &DeviceSelectorWidget::handleDeviceIndexChanged, Qt::DirectConnection);

        deviceRowLayout->addWidget(deviceCombo_, 1);
        deviceRowLayout->setStretch(0, 1);

        grid->addWidget(deviceLabel, 0, 0);
        grid->addWidget(deviceRow, 0, 1);

        auto *configLabel = new QLabel(tr("配置文件:"), this);
        configLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        configCombo_->setEditable(false);
        configCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(configCombo_, &QComboBox::currentIndexChanged,
            this, &DeviceSelectorWidget::handleConfigIndexChanged, Qt::DirectConnection);

        // 配置文件选择行：浏览按钮 + 下拉框
        auto *configRow = new QWidget(this);
        auto *configRowLayout = new QHBoxLayout(configRow);
        configRowLayout->setContentsMargins(0, 0, 0, 0);
        configRowLayout->setSpacing(6);
        
        // Use font metrics for DPI-aware sizing
        int btnWidth = fontMetrics().horizontalAdvance(QStringLiteral("...")) + 16;
        browseConfigBtn_->setFixedWidth(qMax(btnWidth, 30));
        browseConfigBtn_->setToolTip(tr("Select configuration folder"));
        connect(browseConfigBtn_, &QPushButton::clicked,
            this, &DeviceSelectorWidget::handleBrowseConfigFolder, Qt::DirectConnection);
        
        configRowLayout->addWidget(browseConfigBtn_);
        configRowLayout->addWidget(configCombo_, 1);

        grid->addWidget(configLabel, 1, 0);
        grid->addWidget(configRow, 1, 1);
}

void DeviceSelectorWidget::setDevices(const QVector<DeviceInfo> &devices)
{
    devices_ = devices;
    deviceCombo_->clear();
    for (const auto &device : devices_) {
        const QString combined = QStringLiteral("%1    %2").arg(device.name, device.interfaceType);
        deviceCombo_->addItem(combined);
    }
    if (!devices_.isEmpty()) {
        deviceCombo_->setCurrentIndex(0);
    }
}

void DeviceSelectorWidget::setConfigurations(const QStringList &configurations, bool emitSignal)
{
    // Block signals to prevent unwanted configurationChanged emission
    bool oldState = configCombo_->blockSignals(true);
    
    configCombo_->clear();
    for (const QString &config : configurations) {
        // 显示文件名，但保存完整路径为itemData
        QFileInfo fileInfo(config);
        configCombo_->addItem(fileInfo.fileName(), config);
    }
    
    // 如果有配置文件，选择第一个
    if (configCombo_->count() > 0) {
        configCombo_->setCurrentIndex(0);
    }
    
    // Restore signal blocking state
    configCombo_->blockSignals(oldState);
    
    // Only emit signal if explicitly requested (e.g., from user interaction)
    if (emitSignal && configCombo_->count() > 0) {
        int currentIndex = configCombo_->currentIndex();
        QString configPath = configCombo_->itemData(currentIndex).toString();
        if (configPath.isEmpty()) {
            configPath = configCombo_->itemText(currentIndex);
        }
        emit configurationChanged(configPath);
    }
}

void DeviceSelectorWidget::clearDevices()
{
    devices_.clear();
    deviceCombo_->clear();
}

void DeviceSelectorWidget::clearConfigurations()
{
    configCombo_->clear();
}

void DeviceSelectorWidget::handleDeviceIndexChanged(int index)
{
    if (index < 0 || index >= devices_.size()) {
        return;
    }

    const auto info = devices_.at(index);
    emit deviceChanged(info);
}

void DeviceSelectorWidget::handleConfigIndexChanged(int index)
{
    if (index < 0 || index >= configCombo_->count()) {
        return;
    }

    // 使用itemData获取完整路径，如果没有itemData则使用itemText
    QString configPath = configCombo_->itemData(index).toString();
    if (configPath.isEmpty()) {
        configPath = configCombo_->itemText(index);
    }
    emit configurationChanged(configPath);
}

QString DeviceSelectorWidget::getCurrentDeviceName() const
{
    int index = deviceCombo_->currentIndex();
    if (index >= 0 && index < devices_.size()) {
        return devices_.at(index).name;
    }
    return QString();
}

QString DeviceSelectorWidget::getCurrentConfiguration() const
{
    int index = configCombo_->currentIndex();
    if (index >= 0) {
        QString configPath = configCombo_->itemData(index).toString();
        if (!configPath.isEmpty()) {
            return configPath;
        }
    }
    return configCombo_->currentText();
}

int DeviceSelectorWidget::getCurrentDeviceIndex() const
{
    return deviceCombo_->currentIndex();
}

void DeviceSelectorWidget::setConfigDirectory(const QString &directory)
{
    configDirectory_ = directory;
}

QString DeviceSelectorWidget::getConfigDirectory() const
{
    return configDirectory_;
}

void DeviceSelectorWidget::handleBrowseConfigFolder()
{
    QString startDir = configDirectory_;
    if (startDir.isEmpty()) {
        startDir = QDir::homePath();
    }
    
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Configuration Folder"),
        startDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!dir.isEmpty()) {
        configDirectory_ = dir;
        scanConfigurationFiles(dir);
        emit configDirectoryChanged(dir);
    }
}

void DeviceSelectorWidget::scanConfigurationFiles(const QString &directory)
{
    QDir dir(directory);
    if (!dir.exists()) {
        dir.mkpath(directory);
    }
    
    // 查找.ccf配置文件
    QStringList filters;
    filters << "*.ccf" << "*.CCF";
    dir.setNameFilters(filters);
    
    QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::Readable);
    
    QStringList configFiles;
    for (const QFileInfo &fileInfo : fileList) {
        configFiles.append(fileInfo.absoluteFilePath());
    }
    
    // 更新下拉框，并自动加载第一个配置（用户主动选择文件夹，所以发送信号）
    setConfigurations(configFiles, true);
}
