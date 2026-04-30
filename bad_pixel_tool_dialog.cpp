#include "bad_pixel_tool_dialog.h"

#include "display_dock.h"

#include <QCloseEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QIntValidator>
#include <QFile>
#include <QFileDialog>
#include <QDataStream>
#include <algorithm>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>

BadPixelToolDialog::BadPixelToolDialog(DisplayDock *displayDock, QWidget *parent)
    : QDialog(parent)
    , displayDock_(displayDock)
{
    setWindowTitle(tr("坏点工具"));
    setModal(false);
    setMinimumSize(700, 450);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // ========== 左侧：读取坏点列表 ==========
    auto *leftGroup = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftGroup);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    readHintLabel_ = new QLabel(tr("点击\"读取\"按钮获取相机中存储的坏点坐标。"), leftGroup);
    readHintLabel_->setWordWrap(true);
    leftLayout->addWidget(readHintLabel_);

    readListWidget_ = new QListWidget(leftGroup);
    readListWidget_->setMinimumWidth(220);
    readListWidget_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    leftLayout->addWidget(readListWidget_, 1);

    // 左侧按钮行
    auto *leftButtonLayout = new QHBoxLayout();
    leftButtonLayout->setSpacing(8);

    readButton_ = new QPushButton(tr("读取"), leftGroup);
    readButton_->setToolTip(tr("从相机读取已存储的坏点坐标"));
    readButton_->setMinimumWidth(80);
    connect(readButton_, &QPushButton::clicked, this, &BadPixelToolDialog::onReadClicked);
    leftButtonLayout->addWidget(readButton_);

    leftButtonLayout->addStretch();

    deleteReadButton_ = new QPushButton(tr("删除选中"), leftGroup);
    deleteReadButton_->setToolTip(tr("从相机删除选中的坏点"));
    deleteReadButton_->setEnabled(false);
    deleteReadButton_->setMinimumWidth(80);
    connect(deleteReadButton_, &QPushButton::clicked, this, &BadPixelToolDialog::onDeleteReadClicked);
    leftButtonLayout->addWidget(deleteReadButton_);

    leftLayout->addLayout(leftButtonLayout);

    mainLayout->addWidget(leftGroup, 1);

    // ========== 右侧：添加坏点列表 ==========
    auto *rightGroup = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightGroup);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    addHintLabel_ = new QLabel(tr("在\"图像显示\"中单击拾取坏点。"), rightGroup);
    addHintLabel_->setWordWrap(true);
    rightLayout->addWidget(addHintLabel_);

    addListWidget_ = new QListWidget(rightGroup);
    addListWidget_->setMinimumWidth(220);
    addListWidget_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rightLayout->addWidget(addListWidget_, 1);

    // 操作按钮行1：删除选中 和 保存主要操作
    auto *opButtonLayout = new QHBoxLayout();
    opButtonLayout->setSpacing(8);

    deleteAddButton_ = new QPushButton(tr("删除选中"), rightGroup);
    deleteAddButton_->setToolTip(tr("从待添加列表中删除选中项"));
    deleteAddButton_->setEnabled(false);
    connect(deleteAddButton_, &QPushButton::clicked, this, &BadPixelToolDialog::onDeleteAddClicked);
    opButtonLayout->addWidget(deleteAddButton_);

    opButtonLayout->addStretch();

    addButton_ = new QPushButton(tr("发送添加指令"), rightGroup);
    addButton_->setToolTip(tr("将列表中的坏点坐标逐个发送到相机"));
    addButton_->setEnabled(false);
    connect(addButton_, &QPushButton::clicked, this, &BadPixelToolDialog::onAddClicked);
    opButtonLayout->addWidget(addButton_);

    opButtonLayout->addStretch();

    saveButton_ = new QPushButton(tr("保存到相机"), rightGroup);
    saveButton_->setToolTip(tr("将当前坏点坐标保存到相机Flash"));
    connect(saveButton_, &QPushButton::clicked, this, &BadPixelToolDialog::onSaveClicked);
    opButtonLayout->addWidget(saveButton_);

    rightLayout->addLayout(opButtonLayout);

    // 操作按钮行2：生成和读取DCP文件
    auto *dcpLayout = new QHBoxLayout();
    dcpLayout->setSpacing(8);

    generateDcpButton_ = new QPushButton(tr("生成dcp"), rightGroup);
    generateDcpButton_->setToolTip(tr("将当前添加列表中的坏点坐标生成.dcp文件"));
    connect(generateDcpButton_, &QPushButton::clicked, this, &BadPixelToolDialog::onGenerateDcpClicked);
    dcpLayout->addWidget(generateDcpButton_);
    
    dcpLayout->addStretch();

    readDcpButton_ = new QPushButton(tr("读取dcp"), rightGroup);
    readDcpButton_->setToolTip(tr("从.dcp文件读取坏点坐标并添加到列表"));
    connect(readDcpButton_, &QPushButton::clicked, this, &BadPixelToolDialog::onReadDcpClicked);
    dcpLayout->addWidget(readDcpButton_);
    
    dcpLayout->addStretch();
    
    toggleMarkersButton_ = new QPushButton(tr("隐藏坏点标记"), rightGroup);
    toggleMarkersButton_->setToolTip(tr("在图像上隐藏或显示当前选择的坏点标记"));
    connect(toggleMarkersButton_, &QPushButton::clicked, this, &BadPixelToolDialog::onToggleMarkersClicked);
    dcpLayout->addWidget(toggleMarkersButton_);

    rightLayout->addLayout(dcpLayout);
    // 发送添加指令连接等已经在上面初始化了。

    mainLayout->addWidget(rightGroup, 1);

    // 连接 DisplayDock 的坏点拾取信号
    if (displayDock_) {
        connect(displayDock_, &DisplayDock::badPixelPointPicked,
                this, &BadPixelToolDialog::onPointPicked,
                Qt::QueuedConnection);
    }

    // 连接列表选择变化信号
    connect(readListWidget_, &QListWidget::itemSelectionChanged, this, [this]() {
        deleteReadButton_->setEnabled(!readListWidget_->selectedItems().isEmpty());
    });

    connect(addListWidget_, &QListWidget::itemSelectionChanged, this, [this]() {
        deleteAddButton_->setEnabled(!addListWidget_->selectedItems().isEmpty());
    });

    // 右键菜单策略
    readListWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(readListWidget_, &QListWidget::customContextMenuRequested, this, &BadPixelToolDialog::showReadListContextMenu);

    addListWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(addListWidget_, &QListWidget::customContextMenuRequested, this, &BadPixelToolDialog::showAddListContextMenu);

    setPickMode(true);
    refreshAddList();
    refreshReadList();
}

