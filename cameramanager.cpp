#include "cameramanager.h"

#ifdef ENABLE_GIGE_CAMERA

#include <QMutexLocker>
#include <QElapsedTimer>
#include <QThread>
#include <cstring>

CameraManager::CameraManager(QObject *parent)
    : QObject(parent)
    , m_handle(nullptr)
    , m_initialized(false)
    , m_connected(false)
    , m_autoReconnect(false)
{
}

CameraManager::~CameraManager()
{
    finalize();
}

bool CameraManager::initialize()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    int nRet = MV_CC_Initialize();
    if (nRet != MV_OK) {
        m_lastError = QString("Initialize failed, error code: %1").arg(nRet);
        return false;
    }
    
    m_initialized = true;
    return true;
}

void CameraManager::finalize()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    if (m_connected) {
        disconnectDevice();
    }
    
    MV_CC_Finalize();
    m_initialized = false;
}

QList<MV_CC_DEVICE_INFO*> CameraManager::enumDevices(unsigned int deviceType)
{
    QMutexLocker locker(&m_mutex);
    
    QList<MV_CC_DEVICE_INFO*> deviceList;
    
    MV_CC_DEVICE_INFO_LIST deviceInfoList;
    memset(&deviceInfoList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    
    int nRet = MV_CC_EnumDevices(deviceType, &deviceInfoList);
    if (nRet != MV_OK) {
        m_lastError = QString("Enum devices failed, error code: %1").arg(nRet);
        return deviceList;
    }
    
    for (unsigned int i = 0; i < deviceInfoList.nDeviceNum; i++) {
        MV_CC_DEVICE_INFO* pDeviceInfo = new MV_CC_DEVICE_INFO();
        memcpy(pDeviceInfo, deviceInfoList.pDeviceInfo[i], sizeof(MV_CC_DEVICE_INFO));
        deviceList.append(pDeviceInfo);
    }
    
    return deviceList;
}

bool CameraManager::connectDevice(MV_CC_DEVICE_INFO* deviceInfo)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_connected) {
        m_lastError = "Already connected";
        return false;
    }
    
    if (deviceInfo == nullptr) {
        m_lastError = "Invalid device info";
        return false;
    }
    
    int nRet = MV_CC_CreateHandle(&m_handle, deviceInfo);
    if (nRet != MV_OK) {
        m_lastError = QString("Create handle failed, error code: %1").arg(nRet);
        return false;
    }
    
    nRet = MV_CC_OpenDevice(m_handle, MV_ACCESS_Exclusive);
    if (nRet != MV_OK) {
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        m_lastError = QString("Open device failed, error code: %1").arg(nRet);
        return false;
    }

    memcpy(&m_deviceInfo, deviceInfo, sizeof(MV_CC_DEVICE_INFO));

    if (deviceInfo->nTLayerType == MV_GIGE_DEVICE) {
        optimizeGigEParameters();
    }

    nRet = MV_CC_RegisterExceptionCallBack(m_handle, exceptionCallback, this);
    if (nRet != MV_OK) {
        m_lastError = QString("Register exception callback failed, error code: %1").arg(nRet);
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    m_connected = true;
    emit deviceConnected();
    return true;
}

void CameraManager::disconnectDevice()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        return;
    }
    
    if (m_handle != nullptr) {
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
    }
    
    m_connected = false;
    emit deviceDisconnected();
}

bool CameraManager::isConnected() const
{
    QMutexLocker locker(&m_mutex);
    return m_connected;
}

MV_CC_DEVICE_INFO CameraManager::getDeviceInfo() const
{
    QMutexLocker locker(&m_mutex);
    
    MV_CC_DEVICE_INFO info;
    memset(&info, 0, sizeof(MV_CC_DEVICE_INFO));
    
    if (m_connected && m_handle != nullptr) {
        int nRet = MV_CC_GetDeviceInfo(m_handle, &info);
        if (nRet != MV_OK) {
            m_lastError = QString("Get device info failed, error code: %1").arg(nRet);
        }
    }
    
    return info;
}

