#ifndef DISPLAY_DOCK_H
#define DISPLAY_DOCK_H

#include "build_config.h"
#include "image_info_worker.h"
#include "app_constants.h"

#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QSize>
#include <QThread>
#include <QMutex>
#include <QElapsedTimer>
#include <QSlider>
#include <QVector>
#include <atomic>

class QVBoxLayout;
class QWheelEvent;
class QScrollArea;
class QMouseEvent;
class ImageInfoWorker;
class QTimer;
class BitDepthSlider;
class ImageDisplayLabel;
class EditableNumberLabel;

class DisplayDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit DisplayDock(QWidget *parent = nullptr);
    ~DisplayDock() override;

    QVBoxLayout *contentLayout() const;
    
    // 图像显示功能
    void displayImage(const QImage &image);
    // 设置原始数据用于信息显示
    void setRawData(const QVector<uint16_t> &data, int width, int height, int bitDepth);
    
    void setBufferCount(int count);
    int getCurrentBufferIndex() const;
    void setCurrentBufferIndex(int index);
    
    // 获取当前原始图像（用于保存）
    QImage getCurrentImage() const { return currentImage_; }
    
    // 获取第一个像素值（用于文件命名）
    int getFirstPixelValue() const { return firstPixelValue_; }

    // 设置脱靶量光标
    void setTrackingCursors(int validity, float x1, float y1, float x2, float y2, float x3, float y3);
    
    // 设置图像显示刷新帧率（0表示不限制，否则为目标FPS）
    void setDisplayRefreshRate(int fps);

    // 坏点批量拾取模式：启用后，单击图像会发出 badPixelPointPicked，且不再影响单点 Pin/ROI。
    void setBadPixelPickMode(bool enabled);
    bool isBadPixelPickMode() const { return badPixelPickMode_; }
    void setBadPixelMarkers(const QVector<QPoint> &points);
    void clearBadPixelMarkers();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    
public slots:
    // 移除 onImageReady，逻辑移至 MainWindow 处理
    void setContinuousCaptureState(bool capturing);

signals:
    void singleCaptureRequested();
    void continuousCaptureRequested();
    void saveSingleImageRequested();
    void saveContinuousImageRequested();
    void saveMultipleImagesRequested(int count, const QString &directory, const QString &format);
    void bufferIndexChanged(int index);
    void bufferCountChanged(int count);
    void bitShiftChanged(int shift);
    void stopCaptureRequested();
    void roiChanged(const QRect &rect, bool active);
    void pinnedPointChanged(const QPoint &point, bool active);
    void badPixelPointPicked(const QPoint &imagePos);
    void trackingRefreshRateChanged(int fps);
    void displayRefreshRateChanged(int fps);
    void gigeWindowToggled(bool visible);

private slots:
    void onBufferSliderChanged(int value);
    void onBufferCountSliderChanged(int value);
    void onBitShiftSliderChanged(int value);
    void onViewSettingsButtonClicked();
    void onWheelZoomRequested(int delta);
    void onMouseMoved(const QPoint &pos);
    void onMouseLeft();
    void onMouseClicked(const QPoint &pos);
    void onMouseDoubleClicked(const QPoint &pos);
    void onRoiChanged(const QRect &rect);
    void onPanRequested(const QPoint &delta);
    void onImageInfoUpdated(const ImageInfo &info);
    void onCaptureToggleClicked();
    void onClickTimeout();

private:
    void setupUI();
    void updateDisplayedPixmap();
    QSize activeBaseSize() const;
    void updateImageInfoDisplay();
    void requestImageInfoUpdate();
    QString getColorTypeString(const QImage &image) const;
    void updateButtonState();
    void showFpsSettingsDialog();
    
    // Coordinate mapping helpers
    QPoint mapToImage(const QPoint &widgetPos) const;
    QPoint mapFromImage(const QPoint &imagePos) const;

    QWidget *contentWidget_ = nullptr;
    QVBoxLayout *contentLayout_ = nullptr;
    ImageDisplayLabel *imageLabel_ = nullptr;
    QScrollArea *imageScrollArea_ = nullptr;
    QSlider *bufferSlider_ = nullptr;
    QSlider *bufferCountSlider_ = nullptr;
    BitDepthSlider *bitShiftSlider_ = nullptr;
    QLabel *bufferInfoLabel_ = nullptr;
    EditableNumberLabel *bufferCountLabel_ = nullptr;
    
    // 图像信息状态栏
    QWidget *imageInfoBar_ = nullptr;
    QLabel *firstPixelLabel_ = nullptr;
    QLabel *mousePosLabel_ = nullptr;
    QLabel *pixelValueLabel_ = nullptr;
    QLabel *fpsLabel_ = nullptr;
    QLabel *imageSizeLabel_ = nullptr;
    QLabel *colorTypeLabel_ = nullptr;
    QLabel *bitDepthLabel_ = nullptr;
    
    // 图像信息工作线程
    QThread *infoThread_ = nullptr;
    ImageInfoWorker *infoWorker_ = nullptr;
    QPixmap backgroundPixmap_;

    // 连续采集时的显示节流：避免放大状态下每帧重绘导致 UI 卡死
    QTimer *renderTimer_ = nullptr;
    qint64 lastUiRenderMs_ = 0;
    bool renderPending_ = false;
    
    int bufferCount_ = 2;
    int currentBufferIndex_ = 0;
    // 移除 cachedImages_，不再在 DisplayDock 中缓存图像
    QPixmap currentPixmap_;
    QImage currentImage_;           // 当前原始图像
    QSize customDisplaySize_;
    const QSize defaultDisplaySize_{640, 512};
    double scaleFactor_ = 1.0;
    
    // 图像信息相关
    QPoint currentMousePos_{-1, -1};
    QPoint pinnedImagePos_{-1, -1};
    bool isPinned_ = false;
    int firstPixelValue_ = 0;

    bool badPixelPickMode_ = false;

    // 单/双击处理
    class QTimer *clickTimer_ = nullptr;
    QPoint pendingClickPos_ = QPoint(-1, -1);
    bool hasPendingClick_ = false;
    bool ignorePendingClick_ = false;
    
    // ROI related
    QRect currentRoi_;
    bool isRoiActive_ = false;

    // 原始数据缓存
    QVector<uint16_t> currentRawData_;
    int currentRawWidth_ = 0;
    int currentRawHeight_ = 0;
    int currentBitDepth_ = 0;
    
    // FPS计算
    QElapsedTimer fpsTimer_;
    int frameCount_ = 0;
    double currentFps_ = 0.0;
    qint64 lastFpsUpdate_ = 0;
    
    // 图像显示刷新控制
    int displayRefreshIntervalMs_;  // 显示刷新间隔（毫秒），0表示不限制
    static constexpr int MIN_DISPLAY_INTERVAL_MS = 3;  // 最小间隔约360Hz
    
    // 视图设置相关
    double displayPercentage_ = 100.0;  // 显示百分比
    
    // 连续捕获状态
    bool isContinuousCapturing_ = false;
    QToolButton *captureToggleBtn_ = nullptr;
    QToolButton *singleCaptureBtn_ = nullptr;
    QToolButton *gigeToggleBtn_ = nullptr;
    bool gigeWindowVisible_ = false;
};

// 注册自定义类型以便跨线程传递
Q_DECLARE_METATYPE(ImageInfo)

#endif // DISPLAY_DOCK_H
