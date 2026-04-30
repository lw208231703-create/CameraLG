#include "mainwindow_refactored.h"
#include "thread_manager.h"
#include "sapera_camera_device.h"
#include "gige_camera_device.h"
#include "ImageFrameData.h"
#include "image_save_worker.h"
#include "display_dock.h"
#include "device_selector_widget.h"
#include "image_data_dock.h"
#include "image_algorithm_dock.h"
#include "output_dock.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QDataStream>
#include <QSettings>
#include <QImageWriter>

void MainWindowRefactored::setupAcquisitionConnections()
{
    if (!displayDock_ || !cameraDevice_) {
        return;
    }

    connect(displayDock_, &DisplayDock::singleCaptureRequested,
            this, &MainWindowRefactored::onSingleCaptureRequested);
    connect(displayDock_, &DisplayDock::continuousCaptureRequested,
            this, &MainWindowRefactored::onContinuousCaptureRequested);
    connect(displayDock_, &DisplayDock::stopCaptureRequested,
            this, &MainWindowRefactored::onStopCaptureRequested);
    connect(displayDock_, &DisplayDock::saveSingleImageRequested,
            this, &MainWindowRefactored::onSaveSingleImageRequested);
    connect(displayDock_, &DisplayDock::saveMultipleImagesRequested,
            this, &MainWindowRefactored::onSaveMultipleImagesRequested);
    connect(displayDock_, &DisplayDock::bufferCountChanged,
            this, &MainWindowRefactored::onBufferCountChanged);

    connect(displayDock_, &DisplayDock::bufferIndexChanged,
            this, [this](int index) {
        if (cameraDevice_ && displayDock_) {
            int w = 0, h = 0, bd = 0;
            QVector<uint16_t> rawData = cameraDevice_->getRawData(index, w, h, bd);
            QImage img = cameraDevice_->getDisplayImage(index);

            displayDock_->setRawData(rawData, w, h, bd);
            displayDock_->displayImage(img);

            if (imageAlgorithmDock_) {
                imageAlgorithmDock_->setBitDepth(bd);
                if (!rawData.isEmpty()) {
                    imageAlgorithmDock_->onRawImageReceived(rawData, w, h, bd);
                }
            }

            if (imageDataDock_ && !imageDataDock_->isSinglePixelHistoryMode()) {
#if ANALYZE_RAW_DATA
                if (!rawData.isEmpty()) {
                    imageDataDock_->updateRawData(rawData, w, h, bd);
                }
#else
                if (!img.isNull()) {
                    imageDataDock_->updateImage(img);
                }
#endif
            }
        }
    });

    connect(displayDock_, &DisplayDock::bitShiftChanged,
            this, [this](int shift) {
        if (cameraDevice_) {
            cameraDevice_->setBitShift(shift);
            if (imageAlgorithmDock_) {
                imageAlgorithmDock_->setBitShift(shift);
            }
            if (displayDock_) {
                int idx = displayDock_->getCurrentBufferIndex();
                displayDock_->displayImage(cameraDevice_->getDisplayImage(idx));
            }
        }
    });

    // New: connect to ICameraDevice::frameReady (replaces Image_Acquisition::imageReady)
    connect(cameraDevice_, &ICameraDevice::frameReady,
            this, [this](const ImageFrameData& frame) {
        latestBufferIndex_.store(static_cast<int>(frame.frameIndex), std::memory_order_release);
        if (!displayUpdatePending_.load(std::memory_order_acquire)) {
            displayUpdatePending_.store(true, std::memory_order_release);
            QMetaObject::invokeMethod(this, [this, frame]() {
                displayUpdatePending_.store(false, std::memory_order_release);
                onFrameReady(frame);
            }, Qt::QueuedConnection);
        }
    }, Qt::DirectConnection);

    connect(cameraDevice_, &ICameraDevice::deviceError,
            this, &MainWindowRefactored::onAcquisitionError, Qt::QueuedConnection);
    connect(cameraDevice_, &ICameraDevice::grabbingStarted,
            this, [this]() {
        isContinuousCaptureActive_ = true;
        showTransientMessage(tr("Acquisition started"), 2000);
        if (displayDock_) {
            displayDock_->setContinuousCaptureState(true);
        }
    }, Qt::QueuedConnection);
    connect(cameraDevice_, &ICameraDevice::grabbingStopped,
            this, [this]() {
        isContinuousCaptureActive_ = false;
        showTransientMessage(tr("Acquisition stopped"), 2000);
        if (displayDock_) {
            displayDock_->setContinuousCaptureState(false);
        }
    }, Qt::QueuedConnection);

    if (deviceSelectorWidget_) {
        connect(deviceSelectorWidget_, &DeviceSelectorWidget::deviceChanged,
                this, &MainWindowRefactored::onDeviceChanged);
        connect(deviceSelectorWidget_, &DeviceSelectorWidget::configurationChanged,
                this, &MainWindowRefactored::onConfigurationChanged);
        connect(deviceSelectorWidget_, &DeviceSelectorWidget::configDirectoryChanged,
                this, [this](const QString& directory) {
            configDirectory_ = directory;
        });
    }
}

