#include "cameraparameter.h"

CameraParameter::CameraParameter(QObject *parent)
    : QObject(parent)
    , m_type(ParameterType::Unknown)
    , m_readable(true)
    , m_writable(true)
{
}

CameraParameter::~CameraParameter()
{
}

void CameraParameter::setName(const QString& name)
{
    m_name = name;
}

QString CameraParameter::getName() const
{
    return m_name;
}

void CameraParameter::setDisplayName(const QString& displayName)
{
    m_displayName = displayName;
}

QString CameraParameter::getDisplayName() const
{
    return m_displayName.isEmpty() ? m_name : m_displayName;
}

void CameraParameter::setType(ParameterType type)
{
    m_type = type;
}

ParameterType CameraParameter::getType() const
{
    return m_type;
}

void CameraParameter::setValue(const QVariant& value)
{
    if (validateValue(value)) {
        m_value = value;
        emit valueChanged(m_name, value);
    }
}

QVariant CameraParameter::getValue() const
{
    return m_value;
}

void CameraParameter::setMinValue(const QVariant& value)
{
    m_minValue = value;
}

QVariant CameraParameter::getMinValue() const
{
    return m_minValue;
}

void CameraParameter::setMaxValue(const QVariant& value)
{
    m_maxValue = value;
}

QVariant CameraParameter::getMaxValue() const
{
    return m_maxValue;
}

void CameraParameter::setIncValue(const QVariant& value)
{
    m_incValue = value;
}

QVariant CameraParameter::getIncValue() const
{
    return m_incValue;
}

void CameraParameter::setEnumEntries(const QList<QString>& entries)
{
    m_enumEntries = entries;
}

QList<QString> CameraParameter::getEnumEntries() const
{
    return m_enumEntries;
}

void CameraParameter::setEnumValues(const QList<unsigned int>& values)
{
    m_enumValues = values;
}

QList<unsigned int> CameraParameter::getEnumValues() const
{
    return m_enumValues;
}

void CameraParameter::setReadable(bool readable)
{
    m_readable = readable;
}

bool CameraParameter::isReadable() const
{
    return m_readable;
}

void CameraParameter::setWritable(bool writable)
{
    m_writable = writable;
}

bool CameraParameter::isWritable() const
{
    return m_writable;
}

void CameraParameter::setCategory(const QString& category)
{
    m_category = category;
}

QString CameraParameter::getCategory() const
{
    return m_category;
}

void CameraParameter::setDescription(const QString& description)
{
    m_description = description;
}

QString CameraParameter::getDescription() const
{
    return m_description;
}

bool CameraParameter::validateValue(const QVariant& value) const
{
    if (!m_writable) {
        return false;
    }

    switch (m_type) {
        case ParameterType::Int:
            if (!value.canConvert<int64_t>()) {
                return false;
            }
            if (m_minValue.isValid() && value.toInt() < m_minValue.toInt()) {
                return false;
            }
            if (m_maxValue.isValid() && value.toInt() > m_maxValue.toInt()) {
                return false;
            }
            break;

        case ParameterType::Float:
            if (!value.canConvert<float>()) {
                return false;
            }
            if (m_minValue.isValid() && value.toFloat() < m_minValue.toFloat()) {
                return false;
            }
            if (m_maxValue.isValid() && value.toFloat() > m_maxValue.toFloat()) {
                return false;
            }
            break;

        case ParameterType::Enum:
            if (!value.canConvert<unsigned int>()) {
                return false;
            }
            if (!m_enumValues.contains(value.toUInt())) {
                return false;
            }
            break;

        case ParameterType::Bool:
            if (!value.canConvert<bool>()) {
                return false;
            }
            break;

        case ParameterType::String:
            if (!value.canConvert<QString>()) {
                return false;
            }
            break;

        default:
            return false;
    }

    return true;
}