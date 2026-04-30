#include "image_algorithm_dock.h"
#include "mixed_processing_panel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

QWidget* ImageAlgorithmDock::createMixedProcessingPage()
{
    QWidget *page = new QWidget(stackedPages_);
    page->setMinimumWidth(0);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 10, 10, 10);

    QPushButton *openMixedBtn = new QPushButton(tr("打开混合处理窗口"), page);
    openMixedBtn->setMinimumHeight(40);
    openMixedBtn->setStyleSheet("QPushButton { font-size: 14px; font-weight: bold; }");

    connect(openMixedBtn, &QPushButton::clicked, this, [this]() {
        if (!mixedProcessingDialog_) {
            mixedProcessingDialog_ = new MixedProcessingDialog(this);

            if (imageAlgorithmManager_) {
                mixedProcessingDialog_->setAlgorithmManager(imageAlgorithmManager_);
            }

            connect(mixedProcessingDialog_, &MixedProcessingDialog::closed, this, [this]() {
                mixedProcessingDialog_ = nullptr;
            });
            connect(mixedProcessingDialog_, &QDialog::finished, this, [this]() {
                mixedProcessingDialog_ = nullptr;
            });
        }

        mixedProcessingDialog_->show();
        mixedProcessingDialog_->raise();
        mixedProcessingDialog_->activateWindow();
    });

    layout->addWidget(openMixedBtn);
    layout->addStretch();

    page->setLayout(layout);
    return page;
}