BadPixelToolDialog::~BadPixelToolDialog()
{
    setPickMode(false);
}

void BadPixelToolDialog::closeEvent(QCloseEvent *event)
{
    setPickMode(false);
    QDialog::closeEvent(event);
}

void BadPixelToolDialog::setPickMode(bool enabled)
{
    if (!displayDock_) {
        return;
    }

    displayDock_->setBadPixelPickMode(enabled);
    if (!enabled) {
        displayDock_->clearBadPixelMarkers();
    } else {
        updateMarkers();
    }
}

void BadPixelToolDialog::onPointPicked(const QPoint &imagePos)
{
    if (imagePos.x() < 0 || imagePos.y() < 0) {
        return;
    }

    // 检查是否已存在于待添加列表
    if (addPoints_.contains(imagePos)) {
        return;
    }

    addPoints_.push_back(imagePos);
    refreshAddList();
    updateMarkers();
}

void BadPixelToolDialog::onReadClicked()
{
    readHintLabel_->setText(tr("正在读取相机坏点坐标..."));
    emit readBadPixelsRequested();
}

void BadPixelToolDialog::onAddClicked()
{
    if (addPoints_.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("待添加列表为空。"));
        return;
    }

    // 逐个发送添加坏点指令
    for (const QPoint &p : addPoints_) {
        emit addBadPixelRequested(static_cast<unsigned int>(p.x()),
                                   static_cast<unsigned int>(p.y()));
    }

  //  QMessageBox::information(this, tr("完成"), 
    //    tr("已发送 %1 个坏点添加指令。\n如需永久保存，请点击\"保存到相机\"。")
      //      .arg(addPoints_.size()));

    // 清空待添加列表
    addPoints_.clear();
    refreshAddList();
    updateMarkers();
}

void BadPixelToolDialog::onDeleteReadClicked()
{
    QList<QListWidgetItem*> selected = readListWidget_->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    // 发送删除坏点指令
    for (QListWidgetItem *item : selected) {
        QStringList parts = item->text().split(',', Qt::SkipEmptyParts);
        if (parts.size() == 2) {
            bool okX = false, okY = false;
            unsigned int x = parts[0].trimmed().toUInt(&okX);
            unsigned int y = parts[1].trimmed().toUInt(&okY);
            if (okX && okY) {
                emit removeBadPixelRequested(x, y);
                
                // 从本地列表中移除
                readPoints_.removeAll(QPoint(x, y));
            }
        }
    }

    refreshReadList();
   // QMessageBox::information(this, tr("完成"), 
       // tr("已发送删除指令。如需永久保存，请点击\"保存到相机\"。"));
}

