#include "gige_dock.h"
#include "gige_camera_device.h"
#include "image_depth_converter.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QImage>
#include <QPixmap>
#include <QMessageBox>
#include <QEvent>
#include <QWheelEvent>
#include <QLineEdit>
#include <opencv2/opencv.hpp>

// 滚轮事件过滤器：拦截滚轮事件，防止参数自动变化
class WheelBlocker : public QObject {
public:
    WheelBlocker(QObject* parent) : QObject(parent) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::Wheel) {
            return true; // 拦截滚轮事件，不让控件处理
        }
        return QObject::eventFilter(obj, event);
    }
};

GigeDock::GigeDock(GigECameraDevice* cameraDevice, QWidget *parent)
    : QDockWidget(tr("GigE"), parent)
    , contentWidget_(new QWidget(this))
    , m_cameraManager(cameraDevice->cameraManager())
    , m_imageGrabber(cameraDevice->imageGrabber())
    , m_gigeCameraDevice(cameraDevice)
{
    setObjectName(QStringLiteral("gigeDock"));

    QVBoxLayout *mainLayout = new QVBoxLayout(contentWidget_);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    if (!cameraDevice->initializeSDK()) {
        QMessageBox::critical(this, "错误", "SDK初始化失败");
        return;
    }

    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(5);

    m_enumButton = new QPushButton("枚举设备", this);
    m_connectButton = new QPushButton("连接", this);
    m_disconnectButton = new QPushButton("断开", this);
    m_loadParamsButton = new QPushButton("加载参数", this);
    m_startGrabButton = new QPushButton("开始采集", this);
    m_stopGrabButton = new QPushButton("停止采集", this);
    m_autoReconnectCheckBox = new QCheckBox("自动重连", this);
    m_autoReconnectCheckBox->setChecked(false);

    m_deviceComboBox = new QComboBox(this);
    m_deviceComboBox->setEnabled(false);
    m_deviceComboBox->installEventFilter(new WheelBlocker(m_deviceComboBox));
    m_deviceComboBox->setFocusPolicy(Qt::StrongFocus);

    controlLayout->addWidget(m_enumButton);
    controlLayout->addWidget(m_deviceComboBox);
    controlLayout->addWidget(m_connectButton);
    controlLayout->addWidget(m_disconnectButton);
    controlLayout->addWidget(m_loadParamsButton);
    controlLayout->addWidget(m_startGrabButton);
    controlLayout->addWidget(m_stopGrabButton);
    controlLayout->addWidget(m_autoReconnectCheckBox);

    mainLayout->addLayout(controlLayout);

    QHBoxLayout *infoSplitLayout = new QHBoxLayout();
    infoSplitLayout->setSpacing(10);

    QGroupBox *deviceInfoGroup = new QGroupBox("设备信息", this);
    QVBoxLayout *deviceInfoLayout = new QVBoxLayout(deviceInfoGroup);

    m_deviceInfoPlaceholderLabel = new QLabel(tr("未连接设备"), this);
    m_deviceInfoPlaceholderLabel->setStyleSheet("color: gray; font-size: 11px;");
    deviceInfoLayout->addWidget(m_deviceInfoPlaceholderLabel);

    m_manufacturerLabel = new QLabel("制造商: --", this);
    m_modelLabel = new QLabel("型号: --", this);
    m_serialLabel = new QLabel("序列号: --", this);
    m_ipLabel = new QLabel("IP地址: --", this);
    m_versionLabel = new QLabel("版本: --", this);
    m_userDefinedNameLabel = new QLabel("用户定义名称: --", this);
    m_macAddressLabel = new QLabel("MAC地址: --", this);
    m_transportLayerTypeLabel = new QLabel("传输层类型: --", this);
    m_productLineLabel = new QLabel("产品线: --", this);
    m_productTypeLabel = new QLabel("产品类型: --", this);
    m_productSubtypeLabel = new QLabel("产品子类型: --", this);
    m_subnetMaskLabel = new QLabel("子网掩码: --", this);
    m_gatewayLabel = new QLabel("网关: --", this);
    m_ipCfgOptionLabel = new QLabel("IP配置选项: --", this);
    m_ipCfgCurrentLabel = new QLabel("当前IP配置: --", this);
    m_manufacturerSpecificInfoLabel = new QLabel("制造商特定信息: --", this);
    m_netExportLabel = new QLabel("网络导出: --", this);
    m_vendorIdLabel = new QLabel("供应商ID: --", this);
    m_productIdLabel = new QLabel("产品ID: --", this);
    m_deviceNumberLabel = new QLabel("设备编号: --", this);
    m_deviceGuidLabel = new QLabel("设备GUID: --", this);
    m_familyNameLabel = new QLabel("家族名称: --", this);
    m_usbProtocolLabel = new QLabel("USB协议: --", this);
    m_deviceAddressLabel = new QLabel("设备地址: --", this);
    m_ctrlInEndPointLabel = new QLabel("控制输入端点: --", this);
    m_ctrlOutEndPointLabel = new QLabel("控制输出端点: --", this);
    m_streamEndPointLabel = new QLabel("流端点: --", this);
    m_eventEndPointLabel = new QLabel("事件端点: --", this);
    m_temperatureLabel = new QLabel("设备温度: --", this);

    deviceInfoLayout->addWidget(m_manufacturerLabel);
    deviceInfoLayout->addWidget(m_modelLabel);
    deviceInfoLayout->addWidget(m_serialLabel);
    deviceInfoLayout->addWidget(m_ipLabel);
    deviceInfoLayout->addWidget(m_versionLabel);
    deviceInfoLayout->addWidget(m_userDefinedNameLabel);
    deviceInfoLayout->addWidget(m_macAddressLabel);
    deviceInfoLayout->addWidget(m_transportLayerTypeLabel);
    deviceInfoLayout->addWidget(m_productLineLabel);
    deviceInfoLayout->addWidget(m_productTypeLabel);
    deviceInfoLayout->addWidget(m_productSubtypeLabel);
    deviceInfoLayout->addWidget(m_subnetMaskLabel);
    deviceInfoLayout->addWidget(m_gatewayLabel);
    deviceInfoLayout->addWidget(m_ipCfgOptionLabel);
    deviceInfoLayout->addWidget(m_ipCfgCurrentLabel);
    deviceInfoLayout->addWidget(m_manufacturerSpecificInfoLabel);
    deviceInfoLayout->addWidget(m_netExportLabel);
    deviceInfoLayout->addWidget(m_vendorIdLabel);
    deviceInfoLayout->addWidget(m_productIdLabel);
    deviceInfoLayout->addWidget(m_deviceNumberLabel);
    deviceInfoLayout->addWidget(m_deviceGuidLabel);
    deviceInfoLayout->addWidget(m_familyNameLabel);
    deviceInfoLayout->addWidget(m_usbProtocolLabel);
    deviceInfoLayout->addWidget(m_deviceAddressLabel);
    deviceInfoLayout->addWidget(m_ctrlInEndPointLabel);
    deviceInfoLayout->addWidget(m_ctrlOutEndPointLabel);
    deviceInfoLayout->addWidget(m_streamEndPointLabel);
    deviceInfoLayout->addWidget(m_eventEndPointLabel);
    deviceInfoLayout->addWidget(m_temperatureLabel);

    hideAllDeviceInfoLabels();

    infoSplitLayout->addWidget(deviceInfoGroup, 1);

    m_registerGroup = new QGroupBox("寄存器读写", this);
    QVBoxLayout *registerLayout = new QVBoxLayout(m_registerGroup);

    m_registerPlaceholderLabel = new QLabel(tr("未连接设备"), this);
    m_registerPlaceholderLabel->setStyleSheet("color: gray; font-size: 11px;");
    m_registerPlaceholderLabel->setAlignment(Qt::AlignCenter);
    registerLayout->addWidget(m_registerPlaceholderLabel);

    m_registerAddressEdit = new QLineEdit(this);
    m_registerAddressEdit->setPlaceholderText("寄存器地址(16进制)");
    registerLayout->addWidget(m_registerAddressEdit);

    m_registerValueEdit = new QLineEdit(this);
    m_registerValueEdit->setPlaceholderText("寄存器值(16进制)");
    registerLayout->addWidget(m_registerValueEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_readRegisterButton = new QPushButton("读寄存器", this);
    m_writeRegisterButton = new QPushButton("写寄存器", this);
    m_readRegisterButton->setEnabled(false);
    m_writeRegisterButton->setEnabled(false);
    buttonLayout->addWidget(m_readRegisterButton);
    buttonLayout->addWidget(m_writeRegisterButton);
    registerLayout->addLayout(buttonLayout);

    m_registerResultLabel = new QLabel("结果: --", this);
    m_registerResultLabel->setWordWrap(true);
    registerLayout->addWidget(m_registerResultLabel);

    hideRegisterControls();

    infoSplitLayout->addWidget(m_registerGroup, 1);

    mainLayout->addLayout(infoSplitLayout);



    m_paramsScrollArea = new QScrollArea(this);
    m_paramsScrollArea->setWidgetResizable(true);
    m_paramsWidget = new QWidget();
    QVBoxLayout *paramsLayout = new QVBoxLayout(m_paramsWidget);
    paramsLayout->setContentsMargins(2, 2, 2, 2);
    paramsLayout->setSpacing(2);
    m_paramsScrollArea->setWidget(m_paramsWidget);

    mainLayout->addWidget(m_paramsScrollArea);

    contentWidget_->setMinimumSize(200, 200);
    setWidget(contentWidget_);
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_connectButton->setEnabled(false);
    m_disconnectButton->setEnabled(false);
    m_loadParamsButton->setEnabled(false);
    m_startGrabButton->setEnabled(false);
    m_stopGrabButton->setEnabled(false);

    initializeConnections();

}

GigeDock::~GigeDock()
{
    if (m_imageGrabber != nullptr) {
        m_imageGrabber->stopGrabbing();
        delete m_imageGrabber;
        m_imageGrabber = nullptr;
    }

    clearParameterWidgets();

    for (CameraParameter* param : m_parameters) {
        delete param;
    }
    m_parameters.clear();

    if (m_cameraManager != nullptr) {
        m_cameraManager->finalize();
    }
}

void GigeDock::initializeConnections()
{
    connect(m_enumButton, &QPushButton::clicked, this, &GigeDock::onEnumButtonClicked);
    connect(m_connectButton, &QPushButton::clicked, this, &GigeDock::onConnectButtonClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &GigeDock::onDisconnectButtonClicked);
    connect(m_loadParamsButton, &QPushButton::clicked, this, &GigeDock::onLoadParamsButtonClicked);
    connect(m_startGrabButton, &QPushButton::clicked, this, &GigeDock::onStartGrabButtonClicked);
    connect(m_stopGrabButton, &QPushButton::clicked, this, &GigeDock::onStopGrabButtonClicked);
    connect(m_autoReconnectCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
        m_cameraManager->setAutoReconnect(state == Qt::Checked);
    });

    connect(m_cameraManager, &CameraManager::deviceConnected, this, &GigeDock::onDeviceConnected);
    connect(m_cameraManager, &CameraManager::deviceDisconnected, this, &GigeDock::onDeviceDisconnected);
    connect(m_cameraManager, &CameraManager::deviceReconnected, this, &GigeDock::onDeviceReconnected);
    connect(m_cameraManager, &CameraManager::errorOccurred, this, &GigeDock::onErrorOccurred);

    // Frame handling moved to GigECameraDevice; GigeDock only manages UI
    // connect(m_imageGrabber, &ImageGrabber::frameReceived, this, &GigeDock::onFrameReceived);

    connect(m_readRegisterButton, &QPushButton::clicked, this, &GigeDock::onReadRegisterClicked);
    connect(m_writeRegisterButton, &QPushButton::clicked, this, &GigeDock::onWriteRegisterClicked);
}