void MainWindowRefactored::populateAcquisitionDevices()
{
    if (!deviceSelectorWidget_) return;

    if (deviceSelectorWidget_->getCurrentDeviceIndex() >= 0) {
        QString current = deviceSelectorWidget_->getCurrentDeviceName();
        if (!current.isEmpty() && current != SaperaCameraDevice::NoServerFoundMessage)
            return;
    }

    QStringList servers = SaperaCameraDevice::getAvailableServers();

    QVector<DeviceSelectorWidget::DeviceInfo> devices;
    for (const QString& server : servers) {
        DeviceSelectorWidget::DeviceInfo info;
        info.name = server;
        info.interfaceType = QStringLiteral("Sapera LT");
        devices.append(info);
    }

    deviceSelectorWidget_->setDevices(devices);

    QString defaultConfigDir = QCoreApplication::applicationDirPath() + "/cfg";
    QDir dir;
    if (!dir.exists(defaultConfigDir))
        dir.mkpath(defaultConfigDir);

    configDirectory_ = defaultConfigDir;
    deviceSelectorWidget_->setConfigDirectory(defaultConfigDir);

    QStringList configs = SaperaCameraDevice::getConfigurationFiles(configDirectory_);
    deviceSelectorWidget_->setConfigurations(configs);

    // Set initial selection on Sapera device
    auto* saperaDev = dynamic_cast<SaperaCameraDevice*>(cameraDevice_);
    if (saperaDev) {
        if (!devices.isEmpty() && !devices.first().name.isEmpty() &&
            devices.first().name != SaperaCameraDevice::NoServerFoundMessage) {
            saperaDev->openDevice(devices.first().name, "");
        }
    }
}

void MainWindowRefactored::onSingleCaptureRequested()
{
    if (!cameraDevice_) return;

    auto* saperaDev = dynamic_cast<SaperaCameraDevice*>(cameraDevice_);
    if (!saperaDev) {
        // Non-Sapera camera: simple grab
        if (!cameraDevice_->isOpened()) {
            if (!cameraDevice_->openDevice("", "")) {
                QMessageBox::critical(this, tr("Error"), tr("Failed to initialize camera"));
                return;
            }
        }
        if (cameraDevice_->grabOne()) {
            showTransientMessage(tr("Single capture started"), 2000);
        }
        return;
    }

    // Sapera camera path (needs server/config)
    QString server = saperaDev->openDevice("", "") ? QString() : QString(); // already set in populate

    if (!saperaDev->isOpened()) {
        // Try to open with last known config
        QString configFile = QFileDialog::getOpenFileName(this, tr("Select Configuration File"),
            configDirectory_, tr("Configuration Files (*.ccf);;All Files (*)"));
        if (configFile.isEmpty()) return;
        if (!saperaDev->openDevice(saperaDev->getAvailableServers().first(), configFile)) {
            QMessageBox::critical(this, tr("Error"), tr("Failed to initialize camera"));
            return;
        }
    }

    if (saperaDev->grabOne()) {
        showTransientMessage(tr("Single capture started"), 2000);
    } else {
        showTransientMessage(tr("Single capture failed"), 3000);
    }
}