void BadPixelToolDialog::onDeleteAddClicked()
{
    QList<QListWidgetItem*> selected = addListWidget_->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    // 从待添加列表中移除选中项
    for (QListWidgetItem *item : selected) {
        QStringList parts = item->text().split(',', Qt::SkipEmptyParts);
        if (parts.size() == 2) {
            bool okX = false, okY = false;
            int x = parts[0].trimmed().toInt(&okX);
            int y = parts[1].trimmed().toInt(&okY);
            if (okX && okY) {
                addPoints_.removeAll(QPoint(x, y));
            }
        }
    }

    refreshAddList();
    updateMarkers();
}

void BadPixelToolDialog::onGenerateDcpClicked()
{
    if (addPoints_.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("待添加列表为空，无法生成文件。"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("生成 dcp 文件"), "", tr("DCP Files (*.dcp);;All Files (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("错误"), tr("无法打开文件进行写入。"));
        return;
    }

    // 写入坐标，大端序，2个UInt16 = 4 bytes 
    QDataStream out(&file);
    out.setByteOrder(QDataStream::BigEndian);

    int count = addPoints_.size();
    // 限制在最大容量2048个(8192 bytes / 4 bytes)
    if (count > 2048) {
        count = 2048;
    }

    for (int i = 0; i < count; ++i) {
        const QPoint &p = addPoints_[i];
        out << static_cast<quint16>(p.x());
        out << static_cast<quint16>(p.y());
    }

    // 剩下的字节补 FF
    int writtenBytes = count * 4;
    int remainBytes = 8192 - writtenBytes;
    if (remainBytes > 0) {
        QByteArray padding(remainBytes, 0xFF);
        file.write(padding);
    }
    
    file.close();
    QMessageBox::information(this, tr("成功"), tr("成功生成 dcp 文件，包含 %1 个坐标。").arg(count));
}

void BadPixelToolDialog::onReadDcpClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("读取 dcp 文件"), "", tr("DCP Files (*.dcp);;All Files (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("错误"), tr("无法读取文件。"));
        return;
    }

    QByteArray data = file.read(8192);
    file.close();

    if (data.size() < 8192) {
        QMessageBox::warning(this, tr("错误"), tr("文件大小不符合要求。"));
        return;
    }

    QDataStream in(data);
    in.setByteOrder(QDataStream::BigEndian);

    addPoints_.clear();

    for (int i = 0; i < 2048; ++i) {
        quint16 x, y;
        in >> x >> y;
        
        // 坐标都为 FFFF 即没有坐标了
        if (x == 0xFFFF && y == 0xFFFF) {
            break;
        }

        addPoints_.push_back(QPoint(x, y));
    }

    refreshAddList();
    updateMarkers();
    
    QMessageBox::information(this, tr("成功"), tr("成功读取 dcp 文件，包含 %1 个坐标。").arg(addPoints_.size()));
}

void BadPixelToolDialog::onSaveClicked()
{
    emit saveBadPixelsRequested();
}

void BadPixelToolDialog::onBadPixelCoordinatesReceived(const QByteArray &data)
{
    readPoints_.clear();

    // 解析坏点数据：每4字节一组 (XH XL YH YL)
    int pixelCount = data.size() / 4;
    for (int i = 0; i < pixelCount; ++i) {
        int offset = i * 4;
        if (offset + 3 >= data.size()) break;

        unsigned int x = (static_cast<unsigned char>(data[offset]) << 8) |
                         static_cast<unsigned char>(data[offset + 1]);
        unsigned int y = (static_cast<unsigned char>(data[offset + 2]) << 8) |
                         static_cast<unsigned char>(data[offset + 3]);

        readPoints_.append(QPoint(x, y));
    }

    refreshReadList();
    readHintLabel_->setText(tr("已读取 %1 个坏点坐标。").arg(readPoints_.size()));
}

void BadPixelToolDialog::refreshAddList()
{
    if (!addListWidget_) {
        return;
    }

    // 总体按y从大到小排序，y相同则按x从大到小排序
    std::sort(addPoints_.begin(), addPoints_.end(), [](const QPoint &a, const QPoint &b) {
        if (a.y() != b.y()) {
            return a.y() > b.y();
        }
        return a.x() > b.x();
    });

    addListWidget_->clear();
    for (const QPoint &p : addPoints_) {
        addListWidget_->addItem(QStringLiteral("%1, %2").arg(p.x()).arg(p.y()));
    }

    addHintLabel_->setText(tr("在\"图像显示\"中单击拾取坏点（已选择 %1 个）。")
                               .arg(addPoints_.size()));

    addButton_->setEnabled(!addPoints_.isEmpty());
    deleteAddButton_->setEnabled(!addListWidget_->selectedItems().isEmpty());
}

void BadPixelToolDialog::refreshReadList()
{
    if (!readListWidget_) {
        return;
    }

    // 总体按y从大到小排序，y相同则按x从大到小排序
    std::sort(readPoints_.begin(), readPoints_.end(), [](const QPoint &a, const QPoint &b) {
        if (a.y() != b.y()) {
            return a.y() > b.y();
        }
        return a.x() > b.x();
    });

    readListWidget_->clear();
    for (const QPoint &p : readPoints_) {
        readListWidget_->addItem(QStringLiteral("%1, %2").arg(p.x()).arg(p.y()));
    }

    deleteReadButton_->setEnabled(!readListWidget_->selectedItems().isEmpty());
}

void BadPixelToolDialog::updateMarkers()
{
    if (!displayDock_) {
        return;
    }

    if (markersVisible_) {
        // 显示待添加的坏点标记
        displayDock_->setBadPixelMarkers(addPoints_);
    } else {
        // 隐藏坏点标记
        displayDock_->clearBadPixelMarkers();
    }
}

void BadPixelToolDialog::onToggleMarkersClicked()
{
    markersVisible_ = !markersVisible_;
    if (markersVisible_) {
        toggleMarkersButton_->setText(tr("隐藏坏点标记"));
    } else {
        toggleMarkersButton_->setText(tr("显示坏点标记"));
    }
    updateMarkers();
}

void BadPixelToolDialog::showReadListContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    QAction *copyAct = menu.addAction(tr("复制"));
    QAction *cutAct = menu.addAction(tr("剪切"));
    QAction *pasteAct = menu.addAction(tr("粘贴"));

    if (readListWidget_->selectedItems().isEmpty()) {
        copyAct->setEnabled(false);
        cutAct->setEnabled(false);
    }
    
    if (QApplication::clipboard()->text().isEmpty()) {
        pasteAct->setEnabled(false);
    }

    QAction *res = menu.exec(readListWidget_->viewport()->mapToGlobal(pos));
    if (res == copyAct) {
        handleCopy(readListWidget_);
    } else if (res == cutAct) {
        handleCut(readListWidget_, readPoints_, false);
    } else if (res == pasteAct) {
        handlePaste(readPoints_, false);
    }
}