void GigeDock::onEnumButtonClicked()
{
    m_deviceList = m_cameraManager->enumDevices(MV_GIGE_DEVICE);

    if (m_deviceList.isEmpty()) {
        QMessageBox::information(this, "信息", "未发现GigE设备");
        m_deviceComboBox->setEnabled(false);
        m_connectButton->setEnabled(false);
        return;
    }

    updateDeviceList(m_deviceList);
    m_deviceComboBox->setEnabled(true);
    m_connectButton->setEnabled(true);
}

void GigeDock::onConnectButtonClicked()
{
    int currentIndex = m_deviceComboBox->currentIndex();
    if (currentIndex < 0 || currentIndex >= m_deviceList.size()) {
        QMessageBox::warning(this, "警告", "请选择一个设备");
        return;
    }

    MV_CC_DEVICE_INFO* pDeviceInfo = m_deviceList[currentIndex];
    if (pDeviceInfo == nullptr) {
        QMessageBox::warning(this, "警告", "设备信息无效");
        return;
    }

    if (!m_cameraManager->connectDevice(pDeviceInfo)) {
        QMessageBox::critical(this, "错误", "连接设备失败: " + m_cameraManager->getLastError());
        return;
    }

    MV_CC_DEVICE_INFO info = m_cameraManager->getDeviceInfo();
    updateDeviceInfo(info);
}

