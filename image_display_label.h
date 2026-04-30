#ifndef IMAGE_DISPLAY_LABEL_H
#define IMAGE_DISPLAY_LABEL_H

#include <QLabel>
#include <QImage>
#include <QRect>
#include <QPoint>
#include <QVector>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QEvent>

class ImageDisplayLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ImageDisplayLabel(QWidget *parent = nullptr);

    void setSourceImage(const QImage &image);
    void setPinnedPoint(const QPoint &point, bool pinned);
    void setMarkerPoints(const QVector<QPoint> &points, bool visible);
    void setRoiRect(const QRect &rect, bool active);
    QRect getRoiRect() const { return roiRect_; }
    bool isRoiActive() const { return isRoiActive_; }
    void setTrackingCursors(int validity, float x1, float y1, float x2, float y2, float x3, float y3);

signals:
    void wheelZoomRequested(int delta);
    void mouseMoved(const QPoint &pos);
    void mouseLeft();
    void mouseClicked(const QPoint &pos);
    void mouseDoubleClicked(const QPoint &pos);
    void roiChanged(const QRect &rect);
    void panRequested(const QPoint &delta);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    enum Handle { None, TopLeft, Top, TopRight, Left, Right, BottomLeft, Bottom, BottomRight, Body };
    Handle getHandleAt(const QPoint &pos) const;
    void updateCursor(const QPoint &pos);
    QRect getHandleRect(int x, int y) const;

    QPoint pinnedPoint_;
    bool isPinned_ = false;

    QVector<QPoint> markerPoints_;
    bool markerVisible_ = false;

    QImage sourceImage_;

    QRect roiRect_;
    bool isRoiActive_ = false;
    Handle currentHandle_ = None;
    QPoint lastMousePos_;
    QPoint lastGlobalMousePos_;
    QPoint dragStartGlobalPos_;
    bool isDragging_ = false;
    bool isPanning_ = false;

    struct TrackingCursor {
        bool visible = false;
        int validity = 0;
        float x[3] = {0.0f, 0.0f, 0.0f};  // 3个光斑的X坐标
        float y[3] = {0.0f, 0.0f, 0.0f};  // 3个光斑的Y坐标
    } trackingCursor_;
};

#endif // IMAGE_DISPLAY_LABEL_H
