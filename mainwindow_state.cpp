#include "mainwindow_refactored.h"
#include "device_dock.h"
#include "display_dock.h"
#include "gige_dock.h"
#include "app_constants.h"
#include <QSettings>
#include <QWindowStateChangeEvent>
#include <QResizeEvent>
#include <QTimer>

#include <QApplication>

void MainWindowRefactored::closeEvent(QCloseEvent *event)
{
    saveWindowState();
    QMainWindow::closeEvent(event);
}

void MainWindowRefactored::changeEvent(QEvent *event)
{
    if (event && event->type() == QEvent::WindowStateChange) {
        if (uiReady_) {
            // 状态切换后确实需要强制刷新一次布局，因为showNormal()之后Qt可能会恢复之前的几何形状
            // 这个几何形状中左侧dock的宽度可能已经被之前的resizeEvent按比例缩小了
            // 必须在事件循环之后再次强制应用我们的 leftDockTargetWidth_
            QTimer::singleShot(0, this, [this](){
                 // 手动触发一次resizeEvent中的布局逻辑
                 QResizeEvent dummy(size(), size());
                 resizeEvent(&dummy);
            });
        }
    }
    QMainWindow::changeEvent(event);
}

bool MainWindowRefactored::eventFilter(QObject *watched, QEvent *event)
{
    // 监听左侧dock的尺寸变化
    if (watched == deviceDock_ && event->type() == QEvent::Resize) {
        // 只有当用户按住鼠标左键（拖拽）时的Resize才被认为是意图改变布局
        // 这能有效过滤掉窗口缩放、全屏切换等导致的“被动”尺寸变化
        bool isUserInteracting = (QApplication::mouseButtons() & Qt::LeftButton);
        
        if (isUserInteracting && !isWindowResizing_ && !dockStateRestoreInProgress_ && 
            deviceDock_ && !deviceDock_->isFloating() && deviceDock_->isVisible()) {
            
            const int currentW = deviceDock_->width();
            if (currentW > 50 && currentW != leftDockTargetWidth_) {
                 leftDockTargetWidth_ = currentW;
                 // 延迟保存，避免拖拽过程频繁IO
                 static QTimer* saveTimer = nullptr;
                 if (!saveTimer) {
                     saveTimer = new QTimer(this);
                     saveTimer->setSingleShot(true);
                     saveTimer->setInterval(1000);
                     connect(saveTimer, &QTimer::timeout, this, [this](){
                         if (leftDockTargetWidth_ > 50) {
                             QSettings settings("CameraUI", "MainWindow");
                             settings.setValue("leftDockTargetWidth", leftDockTargetWidth_);
                         }
                     });
                 }
                 saveTimer->start();
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindowRefactored::resizeEvent(QResizeEvent *event)
{
    // 标记正在进行窗口级别的缩放，此时屏蔽对Dock宽度的记录
    isWindowResizing_ = true;
    QMainWindow::resizeEvent(event);
    
    // 如果我们有预设的左侧宽度，强制应用它
    // 这保证了无论窗口如何缩放（拖拉、最大化、全屏），左侧列的宽度都保持用户上次设定的值
    if (uiReady_ && !dockStateRestoreInProgress_ && leftDockTargetWidth_ > 50) {
        if (deviceDock_ && displayDock_ && gigeDock_ && 
            !deviceDock_->isFloating() && !displayDock_->isFloating() && !gigeDock_->isFloating()) {
            
            const int w = width();
            const int minRight = 600; // 右侧必须保留的最小区域
            const int rightDockWidth = 250; // GigE
            
            // 如果窗口太小，就不强制保持左侧宽度了，避免界面完全没法用
            if (w > LEFT_DOCK_MIN_WIDTH + rightDockWidth + 100) {
                 const int maxLeft = qMax(LEFT_DOCK_MIN_WIDTH, w - minRight - rightDockWidth);
                 const int left = qBound(LEFT_DOCK_MIN_WIDTH, leftDockTargetWidth_, maxLeft);
                 
                 // 如果当前的宽度和目标宽度偏差超过5像素（允许一点点误差），则强制布局
                 // 这样做避免了每像素的微小变动都触发重排
                 if (qAbs(deviceDock_->width() - left) > 5) {
                     const int center = qMax(1, w - left - rightDockWidth);
                     
                     QList<QDockWidget*> docks;
                     docks << displayDock_ << gigeDock_;
                     QList<int> sizes;
                     sizes << center << rightDockWidth;
                     
                     resizeDocks(docks, sizes, Qt::Horizontal);
                 }
            }
        }
    }
    
    isWindowResizing_ = false;
}

void MainWindowRefactored::saveWindowState()
{
    QSettings settings("CameraUI", "MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    settings.setValue("isMaximized", isMaximized());
    settings.setValue("isFullScreen", isFullScreen());
}

bool MainWindowRefactored::restoreWindowState()
{
    QSettings settings("CameraUI", "MainWindow");
    QByteArray geometry = settings.value("geometry").toByteArray();
    QByteArray savedWindowState = settings.value("windowState").toByteArray();
    const bool wantMaximized = settings.value("isMaximized", true).toBool();
    const bool wantFullScreen = settings.value("isFullScreen", false).toBool();
    
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if (!savedWindowState.isEmpty()) {
        restoreState(savedWindowState);
    }

    if (wantFullScreen) {
        setWindowState(QMainWindow::windowState() | Qt::WindowFullScreen);
    } else if (wantMaximized) {
        setWindowState(QMainWindow::windowState() | Qt::WindowMaximized);
    }

    return !geometry.isEmpty() || !savedWindowState.isEmpty();
}

void MainWindowRefactored::onResetLayoutToDefault()
{
    QSettings settings("CameraUI", "MainWindow");
    settings.remove("geometry");
    settings.remove("windowState");
    settings.remove("dockState_normal");
    settings.remove("dockState_maximized");
    settings.remove("dockState_fullscreen");
    settings.remove("leftDockWidth_normal");
    settings.remove("leftDockWidth_maximized");
    settings.remove("leftDockWidth_fullscreen");
    
    showMaximized();
}