void GigeDock::onDisconnectButtonClicked()
{
    m_cameraManager->disconnectDevice();
    clearParameterWidgets();
}

void GigeDock::onLoadParamsButtonClicked()
{
    if (!m_cameraManager->isConnected()) {
        QMessageBox::warning(this, "警告", "请先连接设备");
        return;
    }

    loadCameraParameters();
}

void GigeDock::onStartGrabButtonClicked()
{
    if (!m_cameraManager->isConnected()) {
        QMessageBox::warning(this, "警告", "请先连接设备");
        return;
    }

    void* handle = m_cameraManager->getHandle();
    if (m_imageGrabber->startGrabbing(handle)) {
        m_startGrabButton->setEnabled(false);
        m_stopGrabButton->setEnabled(true);
    } else {
        QMessageBox::critical(this, "错误", "启动采集失败");
    }
}

void GigeDock::onStopGrabButtonClicked()
{
    if (m_imageGrabber != nullptr && m_imageGrabber->isGrabbing()) {
        m_imageGrabber->stopGrabbing();
    }

    m_startGrabButton->setEnabled(true);
    m_stopGrabButton->setEnabled(false);

    // m_imageLabel->clear();
    // m_imageLabel->setText("等待采集图像...");
    // m_imageInfoLabel->setText("图像信息: --");
}

void GigeDock::onDeviceConnected()
{
    m_isDeviceConnected = true;
    m_connectButton->setEnabled(false);
    m_disconnectButton->setEnabled(true);
    m_loadParamsButton->setEnabled(true);
    m_deviceComboBox->setEnabled(false);
    m_enumButton->setEnabled(false);
    m_startGrabButton->setEnabled(true);
    m_stopGrabButton->setEnabled(false);
    m_readRegisterButton->setEnabled(true);
    m_writeRegisterButton->setEnabled(true);

    showRegisterControls();
    updateDeviceInfoPlaceholder();
}