QString CameraManager::getLastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

bool CameraManager::getIntValue(const QString& key, int64_t& value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    MVCC_INTVALUE_EX stIntValue;
    memset(&stIntValue, 0, sizeof(MVCC_INTVALUE_EX));
    
    int nRet = MV_CC_GetIntValueEx(m_handle, key.toUtf8().constData(), &stIntValue);
    if (nRet != MV_OK) {
        m_lastError = QString("Get int value failed for key '%1', error code: %2").arg(key).arg(nRet);
        return false;
    }
    
    value = stIntValue.nCurValue;
    return true;
}

bool CameraManager::setIntValue(const QString& key, int64_t value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    int nRet = MV_CC_SetIntValueEx(m_handle, key.toUtf8().constData(), value);
    if (nRet != MV_OK) {
        m_lastError = QString("Set int value failed for key '%1', error code: %2").arg(key).arg(nRet);
        emit errorOccurred(m_lastError);
        return false;
    }
    
    return true;
}

bool CameraManager::getFloatValue(const QString& key, float& value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    MVCC_FLOATVALUE stFloatValue;
    memset(&stFloatValue, 0, sizeof(MVCC_FLOATVALUE));
    
    int nRet = MV_CC_GetFloatValue(m_handle, key.toUtf8().constData(), &stFloatValue);
    if (nRet != MV_OK) {
        m_lastError = QString("Get float value failed for key '%1', error code: %2").arg(key).arg(nRet);
        return false;
    }
    
    value = stFloatValue.fCurValue;
    return true;
}

bool CameraManager::setFloatValue(const QString& key, float value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    int nRet = MV_CC_SetFloatValue(m_handle, key.toUtf8().constData(), value);
    if (nRet != MV_OK) {
        m_lastError = QString("Set float value failed for key '%1', error code: %2").arg(key).arg(nRet);
        emit errorOccurred(m_lastError);
        return false;
    }
    
    return true;
}

bool CameraManager::getEnumValue(const QString& key, unsigned int& value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    MVCC_ENUMVALUE stEnumValue;
    memset(&stEnumValue, 0, sizeof(MVCC_ENUMVALUE));
    
    int nRet = MV_CC_GetEnumValue(m_handle, key.toUtf8().constData(), &stEnumValue);
    if (nRet != MV_OK) {
        m_lastError = QString("Get enum value failed for key '%1', error code: %2").arg(key).arg(nRet);
        return false;
    }
    
    value = stEnumValue.nCurValue;
    return true;
}

bool CameraManager::setEnumValue(const QString& key, unsigned int value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    int nRet = MV_CC_SetEnumValue(m_handle, key.toUtf8().constData(), value);
    if (nRet != MV_OK) {
        m_lastError = QString("Set enum value failed for key '%1', error code: %2").arg(key).arg(nRet);
        emit errorOccurred(m_lastError);
        return false;
    }
    
    return true;
}

bool CameraManager::getBoolValue(const QString& key, bool& value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    bool boolValue = false;
    int nRet = MV_CC_GetBoolValue(m_handle, key.toUtf8().constData(), &boolValue);
    if (nRet != MV_OK) {
        m_lastError = QString("Get bool value failed for key '%1', error code: %2").arg(key).arg(nRet);
        return false;
    }
    
    value = boolValue;
    return true;
}

bool CameraManager::setBoolValue(const QString& key, bool value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    int nRet = MV_CC_SetBoolValue(m_handle, key.toUtf8().constData(), value);
    if (nRet != MV_OK) {
        m_lastError = QString("Set bool value failed for key '%1', error code: %2").arg(key).arg(nRet);
        emit errorOccurred(m_lastError);
        return false;
    }
    
    return true;
}

