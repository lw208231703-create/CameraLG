#include "parameterwidget.h"
#include <QGroupBox>
#include <QEvent>
#include <QWheelEvent>
#include <QApplication>

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

ParameterWidget::ParameterWidget(CameraParameter* parameter, QWidget *parent)
    : QWidget(parent)
    , m_parameter(parameter)
    , m_nameLabel(nullptr)
    , m_valueWidget(nullptr)
    , m_readButton(nullptr)
    , m_writeButton(nullptr)
{
    setupUI();
}

ParameterWidget::~ParameterWidget()
{
}

void ParameterWidget::setupUI()
{
    if (m_parameter == nullptr) {
        return;
    }

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    m_nameLabel = new QLabel(m_parameter->getDisplayName(), this);
    m_nameLabel->setWordWrap(true);
    mainLayout->addWidget(m_nameLabel);

    switch (m_parameter->getType()) {
        case ParameterType::Int:
            m_valueWidget = createIntWidget();
            break;
        case ParameterType::Float:
            m_valueWidget = createFloatWidget();
            break;
        case ParameterType::Enum:
            m_valueWidget = createEnumWidget();
            break;
        case ParameterType::Bool:
            m_valueWidget = createBoolWidget();
            break;
        case ParameterType::String:
            m_valueWidget = createStringWidget();
            break;
        default:
            break;
    }

    if (m_valueWidget != nullptr) {
        mainLayout->addWidget(m_valueWidget);
    }

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(5);

    m_readButton = new QPushButton("读取", this);
    m_readButton->setEnabled(m_parameter->isReadable());
    connect(m_readButton, &QPushButton::clicked, this, &ParameterWidget::onReadButtonClicked);
    buttonLayout->addWidget(m_readButton);

    m_writeButton = new QPushButton("写入", this);
    m_writeButton->setEnabled(m_parameter->isWritable());
    connect(m_writeButton, &QPushButton::clicked, this, &ParameterWidget::onWriteButtonClicked);
    buttonLayout->addWidget(m_writeButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

QWidget* ParameterWidget::createIntWidget()
{
    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    QSpinBox* spinBox = new QSpinBox(this);
    spinBox->setRange(INT_MIN, INT_MAX);
    spinBox->installEventFilter(new WheelBlocker(spinBox));
    spinBox->setFocusPolicy(Qt::StrongFocus);
    
    if (m_parameter->getMinValue().isValid()) {
        spinBox->setMinimum(m_parameter->getMinValue().toInt());
    }
    if (m_parameter->getMaxValue().isValid()) {
        spinBox->setMaximum(m_parameter->getMaxValue().toInt());
    }
    if (m_parameter->getIncValue().isValid()) {
        spinBox->setSingleStep(m_parameter->getIncValue().toInt());
    }
    
    if (m_parameter->getValue().isValid()) {
        spinBox->setValue(m_parameter->getValue().toInt());
    }
    
    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ParameterWidget::onIntValueChanged);
    layout->addWidget(spinBox);

    return widget;
}

QWidget* ParameterWidget::createFloatWidget()
{
    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    QDoubleSpinBox* spinBox = new QDoubleSpinBox(this);
    spinBox->setRange(-1000000.0, 1000000.0);
    spinBox->setDecimals(6);
    spinBox->installEventFilter(new WheelBlocker(spinBox));
    spinBox->setFocusPolicy(Qt::StrongFocus);
    
    if (m_parameter->getMinValue().isValid()) {
        spinBox->setMinimum(m_parameter->getMinValue().toFloat());
    }
    if (m_parameter->getMaxValue().isValid()) {
        spinBox->setMaximum(m_parameter->getMaxValue().toFloat());
    }
    
    if (m_parameter->getValue().isValid()) {
        spinBox->setValue(m_parameter->getValue().toFloat());
    }
    
    connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ParameterWidget::onFloatValueChanged);
    layout->addWidget(spinBox);

    return widget;
}

QWidget* ParameterWidget::createEnumWidget()
{
    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    QComboBox* comboBox = new QComboBox(this);
    comboBox->installEventFilter(new WheelBlocker(comboBox));
    comboBox->setFocusPolicy(Qt::StrongFocus);
    
    QList<QString> entries = m_parameter->getEnumEntries();
    QList<unsigned int> values = m_parameter->getEnumValues();
    
    for (int i = 0; i < entries.size(); i++) {
        comboBox->addItem(entries[i], values[i]);
    }
    
    if (m_parameter->getValue().isValid()) {
        unsigned int currentValue = m_parameter->getValue().toUInt();
        int index = values.indexOf(currentValue);
        if (index >= 0) {
            comboBox->setCurrentIndex(index);
        }
    }
    
    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ParameterWidget::onEnumValueChanged);
    layout->addWidget(comboBox);

    return widget;
}

