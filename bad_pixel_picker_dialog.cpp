#include "bad_pixel_picker_dialog.h"

#include "display_dock.h"

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

BadPixelPickerDialog::BadPixelPickerDialog(DisplayDock *displayDock, QWidget *parent)
    : QDialog(parent)
    , displayDock_(displayDock)
{
    setWindowTitle(tr("批量添加坏点"));
    setModal(false);
    setWindowFlag(Qt::Tool, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    hintLabel_ = new QLabel(tr("在\"图像显示\"中用鼠标单击添加坏点坐标（可连续点击）。"), this);
    hintLabel_->setWordWrap(true);
    layout->addWidget(hintLabel_);

    listWidget_ = new QListWidget(this);
    listWidget_->setMinimumWidth(220);
    listWidget_->setMinimumHeight(160);
    layout->addWidget(listWidget_, 1);

    auto *toolsRow = new QWidget(this);
    auto *toolsLayout = new QHBoxLayout(toolsRow);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(8);

    undoButton_ = new QPushButton(tr("撤销"), toolsRow);
    clearButton_ = new QPushButton(tr("清空"), toolsRow);
    toolsLayout->addWidget(undoButton_);
    toolsLayout->addWidget(clearButton_);
    toolsLayout->addStretch(1);
    layout->addWidget(toolsRow);

    connect(undoButton_, &QPushButton::clicked, this, &BadPixelPickerDialog::onUndoClicked);
    connect(clearButton_, &QPushButton::clicked, this, &BadPixelPickerDialog::onClearClicked);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox_);
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (displayDock_) {
        connect(displayDock_, &DisplayDock::badPixelPointPicked,
                this, &BadPixelPickerDialog::onPointPicked,
                Qt::QueuedConnection);
    }

    setPickMode(true);
    refreshList();
}

BadPixelPickerDialog::~BadPixelPickerDialog()
{
    setPickMode(false);
}

void BadPixelPickerDialog::closeEvent(QCloseEvent *event)
{
    setPickMode(false);
    QDialog::closeEvent(event);
}

void BadPixelPickerDialog::setPickMode(bool enabled)
{
    if (!displayDock_) {
        return;
    }

    displayDock_->setBadPixelPickMode(enabled);
    if (!enabled) {
        displayDock_->clearBadPixelMarkers();
    } else {
        displayDock_->setBadPixelMarkers(points_);
    }
}

void BadPixelPickerDialog::onPointPicked(const QPoint &imagePos)
{
    if (imagePos.x() < 0 || imagePos.y() < 0) {
        return;
    }

    if (points_.contains(imagePos)) {
        return;
    }

    points_.push_back(imagePos);
    refreshList();

    if (displayDock_) {
        displayDock_->setBadPixelMarkers(points_);
    }
}

void BadPixelPickerDialog::onClearClicked()
{
    points_.clear();
    refreshList();

    if (displayDock_) {
        displayDock_->clearBadPixelMarkers();
    }
}

void BadPixelPickerDialog::onUndoClicked()
{
    if (points_.isEmpty()) {
        return;
    }

    points_.removeLast();
    refreshList();

    if (displayDock_) {
        displayDock_->setBadPixelMarkers(points_);
    }
}

void BadPixelPickerDialog::refreshList()
{
    if (!listWidget_) {
        return;
    }

    listWidget_->clear();
    for (const QPoint &p : points_) {
        listWidget_->addItem(QStringLiteral("%1,%2").arg(p.x()).arg(p.y()));
    }

    if (hintLabel_) {
        hintLabel_->setText(tr("在\"图像显示\"中单击添加坏点坐标（已选择 %1 个）。")
                               .arg(points_.size()));
    }

    if (undoButton_) {
        undoButton_->setEnabled(!points_.isEmpty());
    }
    if (clearButton_) {
        clearButton_->setEnabled(!points_.isEmpty());
    }
}
