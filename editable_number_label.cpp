#include "editable_number_label.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>

EditableNumberLabel::EditableNumberLabel(QWidget *parent)
    : QLabel(parent), minValue_(1), maxValue_(100)
{
    setCursor(Qt::PointingHandCursor);
    setStyleSheet("EditableNumberLabel { color: #d4d4d4; background: transparent; } "
                  "QToolTip { color: black; background-color: #FFFFDC; border: 1px solid gray; }");
}

void EditableNumberLabel::setValue(int value, int minValue, int maxValue)
{
    minValue_ = minValue;
    maxValue_ = maxValue;
    setText(QString::number(value));
}

void EditableNumberLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    
    QDialog dialog(parentWidget());
    dialog.setWindowTitle(tr("输入缓冲区数量"));
    dialog.setModal(true);
    dialog.setWindowFlags(dialog.windowFlags() | Qt::WindowStaysOnTopHint);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *label = new QLabel(tr("请输入缓冲区数量 (%1-%2):").arg(minValue_).arg(maxValue_), &dialog);
    layout->addWidget(label);
    
    QSpinBox *spinBox = new QSpinBox(&dialog);
    spinBox->setMinimum(minValue_);
    spinBox->setMaximum(maxValue_);
    spinBox->setValue(text().toInt());
    spinBox->setFocus();
    spinBox->selectAll();
    layout->addWidget(spinBox);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (dialog.exec() == QDialog::Accepted) {
        int newValue = spinBox->value();
        setText(QString::number(newValue));
        emit valueChanged(newValue);
    }
}
