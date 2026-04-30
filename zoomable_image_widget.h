#ifndef ZOOMABLE_IMAGE_WIDGET_H
#define ZOOMABLE_IMAGE_WIDGET_H

#include <QWidget>
#include <QImage>
#include <QPointF>

class ZoomableImageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ZoomableImageWidget(QWidget *parent = nullptr);
    void setImage(const QImage &image);
    void resetView();

public slots:
    void setTransform(float scale, const QPointF &offset);

signals:
    void transformChanged(float scale, const QPointF &offset);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QImage m_image;
    float m_scale;
    QPointF m_offset;
    
    bool m_isPanning;
    QPoint m_lastMousePos;
    
    void updateTransform(float scale, const QPointF &offset);
};

#endif // ZOOMABLE_IMAGE_WIDGET_H