void GigeDock::onDeviceDisconnected()
{
    m_isDeviceConnected = false;
    m_connectButton->setEnabled(true);
    m_disconnectButton->setEnabled(false);
    m_loadParamsButton->setEnabled(false);
    m_deviceComboBox->setEnabled(true);
    m_enumButton->setEnabled(true);
    m_startGrabButton->setEnabled(false);
    m_stopGrabButton->setEnabled(false);
    m_readRegisterButton->setEnabled(false);
    m_writeRegisterButton->setEnabled(false);

    hideRegisterControls();
    hideAllDeviceInfoLabels();

    // m_imageLabel->clear();
    // m_imageLabel->setText("等待采集图像...");
    // m_imageInfoLabel->setText("图像信息: --");
}

void GigeDock::onErrorOccurred(const QString& error)
{
    QMessageBox::critical(this, "错误", error);
}

void GigeDock::onDeviceReconnected()
{
    m_isDeviceConnected = true;
    m_connectButton->setEnabled(false);
    m_disconnectButton->setEnabled(true);
    m_loadParamsButton->setEnabled(true);
    m_deviceComboBox->setEnabled(false);
    m_enumButton->setEnabled(false);
    m_startGrabButton->setEnabled(true);
    m_stopGrabButton->setEnabled(false);
    m_readRegisterButton->setEnabled(true);
    m_writeRegisterButton->setEnabled(true);

    showRegisterControls();

    void* handle = m_cameraManager->getHandle();
    m_imageGrabber->updateHandle(handle);

    MV_CC_DEVICE_INFO deviceInfo = m_cameraManager->getDeviceInfo();
    updateDeviceInfo(deviceInfo);

    QMessageBox::information(this, "重连成功", "相机已重新连接");
}

void GigeDock::onParameterValueChanged(const QString& name, const QVariant& value)
{
    CameraParameter* parameter = nullptr;
    for (CameraParameter* param : m_parameters) {
        if (param->getName() == name) {
            parameter = param;
            break;
        }
    }

    if (parameter == nullptr) {
        return;
    }

    if (!value.isValid()) {
        readParameter(parameter);
    } else {
        writeParameter(parameter, value);
    }
}

void GigeDock::onFrameReceived(const ImageData& /*image*/)
{
    // Frame handling is now done by GigECameraDevice.
    // GigeDock is only responsible for device management UI.
}


void GigeDock::updateDeviceList(const QList<MV_CC_DEVICE_INFO*>& devices)
{
    m_deviceComboBox->clear();

    for (const MV_CC_DEVICE_INFO* pInfo : devices) {
        if (pInfo == nullptr) {
            continue;
        }

        QString modelName;
        QString serialNumber;

        if (pInfo->nTLayerType == MV_GIGE_DEVICE) {
            const MV_GIGE_DEVICE_INFO* pGigEInfo = &pInfo->SpecialInfo.stGigEInfo;
            modelName = CameraManager::convertToQString(pGigEInfo->chModelName, 32);
            serialNumber = CameraManager::convertToQString(pGigEInfo->chSerialNumber, 16);
        } else if (pInfo->nTLayerType == MV_USB_DEVICE) {
            const MV_USB3_DEVICE_INFO* pUsbInfo = &pInfo->SpecialInfo.stUsb3VInfo;
            modelName = CameraManager::convertToQString(pUsbInfo->chModelName, 64);
            serialNumber = CameraManager::convertToQString(pUsbInfo->chSerialNumber, 64);
        } else {
            modelName = "Unknown Device";
            serialNumber = "N/A";
        }

        QString displayText = QString("%1 (%2)")
            .arg(modelName)
            .arg(serialNumber);
        m_deviceComboBox->addItem(displayText);
    }
}

void GigeDock::setLabelValue(QLabel *label, const QString &prefix, const QString &value, bool visible)
{
    if (!label) {
        return;
    }
    if (visible) {
        label->setText(prefix + value);
        label->setVisible(true);
    } else {
        label->setVisible(false);
    }
}

void GigeDock::setLabelValue(QLabel *label, const QString &prefix, const QString &value)
{
    const QString trimmed = value.trimmed();
    const bool visible = !trimmed.isEmpty() && trimmed != "--";
    setLabelValue(label, prefix, trimmed, visible);
}

void GigeDock::hideAllDeviceInfoLabels()
{
    const QList<QLabel*> labels = {
        m_manufacturerLabel, m_modelLabel, m_serialLabel, m_ipLabel, m_versionLabel,
        m_userDefinedNameLabel, m_macAddressLabel, m_transportLayerTypeLabel,
        m_productLineLabel, m_productTypeLabel, m_productSubtypeLabel,
        m_subnetMaskLabel, m_gatewayLabel, m_ipCfgOptionLabel, m_ipCfgCurrentLabel,
        m_manufacturerSpecificInfoLabel, m_netExportLabel,
        m_vendorIdLabel, m_productIdLabel, m_deviceNumberLabel, m_deviceGuidLabel,
        m_familyNameLabel, m_usbProtocolLabel, m_deviceAddressLabel,
        m_ctrlInEndPointLabel, m_ctrlOutEndPointLabel, m_streamEndPointLabel, m_eventEndPointLabel,
        m_temperatureLabel
    };

    for (QLabel *label : labels) {
        if (label) {
            label->setVisible(false);
        }
    }

    updateDeviceInfoPlaceholder();
}