void MainWindowRefactored::onContinuousCaptureRequested()
{
    if (!cameraDevice_) return;

    if (!cameraDevice_->isOpened()) {
        // For Sapera: need server + config
        auto* saperaDev = dynamic_cast<SaperaCameraDevice*>(cameraDevice_);
        if (saperaDev) {
            QStringList servers = SaperaCameraDevice::getAvailableServers();
            if (servers.isEmpty() || servers.first() == SaperaCameraDevice::NoServerFoundMessage) {
                QMessageBox::warning(this, tr("Warning"), tr("No acquisition server available"));
                return;
            }
            QString configFile = QFileDialog::getOpenFileName(this, tr("Select Configuration File"),
                configDirectory_, tr("Configuration Files (*.ccf);;All Files (*)"));
            if (configFile.isEmpty()) return;
            if (!saperaDev->openDevice(servers.first(), configFile)) {
                QMessageBox::critical(this, tr("Error"), tr("Failed to initialize camera"));
                return;
            }
        } else {
            if (!cameraDevice_->openDevice("", "")) {
                QMessageBox::critical(this, tr("Error"), tr("Failed to initialize camera"));
                return;
            }
        }
    }

    if (cameraDevice_->startGrabbing()) {
        showTransientMessage(tr("Continuous capture started"), 2000);
        if (displayDock_) {
            displayDock_->setContinuousCaptureState(true);
        }
    } else {
        showTransientMessage(tr("Continuous capture failed"), 3000);
    }
}

void MainWindowRefactored::onStopCaptureRequested()
{
    if (!cameraDevice_) return;

    if (cameraDevice_->stopGrabbing()) {
        if (multiSaveActive_) {
            multiSaveActive_ = false;
            multiSaveAutoStop_ = false;
            multiSaveRemaining_ = 0;
            multiSaveTotal_ = 0;
        }
        showTransientMessage(tr("Capture stopped"), 2000);
        if (displayDock_) {
            displayDock_->setContinuousCaptureState(false);
        }
    }
}

bool MainWindowRefactored::saveImageWithFormat(int bufferIndex, const QString& filePath)
{
    if (!cameraDevice_) return false;

    return cameraDevice_->saveImage(bufferIndex, filePath);
}

void MainWindowRefactored::onSaveSingleImageRequested()
{
    if (!cameraDevice_ || !displayDock_) return;

    int currentIndex = displayDock_->getCurrentBufferIndex();
    QImage image = cameraDevice_->getDisplayImage(currentIndex);

    if (image.isNull()) {
        QMessageBox::warning(this, tr("Warning"), tr("No image in current buffer to save"));
        return;
    }

    int firstPixelValue = displayDock_->getFirstPixelValue();
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (defaultPath.isEmpty())
        defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    QString defaultFileName = QString("/capture_%1_px%2").arg(currentIndex).arg(firstPixelValue);
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Image"),
        defaultPath + defaultFileName,
        tr("PNG Image (*.png);;TIFF Image (*.tiff);;BMP Image (*.bmp);;JPEG Image (*.jpg);;RAW Image (*.raw);;All Files (*)"));

    if (filePath.isEmpty()) return;

    if (saveImageWithFormat(currentIndex, filePath)) {
        showTransientMessage(tr("Image saved: %1").arg(filePath), 3000);
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save image"));
    }
}

