#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include "cameraparameter.h"

class ParameterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterWidget(CameraParameter* parameter, QWidget *parent = nullptr);
    ~ParameterWidget();

    void updateValue(const QVariant& value);

signals:
    void valueChanged(const QString& name, const QVariant& value);

private slots:
    void onIntValueChanged(int value);
    void onFloatValueChanged(double value);
    void onEnumValueChanged(int index);
    void onBoolValueChanged(int state);
    void onStringValueChanged(const QString& text);
    void onReadButtonClicked();
    void onWriteButtonClicked();

private:
    void setupUI();
    QWidget* createIntWidget();
    QWidget* createFloatWidget();
    QWidget* createEnumWidget();
    QWidget* createBoolWidget();
    QWidget* createStringWidget();

    CameraParameter* m_parameter;
    QLabel* m_nameLabel;
    QWidget* m_valueWidget;
    QPushButton* m_readButton;
    QPushButton* m_writeButton;
};