void GigeDock::hideRegisterControls()
{
    if (m_registerAddressEdit) {
        m_registerAddressEdit->setVisible(false);
    }
    if (m_registerValueEdit) {
        m_registerValueEdit->setVisible(false);
    }
    if (m_readRegisterButton) {
        m_readRegisterButton->setVisible(false);
    }
    if (m_writeRegisterButton) {
        m_writeRegisterButton->setVisible(false);
    }
    if (m_registerResultLabel) {
        m_registerResultLabel->setVisible(false);
    }
    if (m_registerPlaceholderLabel) {
        m_registerPlaceholderLabel->setVisible(true);
    }
}

void GigeDock::showRegisterControls()
{
    if (m_registerPlaceholderLabel) {
        m_registerPlaceholderLabel->setVisible(false);
    }
    if (m_registerAddressEdit) {
        m_registerAddressEdit->setVisible(true);
    }
    if (m_registerValueEdit) {
        m_registerValueEdit->setVisible(true);
    }
    if (m_readRegisterButton) {
        m_readRegisterButton->setVisible(true);
    }
    if (m_writeRegisterButton) {
        m_writeRegisterButton->setVisible(true);
    }
    if (m_registerResultLabel) {
        m_registerResultLabel->setVisible(true);
    }
}

void GigeDock::updateDeviceInfoPlaceholder()
{
    if (!m_deviceInfoPlaceholderLabel) {
        return;
    }

    const QList<QLabel*> labels = {
        m_manufacturerLabel, m_modelLabel, m_serialLabel, m_ipLabel, m_versionLabel,
        m_userDefinedNameLabel, m_macAddressLabel, m_transportLayerTypeLabel,
        m_productLineLabel, m_productTypeLabel, m_productSubtypeLabel,
        m_subnetMaskLabel, m_gatewayLabel, m_ipCfgOptionLabel, m_ipCfgCurrentLabel,
        m_manufacturerSpecificInfoLabel, m_netExportLabel,
        m_vendorIdLabel, m_productIdLabel, m_deviceNumberLabel, m_deviceGuidLabel,
        m_familyNameLabel, m_usbProtocolLabel, m_deviceAddressLabel,
        m_ctrlInEndPointLabel, m_ctrlOutEndPointLabel, m_streamEndPointLabel, m_eventEndPointLabel,
        m_temperatureLabel
    };

    bool anyVisible = false;
    for (QLabel *label : labels) {
        if (label && label->isVisible()) {
            anyVisible = true;
            break;
        }
    }

    if (!m_isDeviceConnected) {
        m_deviceInfoPlaceholderLabel->setText(tr("未连接设备"));
        m_deviceInfoPlaceholderLabel->setVisible(true);
        return;
    }

    m_deviceInfoPlaceholderLabel->setText(tr("暂无可用信息"));
    m_deviceInfoPlaceholderLabel->setVisible(!anyVisible);
}

