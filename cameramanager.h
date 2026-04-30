#pragma once

#ifdef ENABLE_GIGE_CAMERA

#include <QObject>
#include <QString>
#include <QList>
#include <QMutex>
#include <QXmlStreamReader>
#include "MvCameraControl.h"
#include "cameraparameter.h"

class CameraManager : public QObject
{
    Q_OBJECT

public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager();

    bool initialize();
    void finalize();

    QList<MV_CC_DEVICE_INFO*> enumDevices(unsigned int deviceType = MV_GIGE_DEVICE);
    bool connectDevice(MV_CC_DEVICE_INFO* deviceInfo);
    void disconnectDevice();
    bool isConnected() const;

    MV_CC_DEVICE_INFO getDeviceInfo() const;
    QString getLastError() const;

    bool getIntValue(const QString& key, int64_t& value);
    bool setIntValue(const QString& key, int64_t value);

    bool getFloatValue(const QString& key, float& value);
    bool setFloatValue(const QString& key, float value);

    bool getEnumValue(const QString& key, unsigned int& value);
    bool setEnumValue(const QString& key, unsigned int value);

    bool getBoolValue(const QString& key, bool& value);
    bool setBoolValue(const QString& key, bool value);

    bool getStringValue(const QString& key, QString& value);
    bool setStringValue(const QString& key, const QString& value);

    bool getEnumEntries(const QString& key, QList<QString>& entries, QList<unsigned int>& values);

    bool startGrabbing();
    bool stopGrabbing();

    void* getHandle() const { return m_handle; }

    bool getParameterInfo(const QString& key, MV_XML_InterfaceType& type, MV_XML_AccessMode& accessMode);
    QList<ParameterInfo> getSupportedParameters();
    QString getGenICamXML();

    bool optimizeGigEParameters();
    bool getOptimalPacketSize(int& packetSize);
    bool setResendMode(bool enable, int maxResendPercent = 50, int resendTimeout = 1000);

    bool readMemory(void* buffer, int64_t address, int64_t length);
    bool writeMemory(const void* buffer, int64_t address, int64_t length);

    void setAutoReconnect(bool enable);
    bool getAutoReconnect() const;

    static QString formatDeviceInfo(const MV_CC_DEVICE_INFO& info);
    static QString formatMacAddress(const MV_CC_DEVICE_INFO& info);
    static QString formatTransportLayerType(unsigned int type);
    static QString formatProductInfo(unsigned int devTypeInfo);
    static QString convertToQString(const unsigned char* data, int length);
    static QString ipToString(unsigned int ip);

signals:
    void deviceConnected();
    void deviceDisconnected();
    void deviceReconnected();
    void errorOccurred(const QString& error);

private:
    static void __stdcall exceptionCallback(unsigned int nMsgType, void* pUser);
    void handleException(unsigned int nMsgType);

private:
    void* m_handle;
    bool m_initialized;
    bool m_connected;
    MV_CC_DEVICE_INFO m_deviceInfo;
    bool m_autoReconnect;
    mutable QString m_lastError;
    mutable QMutex m_mutex;
};

#endif // ENABLE_GIGE_CAMERA
