#ifndef BIT_DEPTH_SLIDER_H
#define BIT_DEPTH_SLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QColor>

class BitDepthSlider : public QSlider {
    Q_OBJECT
public:
    explicit BitDepthSlider(QWidget *parent = nullptr);
    void setMaxBitDepth(int depth);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void updateValueFromPos(const QPoint &pos);
    int maxBitDepth_ = 16;
    
    QColor backgroundColor_;
    QColor textColor_;
    QColor separatorColor_;
    QColor handleColor_;
    QColor handleBorderColor_;
};

#endif // BIT_DEPTH_SLIDER_H
