#ifndef MAINWINDOW_REFACTORED_H
#define MAINWINDOW_REFACTORED_H

#include <QMainWindow>
#include <QDockWidget>
#include <QVariant>
#include <atomic>
#include "device_selector_widget.h"

class QAction;
class QToolBar;
class QWidget;
class QImage;
class QCloseEvent;
class QEvent;
class QResizeEvent;
class QTimer;

class DeviceDock;
class DisplayDock;
class OutputDock;
class ImageDataDock;
class ImageAlgorithmDock;
class GigeDock;
class ThreadManager;
class ICameraDevice;
class GigECameraDevice;
struct ImageFrameData;

class ImageSaveWorker;

class MainWindowRefactored : public QMainWindow
{
    Q_OBJECT

public:
    MainWindowRefactored(QWidget *parent = nullptr);
    ~MainWindowRefactored();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onActionNew();
    void onActionOpen();
    void onActionSave();
    void onActionExit();
    void onResetLayoutToDefault();

    void onSingleCaptureRequested();
    void onContinuousCaptureRequested();
    void onStopCaptureRequested();
    void onSaveSingleImageRequested();
    void onSaveMultipleImagesRequested(int count, const QString &directory, const QString &format);
    void onBufferCountChanged(int count);
    void onFrameReady(const ImageFrameData &frame);
    void onAcquisitionError(const QString &error);
    void onDeviceChanged(const DeviceSelectorWidget::DeviceInfo &info);
    void onConfigurationChanged(const QString &configPath);

private:
    void setupUI();
    void setupPanels();
    void setupMenuAndToolBar();
    void setupTrayIcon();
    void setupConnections();
    void setupAcquisitionConnections();
    void populateAcquisitionDevices();
    void showTransientMessage(const QString &message, int timeoutMs = 2000);
    void updateThemeMenuActions();

    bool saveImageWithFormat(int bufferIndex, const QString &filePath);

    void saveWindowState();
    bool restoreWindowState();

    bool eventFilter(QObject *watched, QEvent *event) override;

    bool uiReady_{false};
    bool dockStateRestoreInProgress_{false};

    bool isWindowResizing_{false};
    int leftDockTargetWidth_{380};

    DeviceDock *deviceDock_;
    DisplayDock *displayDock_;
    GigeDock *gigeDock_;
    OutputDock *outputDock_;
    ImageDataDock *imageDataDock_;
    ImageAlgorithmDock *imageAlgorithmDock_;
    DeviceSelectorWidget *deviceSelectorWidget_;
    QWidget *blankCentral_;
    QToolBar *primaryToolBar_;

    ThreadManager *threadManager_;

    ICameraDevice *cameraDevice_;
    GigECameraDevice *gigeCameraDevice_;

    QThread *imageSaveThread_;
    ImageSaveWorker *imageSaveWorker_;

    QString configDirectory_;

    std::atomic<int> frameCounter_{0};

    std::atomic<int> latestBufferIndex_{-1};
    std::atomic<bool> displayUpdatePending_{false};

    bool isContinuousCaptureActive_;
    bool multiSaveActive_;
    bool multiSaveAutoStop_;
    int multiSaveRemaining_;
    int multiSaveTotal_;
    int multiSaveSavedCount_;
    int multiSaveProcessedCount_;
    quint64 multiSaveBatchId_;
    QString multiSaveDirectory_;
    QString multiSaveBaseName_;
    QString multiSaveFormat_;
};

#endif // MAINWINDOW_REFACTORED_H
