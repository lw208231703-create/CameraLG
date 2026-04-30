#include "image_display_label.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QEvent>
#include <QFontMetrics>

ImageDisplayLabel::ImageDisplayLabel(QWidget *parent)
    : QLabel(parent)
{
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
}

void ImageDisplayLabel::setSourceImage(const QImage &image)
{
    sourceImage_ = image;
    update();
}

void ImageDisplayLabel::setPinnedPoint(const QPoint &point, bool pinned)
{
    pinnedPoint_ = point;
    isPinned_ = pinned;
    update();
}

void ImageDisplayLabel::setMarkerPoints(const QVector<QPoint> &points, bool visible)
{
    markerPoints_ = points;
    markerVisible_ = visible;
    update();
}

void ImageDisplayLabel::setRoiRect(const QRect &rect, bool active)
{
    roiRect_ = rect;
    isRoiActive_ = active;
    update();
}

void ImageDisplayLabel::wheelEvent(QWheelEvent *event)
{
    emit wheelZoomRequested(event->angleDelta().y());
    event->accept();
}

void ImageDisplayLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (isRoiActive_ && isDragging_) {
        QPoint delta = event->pos() - lastMousePos_;
        QRect newRect = roiRect_;

        switch (currentHandle_) {
            case TopLeft:
                newRect.setTopLeft(newRect.topLeft() + delta);
                break;
            case Top:
                newRect.setTop(newRect.top() + delta.y());
                break;
            case TopRight:
                newRect.setTopRight(newRect.topRight() + delta);
                break;
            case Left:
                newRect.setLeft(newRect.left() + delta.x());
                break;
            case Right:
                newRect.setRight(newRect.right() + delta.x());
                break;
            case BottomLeft:
                newRect.setBottomLeft(newRect.bottomLeft() + delta);
                break;
            case Bottom:
                newRect.setBottom(newRect.bottom() + delta.y());
                break;
            case BottomRight:
                newRect.setBottomRight(newRect.bottomRight() + delta);
                break;
            case Body:
                newRect.translate(delta);
                break;
            default:
                break;
        }

        roiRect_ = newRect.normalized();
        lastMousePos_ = event->pos();
        emit roiChanged(roiRect_);
        update();
    } else if (isPanning_) {
        QPoint delta = event->globalPosition().toPoint() - lastGlobalMousePos_;
        lastGlobalMousePos_ = event->globalPosition().toPoint();
        emit panRequested(delta);
    } else {
        if (isRoiActive_) {
            updateCursor(event->pos());
        }
        emit mouseMoved(event->pos());
    }
    QLabel::mouseMoveEvent(event);
}

void ImageDisplayLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (isRoiActive_) {
            currentHandle_ = getHandleAt(event->pos());
            if (currentHandle_ != None) {
                isDragging_ = true;
                lastMousePos_ = event->pos();
                return;
            }
        }
        
        isPanning_ = true;
        lastGlobalMousePos_ = event->globalPosition().toPoint();
        dragStartGlobalPos_ = lastGlobalMousePos_;
        setCursor(Qt::ClosedHandCursor);
    }
    QLabel::mousePressEvent(event);
}

void ImageDisplayLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging_ = false;
        currentHandle_ = None;
        
        if (isPanning_) {
            isPanning_ = false;
            setCursor(Qt::CrossCursor);
            
            QPoint currentGlobalPos = event->globalPosition().toPoint();
            if ((currentGlobalPos - dragStartGlobalPos_).manhattanLength() < 3) {
                emit mouseClicked(event->pos());
            }
        }
    }
    QLabel::mouseReleaseEvent(event);
}

void ImageDisplayLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit mouseDoubleClicked(event->pos());
    }
    QLabel::mouseDoubleClickEvent(event);
}

void ImageDisplayLabel::leaveEvent(QEvent *event)
{
    emit mouseLeft();
    QLabel::leaveEvent(event);
}

void ImageDisplayLabel::enterEvent(QEnterEvent *event)
{
    setCursor(Qt::CrossCursor);
    QLabel::enterEvent(event);
}

