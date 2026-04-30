#include "zoomable_image_widget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <cmath>

ZoomableImageWidget::ZoomableImageWidget(QWidget *parent)
    : QWidget(parent)
    , m_scale(1.0f)
    , m_offset(0, 0)
    , m_isPanning(false)
{
    // Set default background policy
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    setMouseTracking(true); 
}

void ZoomableImageWidget::setImage(const QImage &image)
{
    bool firstLoad = m_image.isNull();
    m_image = image;
    
    if (firstLoad && !m_image.isNull()) {
        resetView();
    }
    update();
}

void ZoomableImageWidget::resetView()
{
    if (m_image.isNull()) return;
    
    // Fit to view by default or just center 1:1?
    // Usually "fit" is better for initial view if image is large.
    // But let's start with 1:1 centered or fit if too big.
    
    float wRatio = (float)width() / m_image.width();
    float hRatio = (float)height() / m_image.height();
    m_scale = qMin(wRatio, hRatio);
    
    if (m_scale > 1.0f) m_scale = 1.0f; // Don't upscale initially if image is small
    
    m_offset.setX((width() - m_image.width() * m_scale) / 2.0);
    m_offset.setY((height() - m_image.height() * m_scale) / 2.0);
    
    updateTransform(m_scale, m_offset);
}

void ZoomableImageWidget::setTransform(float scale, const QPointF &offset)
{
    // Avoid infinite loops if signal connected back
    if (qAbs(m_scale - scale) > 0.0001f || m_offset != offset) {
        m_scale = scale;
        m_offset = offset;
        update();
    }
}

void ZoomableImageWidget::updateTransform(float scale, const QPointF &offset)
{
    m_scale = scale;
    m_offset = offset;
    update();
    emit transformChanged(m_scale, m_offset);
}

void ZoomableImageWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    
    // Background is handled by autoFillBackground, but we can ensure it's clean
    // painter.fillRect(rect(), palette().window());

    if (m_image.isNull()) {
        painter.drawText(rect(), Qt::AlignCenter, tr("等待图像..."));
        return;
    }

    // 连续采集 + 放大时，如果对整幅图像做缩放再裁剪，会导致巨大的 CPU 开销。
    // 这里改为：根据当前变换计算“可见区域”的 sourceRect，只绘制可见部分。
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    if (m_scale <= 0.0f) {
        return;
    }

    const QRectF widgetRect = QRectF(rect());

    // 将窗口可见区域反算到图像坐标系
    QRectF srcRect(
        (widgetRect.left() - m_offset.x()) / m_scale,
        (widgetRect.top() - m_offset.y()) / m_scale,
        widgetRect.width() / m_scale,
        widgetRect.height() / m_scale
    );

    const QRectF imgBounds(0.0, 0.0, m_image.width(), m_image.height());
    srcRect = srcRect.intersected(imgBounds);
    if (srcRect.isEmpty()) {
        return;
    }

    // 将裁剪后的 sourceRect 正向映射到窗口坐标系
    const QRectF dstRect(
        srcRect.x() * m_scale + m_offset.x(),
        srcRect.y() * m_scale + m_offset.y(),
        srcRect.width() * m_scale,
        srcRect.height() * m_scale
    );

    painter.drawImage(dstRect, m_image, srcRect);
}

void ZoomableImageWidget::wheelEvent(QWheelEvent *event)
{
    if (m_image.isNull()) return;

    const float zoomFactor = 1.1f;
    float newScale = m_scale;
    
    if (event->angleDelta().y() > 0) {
        newScale *= zoomFactor;
    } else {
        newScale /= zoomFactor;
    }

    // No artificial min/max zoom limits; only guard against invalid scales.
    if (!std::isfinite(newScale) || newScale <= 0.0f) {
        event->accept();
        return;
    }

    // Calculate new offset to keep mouse position fixed
    // mousePos = offset + imagePos * scale
    // imagePos = (mousePos - offset) / scale
    // newOffset = mousePos - imagePos * newScale
    
    QPointF mousePos = event->position();
    QPointF imagePos = (mousePos - m_offset) / m_scale;
    QPointF newOffset = mousePos - (imagePos * newScale);

    updateTransform(newScale, newOffset);
}

void ZoomableImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ZoomableImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        QPointF delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        updateTransform(m_scale, m_offset + delta);
    }
}

void ZoomableImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
    }
}