void MainWindowRefactored::onSaveMultipleImagesRequested(int count, const QString& directory, const QString& format)
{
    if (!cameraDevice_) return;

    if (!cameraDevice_->isOpened()) {
        QMessageBox::warning(this, tr("Warning"), tr("Acquisition device is not initialized."));
        return;
    }

    if (format.toLower() != "raw") {
        QList<QByteArray> supportedFormats = QImageWriter::supportedImageFormats();
        bool formatSupported = false;
        for (const QByteArray& fmt : supportedFormats) {
            if (QString::fromLatin1(fmt).toLower() == format.toLower()) {
                formatSupported = true;
                break;
            }
        }
        if (!formatSupported) {
            QString supportedList;
            for (const QByteArray& fmt : supportedFormats)
                supportedList += QString::fromLatin1(fmt) + " ";
            QMessageBox::warning(this, tr("Warning"),
                tr("Image format '%1' is not supported.\n\nSupported formats: %2")
                    .arg(format).arg(supportedList.trimmed()));
            return;
        }
    }

    QDir dir(directory);
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            QMessageBox::critical(this, tr("Error"), tr("Failed to create directory: %1").arg(directory));
            return;
        }
    }
    dir = QDir(dir.absolutePath());

    multiSaveActive_ = false;
    multiSaveAutoStop_ = false;
    multiSaveRemaining_ = 0;

    multiSaveDirectory_ = dir.absolutePath();
    multiSaveTotal_ = count;
    multiSaveRemaining_ = count;
    multiSaveSavedCount_ = 0;
    multiSaveProcessedCount_ = 0;
    multiSaveBatchId_++;
    multiSaveFormat_ = format;
    multiSaveBaseName_ = QStringLiteral("capture_%1_%2").arg(multiSaveBatchId_).arg(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));

    bool needStart = !isContinuousCaptureActive_;
    if (needStart) {
        if (!cameraDevice_->startGrabbing()) {
            QMessageBox::critical(this, tr("Error"), tr("Failed to start acquisition for multi-save."));
            return;
        }
        multiSaveAutoStop_ = true;
    }

    multiSaveActive_ = true;
    showTransientMessage(tr("Saving %1 images (%2) to %3").arg(count).arg(format).arg(multiSaveDirectory_), 3000);
}

void MainWindowRefactored::onBufferCountChanged(int count)
{
    if (isContinuousCaptureActive_ && cameraDevice_ && displayDock_) {
        displayDock_->setBufferCount(cameraDevice_->getBufferCount());
        return;
    }

    if (cameraDevice_) {
        cameraDevice_->setBufferCount(count);
        showTransientMessage(tr("Buffer count set to %1").arg(count), 2000);
    }
    if (imageDataDock_) {
        imageDataDock_->setHistorySize(count);
    }
}