void ImageDisplayLabel::paintEvent(QPaintEvent *event)
{
    if (!sourceImage_.isNull()) {
        QPainter painter(this);
        painter.setClipRect(event->rect());
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

        const int dstW = width();
        const int dstH = height();
        if (dstW > 0 && dstH > 0) {
            const double scaleX = static_cast<double>(sourceImage_.width()) / static_cast<double>(dstW);
            const double scaleY = static_cast<double>(sourceImage_.height()) / static_cast<double>(dstH);

            const QRect dstRect = event->rect();
            QRectF srcRect(
                dstRect.x() * scaleX,
                dstRect.y() * scaleY,
                dstRect.width() * scaleX,
                dstRect.height() * scaleY
            );

            const QRectF imgBounds(0.0, 0.0, sourceImage_.width(), sourceImage_.height());
            srcRect = srcRect.intersected(imgBounds);

            if (!srcRect.isEmpty()) {
                painter.drawImage(QRectF(dstRect), sourceImage_, srcRect);
            }
        }

        painter.setClipping(false);
        painter.setRenderHint(QPainter::Antialiasing);

        if (isPinned_) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 107, 107));
            
            const double scaleX = static_cast<double>(width()) / static_cast<double>(sourceImage_.width());
            const double scaleY = static_cast<double>(height()) / static_cast<double>(sourceImage_.height());
            
            QRectF pixelRect(
                pinnedPoint_.x() * scaleX,
                pinnedPoint_.y() * scaleY,
                scaleX,
                scaleY
            );
            
            painter.drawRect(pixelRect);
        }

        if (markerVisible_ && !markerPoints_.isEmpty()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 214, 102));

            const double scaleX = static_cast<double>(width()) / static_cast<double>(sourceImage_.width());
            const double scaleY = static_cast<double>(height()) / static_cast<double>(sourceImage_.height());

            for (const QPoint &pt : markerPoints_) {
                if (pt.x() < 0 || pt.y() < 0 || pt.x() >= sourceImage_.width() || pt.y() >= sourceImage_.height()) {
                    continue;
                }

                QRectF pixelRect(
                    pt.x() * scaleX,
                    pt.y() * scaleY,
                    scaleX,
                    scaleY
                );
                painter.drawRect(pixelRect);
            }
        }

        if (isRoiActive_) {
            painter.setPen(QPen(QColor(102, 187, 106), 2, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(roiRect_);

            painter.setPen(QColor(102, 187, 106));
            painter.setBrush(QColor(220, 220, 220));
            int handleSize = 6;

            auto drawHandle = [&](const QPoint &p) {
                painter.drawRect(p.x() - handleSize/2, p.y() - handleSize/2, handleSize, handleSize);
            };

            drawHandle(roiRect_.topLeft());
            drawHandle(QPoint(roiRect_.center().x(), roiRect_.top()));
            drawHandle(roiRect_.topRight());
            drawHandle(QPoint(roiRect_.left(), roiRect_.center().y()));
            drawHandle(QPoint(roiRect_.right(), roiRect_.center().y()));
            drawHandle(roiRect_.bottomLeft());
            drawHandle(QPoint(roiRect_.center().x(), roiRect_.bottom()));
            drawHandle(roiRect_.bottomRight());
        }

        if (trackingCursor_.visible && trackingCursor_.validity > 0) {
            // 绘制多个光斑（根据validity数量）
            int spotCount = trackingCursor_.validity;
            QColor spotColors[3] = {QColor(255, 107, 107), QColor(102, 187, 106), QColor(66, 165, 245)};
            
            double scaleX = static_cast<double>(width()) / static_cast<double>(sourceImage_.width());
            double scaleY = static_cast<double>(height()) / static_cast<double>(sourceImage_.height());
            
            for (int i = 0; i < spotCount && i < 3; ++i) {
                double targetImageX = trackingCursor_.x[i];
                double targetImageY = trackingCursor_.y[i];
                
                double screenX = targetImageX * scaleX;
                double screenY = targetImageY * scaleY;
                
                QColor cursorColor = spotColors[i];
                
                // 绘制贯穿图像的十字线
                painter.setPen(QPen(cursorColor, 1, Qt::SolidLine));
                painter.drawLine(0, static_cast<int>(screenY), width(), static_cast<int>(screenY));
                painter.drawLine(static_cast<int>(screenX), 0, static_cast<int>(screenX), height());
                
                // 绘制圆圈
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(QPointF(screenX, screenY), 10, 10);
                
                // 绘制标签
                QString labelText = QString("S%1: X:%2, Y:%3")
                    .arg(i + 1)
                    .arg(trackingCursor_.x[i], 0, 'f', 1)
                    .arg(trackingCursor_.y[i], 0, 'f', 1);
                
                QFontMetrics fm(painter.font());
                int textWidth = fm.horizontalAdvance(labelText);
                int textHeight = fm.height();
                int offsetY = i * (textHeight + 8);  // 多个标签垂直排列
                QRect textRect(static_cast<int>(screenX) + 15, static_cast<int>(screenY) - 15 + offsetY, 
                              textWidth + 10, textHeight + 6);
                
                // 边界检查
                if (textRect.right() > width()) textRect.moveRight(width() - 5);
                if (textRect.bottom() > height()) textRect.moveBottom(height() - 5);
                if (textRect.top() < 0) textRect.moveTop(5);
                if (textRect.left() < 0) textRect.moveLeft(5);
                
                painter.fillRect(textRect, QColor(0, 0, 0, 160));
                painter.setPen(Qt::white);
                painter.drawText(textRect, Qt::AlignCenter, labelText);
            }
        }

        return;
    }

    QLabel::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (isPinned_) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 107, 107));
        painter.drawEllipse(pinnedPoint_.x() - 2, pinnedPoint_.y() - 2, 4, 4);
    }

    if (markerVisible_ && !markerPoints_.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 214, 102));
        for (const QPoint &pt : markerPoints_) {
            painter.drawEllipse(pt.x() - 2, pt.y() - 2, 4, 4);
        }
    }

    if (isRoiActive_) {
        painter.setPen(QPen(QColor(102, 187, 106), 2, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(roiRect_);

        painter.setPen(QColor(102, 187, 106));
        painter.setBrush(QColor(220, 220, 220));
        int handleSize = 6;
        
        auto drawHandle = [&](const QPoint &p) {
            painter.drawRect(p.x() - handleSize/2, p.y() - handleSize/2, handleSize, handleSize);
        };

        drawHandle(roiRect_.topLeft());
        drawHandle(QPoint(roiRect_.center().x(), roiRect_.top()));
        drawHandle(roiRect_.topRight());
        drawHandle(QPoint(roiRect_.left(), roiRect_.center().y()));
        drawHandle(QPoint(roiRect_.right(), roiRect_.center().y()));
        drawHandle(roiRect_.bottomLeft());
        drawHandle(QPoint(roiRect_.center().x(), roiRect_.bottom()));
        drawHandle(roiRect_.bottomRight());
    }
}

void ImageDisplayLabel::setTrackingCursors(int validity, float x1, float y1, float x2, float y2, float x3, float y3)
{
    trackingCursor_.validity = validity;
    trackingCursor_.x[0] = x1;
    trackingCursor_.y[0] = y1;
    trackingCursor_.x[1] = x2;
    trackingCursor_.y[1] = y2;
    trackingCursor_.x[2] = x3;
    trackingCursor_.y[2] = y3;
    trackingCursor_.visible = (validity > 0);
    update();
}

ImageDisplayLabel::Handle ImageDisplayLabel::getHandleAt(const QPoint &pos) const
{
    int handleSize = 8;
    auto check = [&](const QPoint &p) {
        return QRect(p.x() - handleSize/2, p.y() - handleSize/2, handleSize, handleSize).contains(pos);
    };

    if (check(roiRect_.topLeft())) return TopLeft;
    if (check(QPoint(roiRect_.center().x(), roiRect_.top()))) return Top;
    if (check(roiRect_.topRight())) return TopRight;
    if (check(QPoint(roiRect_.left(), roiRect_.center().y()))) return Left;
    if (check(QPoint(roiRect_.right(), roiRect_.center().y()))) return Right;
    if (check(roiRect_.bottomLeft())) return BottomLeft;
    if (check(QPoint(roiRect_.center().x(), roiRect_.bottom()))) return Bottom;
    if (check(roiRect_.bottomRight())) return BottomRight;
    
    if (roiRect_.contains(pos)) return Body;

    return None;
}

void ImageDisplayLabel::updateCursor(const QPoint &pos)
{
    Handle handle = getHandleAt(pos);
    switch (handle) {
        case TopLeft:
        case BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case TopRight:
        case BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case Top:
        case Bottom:
            setCursor(Qt::SizeVerCursor);
            break;
        case Left:
        case Right:
            setCursor(Qt::SizeHorCursor);
            break;
        case Body:
            setCursor(Qt::SizeAllCursor);
            break;
        default:
            setCursor(Qt::CrossCursor);
            break;
    }
}
