#ifndef DEVICE_SELECTOR_WIDGET_H
#define DEVICE_SELECTOR_WIDGET_H

#include <QGroupBox>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;

class DeviceSelectorWidget : public QGroupBox
{
    Q_OBJECT
public:
    struct DeviceInfo
    {
        QString name;
        QString interfaceType;
    };

    explicit DeviceSelectorWidget(QWidget *parent = nullptr);

    void setDevices(const QVector<DeviceInfo> &devices);
    void setConfigurations(const QStringList &configurations, bool emitSignal = false);
    void clearDevices();
    void clearConfigurations();
    
    // 获取当前选择的设备和配置
    QString getCurrentDeviceName() const;
    QString getCurrentConfiguration() const;
    int getCurrentDeviceIndex() const;
    
    // 设置配置文件目录
    void setConfigDirectory(const QString &directory);
    QString getConfigDirectory() const;

signals:
    void deviceChanged(const DeviceInfo &info);
    void configurationChanged(const QString &configName);
    void configDirectoryChanged(const QString &directory);

private slots:
    void handleDeviceIndexChanged(int index);
    void handleConfigIndexChanged(int index);
    void handleBrowseConfigFolder();

private:
    void scanConfigurationFiles(const QString &directory);
    
    QVector<DeviceInfo> devices_;
    QComboBox *deviceCombo_;
    QComboBox *configCombo_;
    QPushButton *browseConfigBtn_;
    QString configDirectory_;
};

#endif // DEVICE_SELECTOR_WIDGET_H