void MainWindowRefactored::onFrameReady(const ImageFrameData& frame)
{
    if (cameraDevice_ && multiSaveActive_ && multiSaveRemaining_ > 0) {
        multiSaveRemaining_--;

        QDir targetDir(multiSaveDirectory_);
        const int sequenceNumber = multiSaveTotal_ - multiSaveRemaining_;

        int firstPixelValue = 0;
        if (displayDock_)
            firstPixelValue = displayDock_->getFirstPixelValue();

        QString extension = multiSaveFormat_.isEmpty() ? "png" : multiSaveFormat_;
        const QString fileName = QStringLiteral("%1_%2_px%3.%4")
                                     .arg(multiSaveBaseName_)
                                     .arg(sequenceNumber, 4, 10, QChar('0'))
                                     .arg(firstPixelValue)
                                     .arg(extension);
        const QString filePath = targetDir.filePath(fileName);

        if (imageSaveWorker_) {
            int w = frame.width, h = frame.height, bd = frame.bitDepth;

            if (frame.rawData16) {
                imageSaveWorker_->saveImage(*frame.rawData16, w, h, bd, filePath, extension);
            } else if (frame.rawData8) {
                // Convert 8-bit to 16-bit for save worker compatibility
                QVector<uint16_t> converted(frame.rawData8->size());
                for (int i = 0; i < frame.rawData8->size(); ++i)
                    converted[i] = static_cast<uint16_t>((*frame.rawData8)[i]);
                imageSaveWorker_->saveImage(converted, w, h, bd, filePath, extension);
            } else {
                multiSaveActive_ = false;
                multiSaveAutoStop_ = false;
            }
        } else {
            multiSaveActive_ = false;
            multiSaveAutoStop_ = false;
        }
    }

    if (cameraDevice_ && displayDock_) {
        if (frame.rawData16) {
            QVector<uint16_t> rawCopy = *frame.rawData16;
            displayDock_->setRawData(rawCopy, frame.width, frame.height, frame.bitDepth);
        } else if (frame.rawData8) {
            QVector<uint16_t> rawCopy(frame.rawData8->size());
            for (int i = 0; i < frame.rawData8->size(); ++i)
                rawCopy[i] = static_cast<uint16_t>((*frame.rawData8)[i]);
            displayDock_->setRawData(rawCopy, frame.width, frame.height, frame.bitDepth);
        }

        // 更新缓冲区滑块跟随最新帧
        int bufIdx = static_cast<int>(frame.frameIndex % cameraDevice_->getBufferCount());
        displayDock_->setCurrentBufferIndex(bufIdx);

        if (!frame.displayImage.isNull()) {
            displayDock_->displayImage(frame.displayImage);
        } else {
            QImage img = cameraDevice_->getDisplayImage(bufIdx);
            displayDock_->displayImage(img);
        }

        if (imageAlgorithmDock_) {
            imageAlgorithmDock_->setBitDepth(frame.bitDepth);
            if (frame.rawData16) {
                imageAlgorithmDock_->onRawImageReceived(
                    *frame.rawData16, frame.width, frame.height, frame.bitDepth);
            }
        }

        if (imageDataDock_) {
#if ANALYZE_RAW_DATA
            if (frame.rawData16) {
                imageDataDock_->updateRawData(
                    *frame.rawData16, frame.width, frame.height, frame.bitDepth);
            }
#else
            if (!frame.displayImage.isNull()) {
                imageDataDock_->updateImage(frame.displayImage);
            }
#endif
        }

    }
}

void MainWindowRefactored::onAcquisitionError(const QString& error)
{
    if (multiSaveActive_) {
        multiSaveActive_ = false;
        multiSaveAutoStop_ = false;
        multiSaveRemaining_ = 0;
        multiSaveTotal_ = 0;
    }
    showTransientMessage(tr("Acquisition error: %1").arg(error), 5000);
    QMessageBox::warning(this, tr("Acquisition Error"), error);
}

void MainWindowRefactored::onDeviceChanged(const DeviceSelectorWidget::DeviceInfo& info)
{
    if (cameraDevice_) {
        cameraDevice_->stopGrabbing();
        cameraDevice_->closeDevice();
    }

    // For Sapera: store selected server name
    auto* saperaDev = dynamic_cast<SaperaCameraDevice*>(cameraDevice_);
    if (saperaDev && info.name != SaperaCameraDevice::NoServerFoundMessage) {
        showTransientMessage(tr("Device changed to: %1").arg(info.name), 2000);
    }
}

void MainWindowRefactored::onConfigurationChanged(const QString& configPath)
{
    auto* saperaDev = dynamic_cast<SaperaCameraDevice*>(cameraDevice_);
    if (!saperaDev) return;

    saperaDev->stopGrabbing();
    saperaDev->closeDevice();

    QStringList servers = SaperaCameraDevice::getAvailableServers();
    if (servers.isEmpty() || servers.first() == SaperaCameraDevice::NoServerFoundMessage) return;

    if (saperaDev->openDevice(servers.first(), configPath)) {
        showTransientMessage(tr("Configuration loaded: %1").arg(QFileInfo(configPath).fileName()), 2000);
    } else {
        showTransientMessage(tr("Failed to load configuration"), 3000);
    }
}