bool CameraManager::getStringValue(const QString& key, QString& value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    MVCC_STRINGVALUE stringValue;
    memset(&stringValue, 0, sizeof(MVCC_STRINGVALUE));
    
    int nRet = MV_CC_GetStringValue(m_handle, key.toUtf8().constData(), &stringValue);
    if (nRet != MV_OK) {
        m_lastError = QString("Get string value failed for key '%1', error code: %2").arg(key).arg(nRet);
        return false;
    }
    
    value = QString::fromUtf8(stringValue.chCurValue);
    return true;
}

bool CameraManager::setStringValue(const QString& key, const QString& value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    int nRet = MV_CC_SetStringValue(m_handle, key.toUtf8().constData(), value.toUtf8().constData());
    if (nRet != MV_OK) {
        m_lastError = QString("Set string value failed for key '%1', error code: %2").arg(key).arg(nRet);
        emit errorOccurred(m_lastError);
        return false;
    }
    
    return true;
}

bool CameraManager::getEnumEntries(const QString& key, QList<QString>& entries, QList<unsigned int>& values)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    MVCC_ENUMVALUE_EX stEnumValue;
    memset(&stEnumValue, 0, sizeof(MVCC_ENUMVALUE_EX));
    
    int nRet = MV_CC_GetEnumValueEx(m_handle, key.toUtf8().constData(), &stEnumValue);
    if (nRet != MV_OK) {
        m_lastError = QString("Get enum value failed for key '%1', error code: %2").arg(key).arg(nRet);
        return false;
    }
    
    entries.clear();
    values.clear();
    
    for (unsigned int i = 0; i < stEnumValue.nSupportedNum; i++) {
        unsigned int enumValue = stEnumValue.nSupportValue[i];
        values.append(enumValue);
        
        MVCC_ENUMENTRY stEnumEntry;
        memset(&stEnumEntry, 0, sizeof(MVCC_ENUMENTRY));
        stEnumEntry.nValue = enumValue;
        
        if (MV_CC_GetEnumEntrySymbolic(m_handle, key.toUtf8().constData(), &stEnumEntry) == MV_OK) {
            entries.append(QString::fromUtf8(stEnumEntry.chSymbolic));
        } else {
            entries.append(QString("Value_%1").arg(enumValue));
        }
    }
    
    return true;
}

bool CameraManager::getParameterInfo(const QString& key, MV_XML_InterfaceType& type, MV_XML_AccessMode& accessMode)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    int nRet = MV_XML_GetNodeInterfaceType(m_handle, key.toUtf8().constData(), &type);
    if (nRet != MV_OK) {
        m_lastError = QString("Get node interface type failed for key '%1', error code: %2").arg(key).arg(nRet);
        return false;
    }
    
    nRet = MV_XML_GetNodeAccessMode(m_handle, key.toUtf8().constData(), &accessMode);
    if (nRet != MV_OK) {
        m_lastError = QString("Get node access mode failed for key '%1', error code: %2").arg(key).arg(nRet);
        return false;
    }
    
    return true;
}