void GigeDock::updateDeviceInfo(const MV_CC_DEVICE_INFO& info)
{
    hideAllDeviceInfoLabels();

    const auto isUsefulIp = [](const QString &ip) {
        const QString trimmed = ip.trimmed();
        return !trimmed.isEmpty() && trimmed != "--" && trimmed != "0.0.0.0";
    };

    const QString mac = CameraManager::formatMacAddress(info);
    setLabelValue(m_macAddressLabel, "MAC地址: ", mac, !mac.trimmed().isEmpty() && mac != "--");
    setLabelValue(m_transportLayerTypeLabel, "传输层类型: ", CameraManager::formatTransportLayerType(info.nTLayerType));
    setLabelValue(m_productLineLabel, "产品线: ", QString::number((info.nDevTypeInfo >> 24) & 0xFF));
    setLabelValue(m_productTypeLabel, "产品类型: ", QString::number((info.nDevTypeInfo >> 16) & 0xFF));
    setLabelValue(m_productSubtypeLabel, "产品子类型: ", QString::number((info.nDevTypeInfo >> 8) & 0xFF));

    if (info.nTLayerType == MV_GIGE_DEVICE) {
        const MV_GIGE_DEVICE_INFO* pGigEInfo = &info.SpecialInfo.stGigEInfo;
        setLabelValue(m_manufacturerLabel, "制造商: ", CameraManager::convertToQString(pGigEInfo->chManufacturerName, 32));
        setLabelValue(m_modelLabel, "型号: ", CameraManager::convertToQString(pGigEInfo->chModelName, 32));
        setLabelValue(m_serialLabel, "序列号: ", CameraManager::convertToQString(pGigEInfo->chSerialNumber, 16));
        const QString ip = CameraManager::ipToString(pGigEInfo->nCurrentIp);
        setLabelValue(m_ipLabel, "IP地址: ", ip, isUsefulIp(ip));
        setLabelValue(m_versionLabel, "版本: ", CameraManager::convertToQString(pGigEInfo->chDeviceVersion, 32));
        setLabelValue(m_userDefinedNameLabel, "用户定义名称: ", CameraManager::convertToQString(pGigEInfo->chUserDefinedName, 16));
        const QString subnet = CameraManager::ipToString(pGigEInfo->nCurrentSubNetMask);
        setLabelValue(m_subnetMaskLabel, "子网掩码: ", subnet, isUsefulIp(subnet));
        const QString gateway = CameraManager::ipToString(pGigEInfo->nDefultGateWay);
        setLabelValue(m_gatewayLabel, "网关: ", gateway, isUsefulIp(gateway));
        setLabelValue(m_ipCfgOptionLabel, "IP配置选项: ", QString::number(pGigEInfo->nIpCfgOption));
        setLabelValue(m_ipCfgCurrentLabel, "当前IP配置: ", QString::number(pGigEInfo->nIpCfgCurrent));
        setLabelValue(m_manufacturerSpecificInfoLabel, "制造商特定信息: ", CameraManager::convertToQString(pGigEInfo->chManufacturerSpecificInfo, 48));
        const QString netExport = CameraManager::ipToString(pGigEInfo->nNetExport);
        setLabelValue(m_netExportLabel, "网络导出: ", netExport, isUsefulIp(netExport));
    } else if (info.nTLayerType == MV_USB_DEVICE) {
        const MV_USB3_DEVICE_INFO* pUsbInfo = &info.SpecialInfo.stUsb3VInfo;
        setLabelValue(m_manufacturerLabel, "制造商: ", CameraManager::convertToQString(pUsbInfo->chManufacturerName, 64));
        setLabelValue(m_modelLabel, "型号: ", CameraManager::convertToQString(pUsbInfo->chModelName, 64));
        setLabelValue(m_serialLabel, "序列号: ", CameraManager::convertToQString(pUsbInfo->chSerialNumber, 64));
        setLabelValue(m_versionLabel, "版本: ", CameraManager::convertToQString(pUsbInfo->chDeviceVersion, 64));
        setLabelValue(m_userDefinedNameLabel, "用户定义名称: ", CameraManager::convertToQString(pUsbInfo->chUserDefinedName, 64));
        setLabelValue(m_vendorIdLabel, "供应商ID: ", QString("0x%1").arg(pUsbInfo->idVendor, 4, 16, QChar('0')));
        setLabelValue(m_productIdLabel, "产品ID: ", QString("0x%1").arg(pUsbInfo->idProduct, 4, 16, QChar('0')));
        setLabelValue(m_deviceNumberLabel, "设备编号: ", QString::number(pUsbInfo->nDeviceNumber));
        setLabelValue(m_deviceGuidLabel, "设备GUID: ", CameraManager::convertToQString(pUsbInfo->chDeviceGUID, 64));
        setLabelValue(m_familyNameLabel, "家族名称: ", CameraManager::convertToQString(pUsbInfo->chFamilyName, 64));
        setLabelValue(m_usbProtocolLabel, "USB协议: ", QString("0x%1").arg(pUsbInfo->nbcdUSB, 8, 16, QChar('0')));
        setLabelValue(m_deviceAddressLabel, "设备地址: ", QString::number(pUsbInfo->nDeviceAddress));
        setLabelValue(m_ctrlInEndPointLabel, "控制输入端点: ", QString::number(pUsbInfo->CrtlInEndPoint));
        setLabelValue(m_ctrlOutEndPointLabel, "控制输出端点: ", QString::number(pUsbInfo->CrtlOutEndPoint));
        setLabelValue(m_streamEndPointLabel, "流端点: ", QString::number(pUsbInfo->StreamEndPoint));
        setLabelValue(m_eventEndPointLabel, "事件端点: ", QString::number(pUsbInfo->EventEndPoint));
    }

    if (m_cameraManager && m_cameraManager->isConnected()) {
        float temperature = 0.0f;
        if (m_cameraManager->getFloatValue("DeviceTemperature", temperature)) {
            setLabelValue(m_temperatureLabel, "设备温度: ", QString::number(temperature, 'f', 1) + "°C");
        }
    }

    updateDeviceInfoPlaceholder();
}

void GigeDock::clearParameterWidgets()
{
    for (ParameterWidget* widget : m_parameterWidgets) {
        delete widget;
    }
    m_parameterWidgets.clear();

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_paramsWidget->layout());
    if (layout != nullptr) {
        while (layout->count() > 0) {
            QLayoutItem* item = layout->takeAt(0);
            if (item->widget() != nullptr) {
                delete item->widget();
            }
            delete item;
        }
    }
}

void GigeDock::loadCameraParameters()
{
    clearParameterWidgets();

    QList<ParameterInfo> supportedParams = m_cameraManager->getSupportedParameters();

    for (const ParameterInfo& info : supportedParams) {
        CameraParameter* parameter = new CameraParameter(this);
        parameter->setName(info.name);
        parameter->setDisplayName(info.displayName);
        parameter->setType(info.type);
        parameter->setValue(info.value);
        parameter->setMinValue(info.minValue);
        parameter->setMaxValue(info.maxValue);
        parameter->setIncValue(info.incValue);
        parameter->setEnumEntries(info.enumEntries);
        parameter->setEnumValues(info.enumValues);
        parameter->setReadable(info.readable);
        parameter->setWritable(info.writable);
        parameter->setCategory(info.category);
        parameter->setDescription(info.description);

        m_parameters.append(parameter);
        createParameterWidget(parameter);
    }

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_paramsWidget->layout());
    if (layout != nullptr) {
        layout->addStretch();
    }
}