QWidget* ParameterWidget::createBoolWidget()
{
    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    QCheckBox* checkBox = new QCheckBox(this);
    
    if (m_parameter->getValue().isValid()) {
        checkBox->setChecked(m_parameter->getValue().toBool());
    }
    
    connect(checkBox, &QCheckBox::stateChanged, this, &ParameterWidget::onBoolValueChanged);
    layout->addWidget(checkBox);

    return widget;
}

QWidget* ParameterWidget::createStringWidget()
{
    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    QLineEdit* lineEdit = new QLineEdit(this);
    
    if (m_parameter->getValue().isValid()) {
        lineEdit->setText(m_parameter->getValue().toString());
    }
    
    connect(lineEdit, &QLineEdit::textChanged, this, &ParameterWidget::onStringValueChanged);
    layout->addWidget(lineEdit);

    return widget;
}

void ParameterWidget::updateValue(const QVariant& value)
{
    if (m_valueWidget == nullptr) {
        return;
    }

    switch (m_parameter->getType()) {
        case ParameterType::Int: {
            QSpinBox* spinBox = m_valueWidget->findChild<QSpinBox*>();
            if (spinBox != nullptr) {
                spinBox->blockSignals(true);
                spinBox->setValue(value.toInt());
                spinBox->blockSignals(false);
            }
            break;
        }
        case ParameterType::Float: {
            QDoubleSpinBox* spinBox = m_valueWidget->findChild<QDoubleSpinBox*>();
            if (spinBox != nullptr) {
                spinBox->blockSignals(true);
                spinBox->setValue(value.toFloat());
                spinBox->blockSignals(false);
            }
            break;
        }
        case ParameterType::Enum: {
            QComboBox* comboBox = m_valueWidget->findChild<QComboBox*>();
            if (comboBox != nullptr) {
                QList<unsigned int> values = m_parameter->getEnumValues();
                int index = values.indexOf(value.toUInt());
                if (index >= 0) {
                    comboBox->blockSignals(true);
                    comboBox->setCurrentIndex(index);
                    comboBox->blockSignals(false);
                }
            }
            break;
        }
        case ParameterType::Bool: {
            QCheckBox* checkBox = m_valueWidget->findChild<QCheckBox*>();
            if (checkBox != nullptr) {
                checkBox->blockSignals(true);
                checkBox->setChecked(value.toBool());
                checkBox->blockSignals(false);
            }
            break;
        }
        case ParameterType::String: {
            QLineEdit* lineEdit = m_valueWidget->findChild<QLineEdit*>();
            if (lineEdit != nullptr) {
                lineEdit->blockSignals(true);
                lineEdit->setText(value.toString());
                lineEdit->blockSignals(false);
            }
            break;
        }
        default:
            break;
    }
}

void ParameterWidget::onIntValueChanged(int value)
{
    emit valueChanged(m_parameter->getName(), value);
}

void ParameterWidget::onFloatValueChanged(double value)
{
    emit valueChanged(m_parameter->getName(), static_cast<float>(value));
}

void ParameterWidget::onEnumValueChanged(int index)
{
    QComboBox* comboBox = qobject_cast<QComboBox*>(sender());
    if (comboBox != nullptr) {
        unsigned int value = comboBox->itemData(index).toUInt();
        emit valueChanged(m_parameter->getName(), value);
    }
}

void ParameterWidget::onBoolValueChanged(int state)
{
    emit valueChanged(m_parameter->getName(), state == Qt::Checked);
}

void ParameterWidget::onStringValueChanged(const QString& text)
{
    emit valueChanged(m_parameter->getName(), text);
}

void ParameterWidget::onReadButtonClicked()
{
    emit valueChanged(m_parameter->getName(), QVariant());
}

void ParameterWidget::onWriteButtonClicked()
{
    QVariant value;
    
    switch (m_parameter->getType()) {
        case ParameterType::Int: {
            QSpinBox* spinBox = m_valueWidget->findChild<QSpinBox*>();
            if (spinBox != nullptr) {
                value = spinBox->value();
            }
            break;
        }
        case ParameterType::Float: {
            QDoubleSpinBox* spinBox = m_valueWidget->findChild<QDoubleSpinBox*>();
            if (spinBox != nullptr) {
                value = static_cast<float>(spinBox->value());
            }
            break;
        }
        case ParameterType::Enum: {
            QComboBox* comboBox = m_valueWidget->findChild<QComboBox*>();
            if (comboBox != nullptr) {
                value = comboBox->currentData().toUInt();
            }
            break;
        }
        case ParameterType::Bool: {
            QCheckBox* checkBox = m_valueWidget->findChild<QCheckBox*>();
            if (checkBox != nullptr) {
                value = checkBox->isChecked();
            }
            break;
        }
        case ParameterType::String: {
            QLineEdit* lineEdit = m_valueWidget->findChild<QLineEdit*>();
            if (lineEdit != nullptr) {
                value = lineEdit->text();
            }
            break;
        }
        default:
            break;
    }
    
    if (value.isValid()) {
        emit valueChanged(m_parameter->getName(), value);
    }
}