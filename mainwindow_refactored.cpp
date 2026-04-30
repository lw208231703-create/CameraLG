#include "icon_cache.h"
#include "mainwindow_refactored.h"
#include "thread_manager.h"
#include "device_dock.h"
#include "display_dock.h"
#include "gige_dock.h"
#include "output_dock.h"
#include "image_data_dock.h"
#include "image_algorithm_dock.h"
#include "device_selector_widget.h"
#include "CameraFactory.h"
#include "sapera_camera_device.h"
#include "gige_camera_device.h"
#include "virtual_camera_device.h"
#include "image_save_worker.h"
#include "app_constants.h"
#include <QStatusBar>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QMessageBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QDataStream>
#include <QImageWriter>
#include <QSettings>
#include <QCloseEvent>
#include <QEvent>
#include <QWindowStateChangeEvent>
#include <QResizeEvent>

MainWindowRefactored::MainWindowRefactored(QWidget *parent)
    : QMainWindow(parent)
    , deviceDock_(nullptr)
    , displayDock_(nullptr)
    , outputDock_(nullptr)
    , imageDataDock_(nullptr)
    , imageAlgorithmDock_(nullptr)
    , deviceSelectorWidget_(nullptr)
    , blankCentral_(nullptr)
    , primaryToolBar_(nullptr)
    , threadManager_(nullptr)
    , cameraDevice_(nullptr)
    , gigeCameraDevice_(nullptr)
    , imageSaveThread_(nullptr)
    , imageSaveWorker_(nullptr)
    , configDirectory_("")
    , isContinuousCaptureActive_(false)
    , multiSaveActive_(false)
    , multiSaveAutoStop_(false)
    , multiSaveRemaining_(0)
    , multiSaveTotal_(0)
    , multiSaveSavedCount_(0)
    , multiSaveProcessedCount_(0)
    , multiSaveBatchId_(0)
{
    setWindowTitle(tr("Camera Link Frame"));
    setWindowIcon(IconCache::applicationIcon());

    setDockNestingEnabled(true);

    threadManager_ = new ThreadManager(this);
    threadManager_->startAllThreads();

    CameraFactory::registerMetaTypes();
    cameraDevice_ = CameraFactory::createCamera(CameraFactory::CameraType::CameraLink_Sapera, this);
    if (!cameraDevice_) {
        cameraDevice_ = CameraFactory::createCamera(CameraFactory::CameraType::Virtual_Test, this);
    }

    gigeCameraDevice_ = new GigECameraDevice(this);

    imageSaveThread_ = threadManager_->imageSaveThread();
    imageSaveWorker_ = threadManager_->imageSaveWorker();

    connect(imageSaveWorker_, &ImageSaveWorker::saveFinished, this, [this](const QString &filePath, bool success) {
        multiSaveProcessedCount_++;

        if (success) {
            multiSaveSavedCount_++;
        }

        if (multiSaveProcessedCount_ >= multiSaveTotal_) {
            multiSaveActive_ = false;
            showTransientMessage(tr("Saved %1 images to %2").arg(multiSaveSavedCount_).arg(multiSaveDirectory_), 4000);

            if (multiSaveAutoStop_) {
                cameraDevice_->stopGrabbing();
            }
            multiSaveAutoStop_ = false;
        }
    });

    threadManager_->startAllThreads();

    setupUI();
    setupConnections();
    setupAcquisitionConnections();
    populateAcquisitionDevices();

    const bool stateRestored = restoreWindowState();

    if (outputDock_) outputDock_->setVisible(true);
    if (deviceDock_) deviceDock_->setVisible(true);
    if (imageDataDock_) imageDataDock_->setVisible(true);
    if (imageAlgorithmDock_) imageAlgorithmDock_->setVisible(true);
    if (displayDock_) displayDock_->setVisible(true);

    if (!stateRestored) {
        showMaximized();

    QTimer::singleShot(0, this, [this]() {
        int w = width() > 0 ? width() : 2560;
        int h = height() > 0 ? height() : 1440;

        int leftWidth = qMax(LEFT_DOCK_MIN_WIDTH, int(w * 0.12));
        int rightWidth = 250;
        int centerWidth = qMax(600, w - leftWidth - rightWidth);

        QList<QDockWidget*> horizontalDocks;
        horizontalDocks << displayDock_ << gigeDock_;
        QList<int> horizontalSizes;
        horizontalSizes << centerWidth << rightWidth;
        resizeDocks(horizontalDocks, horizontalSizes, Qt::Horizontal);

        int outputHeight = qMax(100, int(h * 0.08));
        QList<QDockWidget*> verticalDocks;
        verticalDocks << displayDock_ << outputDock_;
        QList<int> verticalSizes;
        verticalSizes << (h - outputHeight) << outputHeight;
        resizeDocks(verticalDocks, verticalSizes, Qt::Vertical);

        int deviceHeight = DEVICE_DOCK_MIN_HEIGHT;
        int imageDataHeight = IMAGE_DATA_DOCK_MIN_HEIGHT;
        int imageAlgorithmHeight = IMAGE_ALGORITHM_DOCK_MIN_HEIGHT;

        resizeDocks({deviceDock_, imageDataDock_, imageAlgorithmDock_},
                   {deviceHeight, imageDataHeight, imageAlgorithmHeight}, Qt::Vertical);
    });
    }

    uiReady_ = true;

    QSettings settings("CameraUI", "MainWindow");
    leftDockTargetWidth_ = settings.value("leftDockTargetWidth", 380).toInt();
    if (leftDockTargetWidth_ <= 50) leftDockTargetWidth_ = 380;
}

MainWindowRefactored::~MainWindowRefactored()
{
    saveWindowState();

    if (cameraDevice_) {
        cameraDevice_->stopGrabbing();
    }

    if (imageSaveWorker_) {
        imageSaveWorker_->stop();
    }
    if (imageSaveThread_) {
        imageSaveThread_->quit();
        imageSaveThread_->wait();
    }

    threadManager_->stopAllThreads();
}
