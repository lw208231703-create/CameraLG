#include "bit_depth_slider.h"
#include <QPainter>
#include <QMouseEvent>
#include <cmath>

BitDepthSlider::BitDepthSlider(QWidget *parent) : QSlider(parent) {
    setOrientation(Qt::Horizontal);
    setMinimum(0);
    setMaximum(8);
    setFixedHeight(30); 
    setCursor(Qt::PointingHandCursor);
    
    // 初始化颜色 - 深色主题
    backgroundColor_ = QColor(62, 62, 66, 255);
    textColor_ = QColor(212, 212, 212, 255);
    separatorColor_ = QColor(80, 80, 84, 255);
    handleColor_ = QColor(38, 192, 166, 120);
    handleBorderColor_ = QColor(38, 192, 166, 180);
}

void BitDepthSlider::setMaxBitDepth(int depth) {
    int newMax = (depth > 16) ? 32 : 16;
    
    if (newMax != maxBitDepth_) {
        maxBitDepth_ = newMax;
        setMaximum(maxBitDepth_ - 8);
        update();
    }
}

void BitDepthSlider::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    double stepW = w / (double)maxBitDepth_;

    painter.setPen(Qt::NoPen);
    painter.setBrush(backgroundColor_);
    painter.drawRect(0, 0, w, h);

    painter.setPen(textColor_);
    QFont font = painter.font();
    font.setPixelSize(10);
    painter.setFont(font);

    for (int i = 0; i < maxBitDepth_; ++i) {
        int displayValue = maxBitDepth_ - 1 - i;
        double x = i * stepW;
        QRectF rect(x, 0, stepW, h);
        
        painter.drawText(rect, Qt::AlignCenter, QString::number(displayValue));
        
        if (i > 0) {
            painter.setPen(separatorColor_);
            painter.drawLine(x, 0, x, h);
            painter.setPen(textColor_);
        }
    }

    int val = value();
    int reversedVal = maxBitDepth_ - 8 - val;
    double handleX = reversedVal * stepW;
    double handleW = 8 * stepW;
    
    painter.setPen(QPen(handleBorderColor_, 2));
    painter.setBrush(handleColor_);
    painter.drawRect(QRectF(handleX, 1, handleW, h - 2));
}

void BitDepthSlider::mousePressEvent(QMouseEvent *event) {
    updateValueFromPos(event->pos());
}

void BitDepthSlider::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        updateValueFromPos(event->pos());
    }
}

void BitDepthSlider::updateValueFromPos(const QPoint &pos) {
    double stepW = width() / (double)maxBitDepth_;
    int posVal = qRound(pos.x() / stepW - 4);
    if (posVal < minimum()) posVal = minimum();
    if (posVal > maximum()) posVal = maximum();
    int newVal = maximum() - posVal;
    setValue(newVal);
}