QList<ParameterInfo> CameraManager::getSupportedParameters()
{
    QList<ParameterInfo> paramList;
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return paramList;
    }
    
    QString xmlContent = getGenICamXML();
    if (xmlContent.isEmpty()) {
        return paramList;
    }
    
    QXmlStreamReader xml(xmlContent);
    
    QString currentCategory;
    QString currentFeature;
    
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            QString elementName = xml.name().toString();
            
            if (elementName == "Category") {
                currentCategory = xml.attributes().value("Name").toString();
            }
            else if (elementName == "pFeature") {
                currentFeature = xml.attributes().value("Name").toString();
            }
            else if (elementName == "Integer" || 
                       elementName == "Float" || 
                       elementName == "Enumeration" || 
                       elementName == "Boolean" || 
                       elementName == "String") {
                
                QString paramName = xml.attributes().value("Name").toString();
                if (!paramName.isEmpty()) {
                    MV_XML_InterfaceType type;
                    MV_XML_AccessMode accessMode;
                    
                    QMutexLocker locker(&m_mutex);
                    
                    int nRet = MV_XML_GetNodeInterfaceType(m_handle, paramName.toUtf8().constData(), &type);
                    if (nRet == MV_OK) {
                        nRet = MV_XML_GetNodeAccessMode(m_handle, paramName.toUtf8().constData(), &accessMode);
                        if (nRet == MV_OK && (accessMode == AM_RW || accessMode == AM_RO)) {
                            ParameterInfo info;
                            info.name = paramName;
                            info.displayName = paramName;
                            info.category = currentCategory;
                            info.readable = (accessMode == AM_RW || accessMode == AM_RO);
                            info.writable = (accessMode == AM_RW);
                            
                            if (type == IFT_IInteger) {
                                info.type = ParameterType::Int;
                                
                                MVCC_INTVALUE_EX intVal;
                                if (MV_CC_GetIntValueEx(m_handle, paramName.toUtf8().constData(), &intVal) == MV_OK) {
                                    info.value = intVal.nCurValue;
                                    info.minValue = intVal.nMin;
                                    info.maxValue = intVal.nMax;
                                    info.incValue = intVal.nInc;
                                }
                            }
                            else if (type == IFT_IFloat) {
                                info.type = ParameterType::Float;
                                
                                MVCC_FLOATVALUE floatVal;
                                if (MV_CC_GetFloatValue(m_handle, paramName.toUtf8().constData(), &floatVal) == MV_OK) {
                                    info.value = floatVal.fCurValue;
                                    info.minValue = floatVal.fMin;
                                    info.maxValue = floatVal.fMax;
                                }
                            }
                            else if (type == IFT_IEnumeration) {
                                info.type = ParameterType::Enum;
                                
                                MVCC_ENUMVALUE_EX enumVal;
                                if (MV_CC_GetEnumValueEx(m_handle, paramName.toUtf8().constData(), &enumVal) == MV_OK) {
                                    info.value = enumVal.nCurValue;
                                    
                                    for (unsigned int i = 0; i < enumVal.nSupportedNum; i++) {
                                        unsigned int enumValue = enumVal.nSupportValue[i];
                                        info.enumValues.append(enumValue);
                                        
                                        MVCC_ENUMENTRY entry;
                                        memset(&entry, 0, sizeof(MVCC_ENUMENTRY));
                                        entry.nValue = enumValue;
                                        
                                        if (MV_CC_GetEnumEntrySymbolic(m_handle, paramName.toUtf8().constData(), &entry) == MV_OK) {
                                            info.enumEntries.append(QString::fromUtf8(entry.chSymbolic));
                                        } else {
                                            info.enumEntries.append(QString("Value_%1").arg(enumValue));
                                        }
                                    }
                                }
                            }
                            else if (type == IFT_IBoolean) {
                                info.type = ParameterType::Bool;
                                
                                bool boolVal = false;
                                if (MV_CC_GetBoolValue(m_handle, paramName.toUtf8().constData(), &boolVal) == MV_OK) {
                                    info.value = boolVal;
                                }
                            }
                            else if (type == IFT_IString) {
                                info.type = ParameterType::String;
                                
                                MVCC_STRINGVALUE stringVal;
                                memset(&stringVal, 0, sizeof(MVCC_STRINGVALUE));
                                if (MV_CC_GetStringValue(m_handle, paramName.toUtf8().constData(), &stringVal) == MV_OK) {
                                    info.value = QString::fromUtf8(stringVal.chCurValue);
                                }
                            }
                            
                            paramList.append(info);
                        }
                    }
                }
            }
        }
    }
    
    if (xml.hasError()) {
        m_lastError = QString("XML parsing error: %1").arg(xml.errorString());
    }
    
    return paramList;
}