void GigeDock::createParameterWidget(CameraParameter* parameter)
{
    ParameterWidget* widget = new ParameterWidget(parameter, m_paramsWidget);
    connect(widget, &ParameterWidget::valueChanged, this, &GigeDock::onParameterValueChanged);

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_paramsWidget->layout());
    if (layout != nullptr) {
        layout->addWidget(widget);
    }

    m_parameterWidgets.append(widget);
}

void GigeDock::readParameter(CameraParameter* parameter)
{
    QString name = parameter->getName();
    QVariant value;

    switch (parameter->getType()) {
        case ParameterType::Int: {
            int64_t intValue;
            if (m_cameraManager->getIntValue(name, intValue)) {
                value = intValue;
            }
            break;
        }
        case ParameterType::Float: {
            float floatValue;
            if (m_cameraManager->getFloatValue(name, floatValue)) {
                value = floatValue;
            }
            break;
        }
        case ParameterType::Enum: {
            unsigned int enumValue;
            if (m_cameraManager->getEnumValue(name, enumValue)) {
                value = enumValue;
            }
            break;
        }
        case ParameterType::Bool: {
            bool boolValue;
            if (m_cameraManager->getBoolValue(name, boolValue)) {
                value = boolValue;
            }
            break;
        }
        case ParameterType::String: {
            QString stringValue;
            if (m_cameraManager->getStringValue(name, stringValue)) {
                value = stringValue;
            }
            break;
        }
        default:
            break;
    }

    if (value.isValid()) {
        parameter->setValue(value);

        for (ParameterWidget* widget : m_parameterWidgets) {
            CameraParameter* param = widget->findChild<CameraParameter*>();
            if (param != nullptr && param->getName() == name) {
                widget->updateValue(value);
                break;
            }
        }
    }
}

void GigeDock::writeParameter(CameraParameter* parameter, const QVariant& value)
{
    QString name = parameter->getName();
    bool success = false;

    switch (parameter->getType()) {
        case ParameterType::Int:
            success = m_cameraManager->setIntValue(name, value.toLongLong());
            break;
        case ParameterType::Float:
            success = m_cameraManager->setFloatValue(name, value.toFloat());
            break;
        case ParameterType::Enum:
            success = m_cameraManager->setEnumValue(name, value.toUInt());
            break;
        case ParameterType::Bool:
            success = m_cameraManager->setBoolValue(name, value.toBool());
            break;
        case ParameterType::String:
            success = m_cameraManager->setStringValue(name, value.toString());
            break;
        default:
            break;
    }

    if (success) {
        parameter->setValue(value);
    }
}

void GigeDock::displayImage(const ImageData& image)
{
    // 显示功能已被移除
    Q_UNUSED(image);
}

// getPixelFormatString is no longer used but can be kept as utility or removed. Keeping it for now.
QString GigeDock::getPixelFormatString(MvGvspPixelType pixelFormat)
{
    switch (pixelFormat) {
        case PixelType_Gvsp_Mono8:
            return "Mono8";
        case PixelType_Gvsp_RGB8_Packed:
            return "RGB8";
        case PixelType_Gvsp_BayerRG8:
            return "BayerRG8";
        case PixelType_Gvsp_BayerGB8:
            return "BayerGB8";
        case PixelType_Gvsp_BayerGR8:
            return "BayerGR8";
        case PixelType_Gvsp_BayerBG8:
            return "BayerBG8";
        default:
            return QString("0x%1").arg((int)pixelFormat, 8, 16, QChar('0'));
    }
}

QImage GigeDock::convertToQImage(const ImageData& image)
{
    int width = image.frameInfo.nWidth;
    int height = image.frameInfo.nHeight;
    MvGvspPixelType pixelFormat = (MvGvspPixelType)image.frameInfo.enPixelType;
    
    const unsigned char* data = reinterpret_cast<const unsigned char*>(image.data.constData());
    
    QImage qImage;
    
    // Handle different pixel formats
    if (pixelFormat == PixelType_Gvsp_Mono8) {
        qImage = QImage(data, width, height, width, QImage::Format_Grayscale8).copy();
    } else if (pixelFormat == PixelType_Gvsp_Mono10 || 
               pixelFormat == PixelType_Gvsp_Mono12 || 
               pixelFormat == PixelType_Gvsp_Mono14 || 
               pixelFormat == PixelType_Gvsp_Mono16) {
        // 方案3：使用位断提取而不是线性缩放
        int bitDepth = getPixelBitDepth(pixelFormat);
        
        const uint16_t* srcData = reinterpret_cast<const uint16_t*>(data);
        int pitch = width * sizeof(uint16_t);
        
        // 使用ImageDepthConverter::bitExtract进行位断提取
        qImage = ImageDepthConverter::bitExtract(srcData, width, height, pitch, bitDepth, m_BitShift);
    } else if (pixelFormat == PixelType_Gvsp_RGB8_Packed) {
        qImage = QImage(data, width, height, width * 3, QImage::Format_RGB888).copy();
    } else {
        // Default: treat as grayscale 8-bit
        qImage = QImage(data, width, height, width, QImage::Format_Grayscale8).copy();
    }
    
    return qImage;
}

