#ifndef EDITABLE_NUMBER_LABEL_H
#define EDITABLE_NUMBER_LABEL_H

#include <QLabel>
#include <QMouseEvent>

class EditableNumberLabel : public QLabel
{
    Q_OBJECT
public:
    explicit EditableNumberLabel(QWidget *parent = nullptr);
    void setValue(int value, int minValue = 1, int maxValue = 100);

signals:
    void valueChanged(int newValue);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int minValue_ = 1;
    int maxValue_ = 100;
};

#endif // EDITABLE_NUMBER_LABEL_H