QString CameraManager::getGenICamXML()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return QString();
    }
    
    unsigned int nDataLen = 0;
    
    int nRet = MV_XML_GetGenICamXML(m_handle, nullptr, 0, &nDataLen);
    if (nRet != MV_OK && nDataLen == 0) {
        m_lastError = QString("Get GenICam XML size failed, error code: %1").arg(nRet);
        return QString();
    }
    
    QByteArray xmlData;
    xmlData.resize(nDataLen);
    
    nRet = MV_XML_GetGenICamXML(m_handle, 
                                reinterpret_cast<unsigned char*>(xmlData.data()), 
                                nDataLen, 
                                &nDataLen);
    if (nRet != MV_OK) {
        m_lastError = QString("Get GenICam XML failed, error code: %1").arg(nRet);
        return QString();
    }
    
    return QString::fromUtf8(xmlData.data(), nDataLen);
}

QString CameraManager::formatDeviceInfo(const MV_CC_DEVICE_INFO& info)
{
    QString deviceInfo;
    
    if (info.nTLayerType == MV_GIGE_DEVICE) {
        deviceInfo = QString("GigE: %1.%2.%3.%4")
            .arg((info.SpecialInfo.stGigEInfo.nCurrentIp >> 24) & 0xFF)
            .arg((info.SpecialInfo.stGigEInfo.nCurrentIp >> 16) & 0xFF)
            .arg((info.SpecialInfo.stGigEInfo.nCurrentIp >> 8) & 0xFF)
            .arg(info.SpecialInfo.stGigEInfo.nCurrentIp & 0xFF);
    } else if (info.nTLayerType == MV_USB_DEVICE) {
        deviceInfo = QString("USB: %1")
            .arg(convertToQString(info.SpecialInfo.stUsb3VInfo.chSerialNumber, 64));
    } else {
        deviceInfo = "Unknown";
    }
    
    return deviceInfo;
}

QString CameraManager::formatMacAddress(const MV_CC_DEVICE_INFO& info)
{
    QString macAddress;
    
    if (info.nTLayerType == MV_GIGE_DEVICE) {
        unsigned int macHigh = info.nMacAddrHigh;
        unsigned int macLow = info.nMacAddrLow;
        
        macAddress = QString("%1:%2:%3:%4:%5:%6")
            .arg((macHigh >> 8) & 0xFF, 2, 16, QChar('0'))
            .arg(macHigh & 0xFF, 2, 16, QChar('0'))
            .arg((macLow >> 24) & 0xFF, 2, 16, QChar('0'))
            .arg((macLow >> 16) & 0xFF, 2, 16, QChar('0'))
            .arg((macLow >> 8) & 0xFF, 2, 16, QChar('0'))
            .arg(macLow & 0xFF, 2, 16, QChar('0'));
    } else if (info.nTLayerType == MV_USB_DEVICE) {
        macAddress = "N/A";
    } else {
        macAddress = "Unknown";
    }
    
    return macAddress;
}

QString CameraManager::formatTransportLayerType(unsigned int type)
{
    switch (type) {
        case MV_GIGE_DEVICE:
            return "GigE";
        case MV_USB_DEVICE:
            return "USB3.0";
        default:
            return "Unknown";
    }
}

QString CameraManager::formatProductInfo(unsigned int devTypeInfo)
{
    return QString("%1.%2.%3.%4")
        .arg((devTypeInfo >> 24) & 0xFF)
        .arg((devTypeInfo >> 16) & 0xFF)
        .arg((devTypeInfo >> 8) & 0xFF)
        .arg(devTypeInfo & 0xFF);
}

QString CameraManager::convertToQString(const unsigned char* data, int length)
{
    if (data == nullptr || length <= 0) {
        return QString();
    }

    int actualLen = 0;
    while (actualLen < length && data[actualLen] != 0) {
        ++actualLen;
    }

    if (actualLen <= 0) {
        return QString();
    }

    const QByteArray bytes(reinterpret_cast<const char*>(data), actualLen);
    QString utf8 = QString::fromUtf8(bytes);
    if (utf8.contains(QChar::ReplacementCharacter)) {
        return QString::fromLocal8Bit(bytes).trimmed();
    }

    return utf8.trimmed();
}

