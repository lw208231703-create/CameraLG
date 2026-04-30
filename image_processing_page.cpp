#include "image_algorithm_dock.h"
#include "image_algorithm_manager.h"
#include "image_processing_panel.h"

#include <QVBoxLayout>
#include <QTabBar>

QWidget* ImageAlgorithmDock::createImageProcessingPage()
{
    QWidget *page = new QWidget(stackedPages_);
    page->setMinimumWidth(0);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    QTabBar *modeTabs = new QTabBar(page);
    modeTabs->addTab(tr("标准(8位)"));
    modeTabs->addTab(tr("高精度(16位)"));
    modeTabs->setExpanding(false);
    modeTabs->setDocumentMode(true);
    modeTabs->setMovable(false);
    modeTabs->setCurrentIndex(0);
    connect(modeTabs, &QTabBar::currentChanged, this, [this](int idx) {
        setProcessingInputMode(idx == 1 ? ProcessingInputMode::Raw16Bit
                                        : ProcessingInputMode::Standard8Bit);
        if (imageAlgorithmManager_) {
            imageAlgorithmManager_->setInputScaleMode(idx == 1
                                                          ? ImageAlgorithmManager::InputScaleMode::Native
                                                          : ImageAlgorithmManager::InputScaleMode::ScaleTo255);
        }
        if (imageProcessingPanel_) {
            imageProcessingPanel_->setHighPrecisionMode(idx == 1);
        }
    });
    layout->addWidget(modeTabs);

    imageAlgorithmManager_ = new ImageAlgorithmManager(threadManager_, this);

    connect(imageAlgorithmManager_, &ImageAlgorithmManager::diagnosticMessage,
            this, &ImageAlgorithmDock::diagnosticMessage, Qt::QueuedConnection);

    imageProcessingPanel_ = new ImageProcessingPanel(page);
    imageProcessingPanel_->setAlgorithmManager(imageAlgorithmManager_);
    imageProcessingPanel_->setHighPrecisionMode(false);

    connect(imageProcessingPanel_, &ImageProcessingPanel::diagnosticMessage,
            this, &ImageAlgorithmDock::diagnosticMessage, Qt::QueuedConnection);

    layout->addWidget(imageProcessingPanel_);

    page->setLayout(layout);
    return page;
}
