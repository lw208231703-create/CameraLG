#include "mainwindow_refactored.h"
#include "thread_manager.h"
#include "device_dock.h"
#include "display_dock.h"
#include "gige_dock.h"
#include "output_dock.h"
#include "image_data_dock.h"
#include "image_algorithm_dock.h"
#include "device_selector_widget.h"
#include "ICameraDevice.h"
#include "app_constants.h"
#include <QStatusBar>
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

void MainWindowRefactored::setupUI()
{
    blankCentral_ = new QWidget(this);
    blankCentral_->setObjectName("blankCentral");
    setCentralWidget(blankCentral_);
    blankCentral_->setContentsMargins(0, 0, 0, 0);
    blankCentral_->hide();

    setupPanels();
    setupMenuAndToolBar();
}

void MainWindowRefactored::setupPanels()
{
    displayDock_ = new DisplayDock(this);
    displayDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, displayDock_);

    connect(displayDock_, &DisplayDock::displayRefreshRateChanged,
            displayDock_, &DisplayDock::setDisplayRefreshRate);

    gigeDock_ = new GigeDock(gigeCameraDevice_, this);
    gigeDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, gigeDock_);
    gigeDock_->hide();

    connect(displayDock_, &DisplayDock::bitShiftChanged,
            gigeDock_, &GigeDock::setBitShift);

    connect(displayDock_, &DisplayDock::gigeWindowToggled,
            this, [this](bool visible) {
        if (gigeDock_) {
            gigeDock_->setVisible(visible);
        }
    });

    deviceDock_ = new DeviceDock(this);
    deviceDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, deviceDock_);

    imageDataDock_ = new ImageDataDock(threadManager_, this);
    imageDataDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, imageDataDock_);
    if (cameraDevice_) {
        imageDataDock_->setHistorySize(cameraDevice_->getBufferCount());
    }

    imageAlgorithmDock_ = new ImageAlgorithmDock(threadManager_, this);
    imageAlgorithmDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, imageAlgorithmDock_);

    connect(displayDock_, &DisplayDock::roiChanged, imageDataDock_, &ImageDataDock::setRoi);
    connect(displayDock_, &DisplayDock::pinnedPointChanged, imageDataDock_, &ImageDataDock::setPinnedPoint);

    outputDock_ = new OutputDock(this);
    outputDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::BottomDockWidgetArea, outputDock_);

    splitDockWidget(displayDock_, gigeDock_, Qt::Horizontal);

    splitDockWidget(deviceDock_, imageDataDock_, Qt::Vertical);
    splitDockWidget(imageDataDock_, imageAlgorithmDock_, Qt::Vertical);

    splitDockWidget(displayDock_, outputDock_, Qt::Vertical);

    deviceSelectorWidget_ = deviceDock_->selector();

    if (deviceDock_) {
        deviceDock_->installEventFilter(this);
    }
}

void MainWindowRefactored::setupConnections()
{
    connect(deviceSelectorWidget_, &DeviceSelectorWidget::deviceChanged,
            this, [this](const DeviceSelectorWidget::DeviceInfo &info) {
        showTransientMessage(tr("Selected device: %1").arg(info.name), 4000);
    });

    if (imageAlgorithmDock_) {
        connect(imageAlgorithmDock_, &ImageAlgorithmDock::diagnosticMessage,
                this, [this](const QString &msg) {
            if (outputDock_) outputDock_->appendDiagnostic(msg);
        });
    }
}

void MainWindowRefactored::showTransientMessage(const QString &message, int timeoutMs)
{
    if (auto *bar = statusBar()) {
        bar->showMessage(message, timeoutMs);
    }
}