QString CameraManager::ipToString(unsigned int ip)
{
    return QString("%1.%2.%3.%4")
        .arg((ip >> 24) & 0xFF)
        .arg((ip >> 16) & 0xFF)
        .arg((ip >> 8) & 0xFF)
        .arg(ip & 0xFF);
}

bool CameraManager::optimizeGigEParameters()
{
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    MV_CC_DEVICE_INFO info;
    memset(&info, 0, sizeof(MV_CC_DEVICE_INFO));
    int nRet = MV_CC_GetDeviceInfo(m_handle, &info);
    if (nRet != MV_OK) {
        m_lastError = QString("Get device info failed, error code: %1").arg(nRet);
        return false;
    }
    
    if (info.nTLayerType != MV_GIGE_DEVICE) {
        m_lastError = "Not a GigE device";
        return false;
    }
    
    int packetSize = 0;
    if (!getOptimalPacketSize(packetSize)) {
        return false;
    }
    
    if (!setResendMode(true)) {
        return false;
    }
    
    return true;
}

bool CameraManager::getOptimalPacketSize(int& packetSize)
{
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    int nRet = MV_CC_GetOptimalPacketSize(m_handle);
    if (nRet <= 0) {
        m_lastError = QString("Get optimal packet size failed, error code: %1").arg(nRet);
        return false;
    }
    
    packetSize = nRet;
    
    if (setIntValue("GevSCPSPacketSize", packetSize)) {
        return true;
    }
    
    QList<QString> entries;
    QList<unsigned int> values;
    if (getEnumEntries("GevSCPSPacketSize", entries, values)) {
        auto it = std::find(values.begin(), values.end(), packetSize);
        if (it != values.end()) {
            return setEnumValue("GevSCPSPacketSize", *it);
        }
    }
    
    return false;
}

bool CameraManager::setResendMode(bool enable, int maxResendPercent, int resendTimeout)
{
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    if (!setBoolValue("GevStreamChannelEnableResend", enable)) {
        return false;
    }
    
    if (maxResendPercent >= 0 && maxResendPercent <= 100) {
        if (!setIntValue("GevStreamChannelMaxResendPercent", maxResendPercent)) {
            return false;
        }
    }
    
    if (resendTimeout >= 0 && resendTimeout <= 10000) {
        if (!setIntValue("GevStreamChannelResendTimeout", resendTimeout)) {
            return false;
        }
    }
    
    return true;
}

bool CameraManager::readMemory(void* buffer, int64_t address, int64_t length)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    if (buffer == nullptr) {
        m_lastError = "Buffer is null";
        return false;
    }
    
    if (length <= 0) {
        m_lastError = "Invalid length";
        return false;
    }
    
    int nRet = MV_CC_ReadMemory(m_handle, buffer, address, length);
    if (nRet != MV_OK) {
        m_lastError = QString("Read memory failed, error code: %1").arg(nRet);
        return false;
    }
    
    return true;
}

bool CameraManager::writeMemory(const void* buffer, int64_t address, int64_t length)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || m_handle == nullptr) {
        m_lastError = "Device not connected";
        return false;
    }
    
    if (buffer == nullptr) {
        m_lastError = "Buffer is null";
        return false;
    }
    
    if (length <= 0) {
        m_lastError = "Invalid length";
        return false;
    }
    
    int nRet = MV_CC_WriteMemory(m_handle, buffer, address, length);
    if (nRet != MV_OK) {
        m_lastError = QString("Write memory failed, error code: %1").arg(nRet);
        return false;
    }
    
    return true;
}

void CameraManager::setAutoReconnect(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_autoReconnect = enable;
}

bool CameraManager::getAutoReconnect() const
{
    QMutexLocker locker(&m_mutex);
    return m_autoReconnect;
}

void __stdcall CameraManager::exceptionCallback(unsigned int nMsgType, void* pUser)
{
    if (pUser == nullptr) {
        return;
    }

    CameraManager* pThis = static_cast<CameraManager*>(pUser);
    pThis->handleException(nMsgType);
}