QVector<uint16_t> GigeDock::convertToRawData(const ImageData& image)
{
    int width = image.frameInfo.nWidth;
    int height = image.frameInfo.nHeight;
    int size = width * height;
    MvGvspPixelType pixelFormat = (MvGvspPixelType)image.frameInfo.enPixelType;
    
    QVector<uint16_t> rawData(size);
    
    const unsigned char* data = reinterpret_cast<const unsigned char*>(image.data.constData());
    
    // Convert based on pixel format
    if (pixelFormat == PixelType_Gvsp_Mono8) {
        // 8-bit to 16-bit conversion using OpenCV for performance
        cv::Mat src(height, width, CV_8UC1, (void*)data);
        cv::Mat dst(height, width, CV_16UC1, rawData.data());
        src.convertTo(dst, CV_16UC1);
    } else if (pixelFormat == PixelType_Gvsp_Mono10 || 
               pixelFormat == PixelType_Gvsp_Mono12 || 
               pixelFormat == PixelType_Gvsp_Mono14 || 
               pixelFormat == PixelType_Gvsp_Mono16) {
        // High bit-depth data - direct copy
        const uint16_t* data16 = reinterpret_cast<const uint16_t*>(data);
        std::copy(data16, data16 + size, rawData.begin());
    } else {
        // Default: convert 8-bit to 16-bit
        for (int i = 0; i < size; ++i) {
            rawData[i] = static_cast<uint16_t>(data[i]);
        }
    }
    
    return rawData;
}

int GigeDock::getPixelBitDepth(MvGvspPixelType pixelFormat)
{
    // Return bit depth based on pixel format
    switch (pixelFormat) {
        case PixelType_Gvsp_Mono8:
        case PixelType_Gvsp_BayerRG8:
        case PixelType_Gvsp_BayerGB8:
        case PixelType_Gvsp_BayerGR8:
        case PixelType_Gvsp_BayerBG8:
        case PixelType_Gvsp_RGB8_Packed:
            return 8;
        case PixelType_Gvsp_Mono10:
            return 10;
        case PixelType_Gvsp_Mono12:
            return 12;
        case PixelType_Gvsp_Mono14:
            return 14;
        case PixelType_Gvsp_Mono16:
            return 16;
        default:
            return 8;  // Default to 8-bit if unknown
    }
}

void GigeDock::setBitShift(int shift)
{
    if (shift < 0) shift = 0;
    if (shift > 8) shift = 8;
    m_BitShift = shift;
    
    // 同步到Image_Acquisition
    if (m_gigeCameraDevice) {
        m_gigeCameraDevice->setBitShift(m_BitShift);
    }
}

void GigeDock::onReadRegisterClicked()
{
    if (!m_cameraManager || !m_cameraManager->isConnected()) {
        QMessageBox::warning(this, "警告", "请先连接设备");
        return;
    }

    QString addressStr = m_registerAddressEdit->text().trimmed();
    if (addressStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入寄存器地址");
        return;
    }

    bool ok = false;
    int64_t address = addressStr.toLongLong(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "警告", "寄存器地址格式错误，请输入16进制地址");
        return;
    }

    uint32_t value = 0;
    if (m_cameraManager->readMemory(&value, address, 4)) {
        m_registerValueEdit->setText(QString("0x%1").arg(value, 8, 16, QChar('0')));
        m_registerResultLabel->setText(QString("读取成功: 地址 0x%1 = 0x%2 (%3)")
            .arg(address, 0, 16)
            .arg(value, 8, 16, QChar('0'))
            .arg(value));
        m_registerResultLabel->setStyleSheet("color: #00ff00;");
    } else {
        m_registerResultLabel->setText("读取失败: " + m_cameraManager->getLastError());
        m_registerResultLabel->setStyleSheet("color: #ff0000;");
    }
}

void GigeDock::onWriteRegisterClicked()
{
    if (!m_cameraManager || !m_cameraManager->isConnected()) {
        QMessageBox::warning(this, "警告", "请先连接设备");
        return;
    }

    QString addressStr = m_registerAddressEdit->text().trimmed();
    if (addressStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入寄存器地址");
        return;
    }

    QString valueStr = m_registerValueEdit->text().trimmed();
    if (valueStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入寄存器值");
        return;
    }

    bool ok = false;
    int64_t address = addressStr.toLongLong(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "警告", "寄存器地址格式错误，请输入16进制地址");
        return;
    }

    uint32_t value = valueStr.toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "警告", "寄存器值格式错误，请输入16进制值");
        return;
    }

    if (m_cameraManager->writeMemory(&value, address, 4)) {
        m_registerResultLabel->setText(QString("写入成功: 地址 0x%1 = 0x%2 (%3)")
            .arg(address, 0, 16)
            .arg(value, 8, 16, QChar('0'))
            .arg(value));
        m_registerResultLabel->setStyleSheet("color: #00ff00;");
    } else {
        m_registerResultLabel->setText("写入失败: " + m_cameraManager->getLastError());
        m_registerResultLabel->setStyleSheet("color: #ff0000;");
    }
}

