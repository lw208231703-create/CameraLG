#ifndef GIGE_DOCK_H
#define GIGE_DOCK_H

#include <QDockWidget>
#include <QList>
#include <QTimer>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QLineEdit>
#include <QGroupBox>
#include "cameramanager.h"
#include "cameraparameter.h"
#include "parameterwidget.h"
#include "imagegrabber.h"

class GigECameraDevice;

class GigeDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit GigeDock(GigECameraDevice* cameraDevice, QWidget *parent = nullptr);
    ~GigeDock();
    
    // 方案3：设置位断提取参数
    void setBitShift(int shift);
    int getBitShift() const { return m_BitShift; }

private slots:
    void onEnumButtonClicked();
    void onConnectButtonClicked();
    void onDisconnectButtonClicked();
    void onLoadParamsButtonClicked();
    void onStartGrabButtonClicked();
    void onStopGrabButtonClicked();
    void onDeviceConnected();
    void onDeviceDisconnected();
    void onDeviceReconnected();
    void onErrorOccurred(const QString& error);
    void onParameterValueChanged(const QString& name, const QVariant& value);
    void onFrameReceived(const ImageData& image);
    void onReadRegisterClicked();
    void onWriteRegisterClicked();

private:
    void initializeConnections();
    void updateDeviceList(const QList<MV_CC_DEVICE_INFO*>& devices);
    void updateDeviceInfo(const MV_CC_DEVICE_INFO& info);
    void hideAllDeviceInfoLabels();
    void hideRegisterControls();
    void showRegisterControls();
    void updateDeviceInfoPlaceholder();
    void setLabelValue(QLabel *label, const QString &prefix, const QString &value, bool visible);
    void setLabelValue(QLabel *label, const QString &prefix, const QString &value);
    void clearParameterWidgets();
    void loadCameraParameters();
    void createParameterWidget(CameraParameter* parameter);
    void readParameter(CameraParameter* parameter);
    void writeParameter(CameraParameter* parameter, const QVariant& value);
    void displayImage(const ImageData& image);
    QString getPixelFormatString(MvGvspPixelType pixelFormat);
    
    // Helper methods for GigE image conversion
    QImage convertToQImage(const ImageData& image);
    QVector<uint16_t> convertToRawData(const ImageData& image);
    int getPixelBitDepth(MvGvspPixelType pixelFormat);  // Helper to get bit depth from pixel format

    QWidget *contentWidget_ = nullptr;
    CameraManager* m_cameraManager;
    ImageGrabber* m_imageGrabber;
    GigECameraDevice* m_gigeCameraDevice;  // GigE camera device (owns Manager + Grabber)
    QList<MV_CC_DEVICE_INFO*> m_deviceList;
    QList<CameraParameter*> m_parameters;
    QList<ParameterWidget*> m_parameterWidgets;

    QPushButton* m_enumButton;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_loadParamsButton;
    QPushButton* m_startGrabButton;
    QPushButton* m_stopGrabButton;
    QCheckBox* m_autoReconnectCheckBox;
    QComboBox* m_deviceComboBox;
    // QLabel* m_imageLabel; // Removed
    // QLabel* m_imageInfoLabel; // Removed
    QScrollArea* m_paramsScrollArea;
    QWidget* m_paramsWidget;
    QLabel* m_deviceInfoPlaceholderLabel;
    QLabel* m_manufacturerLabel;
    QLabel* m_modelLabel;
    QLabel* m_serialLabel;
    QLabel* m_ipLabel;
    QLabel* m_versionLabel;
    QLabel* m_userDefinedNameLabel;
    QLabel* m_macAddressLabel;
    QLabel* m_transportLayerTypeLabel;
    QLabel* m_productLineLabel;
    QLabel* m_productTypeLabel;
    QLabel* m_productSubtypeLabel;
    QLabel* m_subnetMaskLabel;
    QLabel* m_gatewayLabel;
    QLabel* m_ipCfgOptionLabel;
    QLabel* m_ipCfgCurrentLabel;
    QLabel* m_manufacturerSpecificInfoLabel;
    QLabel* m_netExportLabel;
    QLabel* m_vendorIdLabel;
    QLabel* m_productIdLabel;
    QLabel* m_deviceNumberLabel;
    QLabel* m_deviceGuidLabel;
    QLabel* m_familyNameLabel;
    QLabel* m_usbProtocolLabel;
    QLabel* m_deviceAddressLabel;
    QLabel* m_ctrlInEndPointLabel;
    QLabel* m_ctrlOutEndPointLabel;
    QLabel* m_streamEndPointLabel;
    QLabel* m_eventEndPointLabel;
    QLabel* m_temperatureLabel;

    bool m_isDeviceConnected = false;
    int m_BitShift{6};  // 方案3：位断提取起始位，默认6 (6-13位)

    // 寄存器读写相关控件
    QGroupBox* m_registerGroup;
    QLabel* m_registerPlaceholderLabel;
    QLineEdit* m_registerAddressEdit;
    QLineEdit* m_registerValueEdit;
    QPushButton* m_readRegisterButton;
    QPushButton* m_writeRegisterButton;
    QLabel* m_registerResultLabel;
};

#endif // GIGE_DOCK_H
