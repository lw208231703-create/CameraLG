#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QList>

enum class ParameterType {
    Int,
    Float,
    Enum,
    Bool,
    String,
    Unknown
};

struct ParameterInfo {
    QString name;
    QString displayName;
    ParameterType type;
    QVariant value;
    QVariant minValue;
    QVariant maxValue;
    QVariant incValue;
    QList<QString> enumEntries;
    QList<unsigned int> enumValues;
    bool readable;
    bool writable;
    QString category;
    QString description;
};

class CameraParameter : public QObject
{
    Q_OBJECT

public:
    explicit CameraParameter(QObject *parent = nullptr);
    ~CameraParameter();

    void setName(const QString& name);
    QString getName() const;

    void setDisplayName(const QString& displayName);
    QString getDisplayName() const;

    void setType(ParameterType type);
    ParameterType getType() const;

    void setValue(const QVariant& value);
    QVariant getValue() const;

    void setMinValue(const QVariant& value);
    QVariant getMinValue() const;

    void setMaxValue(const QVariant& value);
    QVariant getMaxValue() const;

    void setIncValue(const QVariant& value);
    QVariant getIncValue() const;

    void setEnumEntries(const QList<QString>& entries);
    QList<QString> getEnumEntries() const;

    void setEnumValues(const QList<unsigned int>& values);
    QList<unsigned int> getEnumValues() const;

    void setReadable(bool readable);
    bool isReadable() const;

    void setWritable(bool writable);
    bool isWritable() const;

    void setCategory(const QString& category);
    QString getCategory() const;

    void setDescription(const QString& description);
    QString getDescription() const;

    bool validateValue(const QVariant& value) const;

signals:
    void valueChanged(const QString& name, const QVariant& value);

private:
    QString m_name;
    QString m_displayName;
    ParameterType m_type;
    QVariant m_value;
    QVariant m_minValue;
    QVariant m_maxValue;
    QVariant m_incValue;
    QList<QString> m_enumEntries;
    QList<unsigned int> m_enumValues;
    bool m_readable;
    bool m_writable;
    QString m_category;
    QString m_description;
};