void BadPixelToolDialog::showAddListContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    QAction *copyAct = menu.addAction(tr("复制"));
    QAction *cutAct = menu.addAction(tr("剪切"));
    QAction *pasteAct = menu.addAction(tr("粘贴"));

    if (addListWidget_->selectedItems().isEmpty()) {
        copyAct->setEnabled(false);
        cutAct->setEnabled(false);
    }
    
    if (QApplication::clipboard()->text().isEmpty()) {
        pasteAct->setEnabled(false);
    }

    QAction *res = menu.exec(addListWidget_->viewport()->mapToGlobal(pos));
    if (res == copyAct) {
        handleCopy(addListWidget_);
    } else if (res == cutAct) {
        handleCut(addListWidget_, addPoints_, true);
    } else if (res == pasteAct) {
        handlePaste(addPoints_, true);
    }
}

void BadPixelToolDialog::handleCopy(QListWidget *listWidget)
{
    QList<QListWidgetItem*> selected = listWidget->selectedItems();
    if (selected.isEmpty()) return;

    QStringList texts;
    for (auto *item : selected) {
        texts << item->text();
    }
    QApplication::clipboard()->setText(texts.join("\n"));
}

void BadPixelToolDialog::handleCut(QListWidget *listWidget, QVector<QPoint> &pointsList, bool isAddList)
{
    handleCopy(listWidget);

    QList<QListWidgetItem*> selected = listWidget->selectedItems();
    for (auto *item : selected) {
        QStringList parts = item->text().split(',', Qt::SkipEmptyParts);
        if (parts.size() == 2) {
            int x = parts[0].trimmed().toInt();
            int y = parts[1].trimmed().toInt();
            pointsList.removeAll(QPoint(x, y));
        }
    }

    if (isAddList) {
        refreshAddList();
        updateMarkers();
    } else {
        refreshReadList();
    }
}

void BadPixelToolDialog::handlePaste(QVector<QPoint> &pointsList, bool isAddList)
{
    QString text = QApplication::clipboard()->text();
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QStringList parts = line.split(',', Qt::SkipEmptyParts);
        if (parts.size() == 2) {
            bool okX = false, okY = false;
            int x = parts[0].trimmed().toInt(&okX);
            int y = parts[1].trimmed().toInt(&okY);
            
            if (okX && okY && x >= 0 && y >= 0) {
                QPoint p(x, y);
                if (!pointsList.contains(p)) {
                    pointsList.push_back(p);
                }
            }
        }
    }

    if (isAddList) {
        refreshAddList();
        updateMarkers();
    } else {
        refreshReadList();
    }
}