void CameraManager::handleException(unsigned int nMsgType)
{
    if (nMsgType == MV_EXCEPTION_DEV_DISCONNECT) {
        if (!m_autoReconnect) {
            return;
        }

        QMutexLocker locker(&m_mutex);

        if (m_handle != nullptr) {
            MV_CC_CloseDevice(m_handle);
            MV_CC_DestroyHandle(m_handle);
            m_handle = nullptr;
        }

        m_connected = false;
        emit deviceDisconnected();

        bool bConnected = false;
        bool bFound = false;
        int retryCount = 0;
        const int maxRetryCount = 10;
        const int retryInterval = 1000;

        while (!bConnected && retryCount < maxRetryCount) {
            retryCount++;

            MV_CC_DEVICE_INFO_LIST stDevTempList;
            memset(&stDevTempList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

            int nRet = MV_CC_EnumDevices(m_deviceInfo.nTLayerType, &stDevTempList);
            if (nRet != MV_OK) {
                m_lastError = QString("Enum devices failed during reconnect, error code: %1").arg(nRet);
                QThread::msleep(retryInterval);
                continue;
            }

            bFound = false;

            for (unsigned int i = 0; i < stDevTempList.nDeviceNum; i++) {
                if (m_deviceInfo.nTLayerType == MV_USB_DEVICE) {
                    if (0 == strcmp((const char*)m_deviceInfo.SpecialInfo.stUsb3VInfo.chSerialNumber,
                                     (const char*)stDevTempList.pDeviceInfo[i]->SpecialInfo.stUsb3VInfo.chSerialNumber)) {
                        memcpy(&m_deviceInfo, stDevTempList.pDeviceInfo[i], sizeof(MV_CC_DEVICE_INFO));
                        bFound = true;
                    }
                } else if (m_deviceInfo.nTLayerType == MV_GIGE_DEVICE) {
                    if (0 == strcmp((const char*)m_deviceInfo.SpecialInfo.stGigEInfo.chSerialNumber,
                                     (const char*)stDevTempList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.chSerialNumber)) {
                        memcpy(&m_deviceInfo, stDevTempList.pDeviceInfo[i], sizeof(MV_CC_DEVICE_INFO));
                        bFound = true;
                    }
                }

                if (bFound) {
                    break;
                }
            }

            if (!bFound) {
                m_lastError = QString("Device not found during reconnect, retry count: %1").arg(retryCount);
                QThread::msleep(retryInterval);
                continue;
            }

            nRet = MV_CC_CreateHandle(&m_handle, &m_deviceInfo);
            if (nRet != MV_OK) {
                m_lastError = QString("Create handle failed during reconnect, error code: %1").arg(nRet);
                QThread::msleep(retryInterval);
                continue;
            }

            nRet = MV_CC_OpenDevice(m_handle, MV_ACCESS_Exclusive);
            if (nRet != MV_OK) {
                MV_CC_DestroyHandle(m_handle);
                m_handle = nullptr;
                m_lastError = QString("Open device failed during reconnect, error code: %1").arg(nRet);
                QThread::msleep(retryInterval);
                continue;
            }

            if (m_deviceInfo.nTLayerType == MV_GIGE_DEVICE) {
                optimizeGigEParameters();
            }

            nRet = MV_CC_RegisterExceptionCallBack(m_handle, exceptionCallback, this);
            if (nRet != MV_OK) {
                MV_CC_CloseDevice(m_handle);
                MV_CC_DestroyHandle(m_handle);
                m_handle = nullptr;
                m_lastError = QString("Register exception callback failed during reconnect, error code: %1").arg(nRet);
                QThread::msleep(retryInterval);
                continue;
            }

            m_connected = true;
            bConnected = true;
            emit deviceReconnected();
        }

        if (!bConnected) {
            emit errorOccurred("Failed to reconnect after " + QString::number(maxRetryCount) + " attempts");
        }
    }
}

#endif // ENABLE_GIGE_CAMERA