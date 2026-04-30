#include "image_algorithm_dock.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QPushButton>
#include <QHBoxLayout>

QWidget* ImageAlgorithmDock::createReadoutNoisePage()
{
    QWidget *page = new QWidget(stackedPages_);
    page->setMinimumWidth(0);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(15);
    layout->setContentsMargins(10, 10, 10, 10);

    QGroupBox *grpArea = new QGroupBox(tr("采样与计算区域"), page);
    grpArea->setFlat(true);
    grpArea->setTitle(QString());
    grpArea->setStyleSheet("QGroupBox { border: none; margin-top: 0px; } QGroupBox::title { color: transparent; }");
    QGridLayout *gridArea = new QGridLayout(grpArea);
    gridArea->setVerticalSpacing(10);

    gridArea->addWidget(new QLabel(tr("数据采样数量："), grpArea), 0, 0);
    spinSampleCount_ = new QSpinBox(grpArea);
    spinSampleCount_->setRange(1, 1000);
    spinSampleCount_->setValue(10);
    gridArea->addWidget(spinSampleCount_, 0, 1, 1, 2);

    gridArea->addWidget(new QLabel(tr("起始坐标 (X,Y)："), grpArea), 1, 0);
    spinStartX_ = new QSpinBox(grpArea);
    spinStartX_->setRange(0, 10000);
    spinStartX_->setValue(0);
    spinStartY_ = new QSpinBox(grpArea);
    spinStartY_->setRange(0, 10000);
    spinStartY_->setValue(0);

    QHBoxLayout *hLayoutStart = new QHBoxLayout;
    hLayoutStart->addWidget(spinStartX_);
    hLayoutStart->addWidget(spinStartY_);
    hLayoutStart->setContentsMargins(0,0,0,0);
    gridArea->addLayout(hLayoutStart, 1, 1, 1, 2);

    gridArea->addWidget(new QLabel(tr("区域大小 (宽,高)："), grpArea), 2, 0);
    spinWidth_ = new QSpinBox(grpArea);
    spinWidth_->setRange(1, 10000);
    spinWidth_->setValue(10);
    spinHeight_ = new QSpinBox(grpArea);
    spinHeight_->setRange(1, 10000);
    spinHeight_->setValue(10);

    QHBoxLayout *hLayoutSize = new QHBoxLayout;
    hLayoutSize->addWidget(spinWidth_);
    hLayoutSize->addWidget(spinHeight_);
    hLayoutSize->setContentsMargins(0,0,0,0);
    gridArea->addLayout(hLayoutSize, 2, 1, 1, 2);

    layout->addWidget(grpArea);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(10);
    buttonLayout->setContentsMargins(0, 0, 0, 0);

    btnLocalAnalysis_ = new QPushButton(tr("本地分析"), page);

    connect(btnLocalAnalysis_, &QPushButton::clicked, this, &ImageAlgorithmDock::onLocalAnalysisClicked);

    buttonLayout->addWidget(btnLocalAnalysis_);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    layout->addStretch();

    return page;
}
