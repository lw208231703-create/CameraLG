#ifndef BAD_PIXEL_TOOL_DIALOG_H
#define BAD_PIXEL_TOOL_DIALOG_H

#include <QDialog>
#include <QPoint>
#include <QVector>

class QLabel;
class QListWidget;
class QPushButton;
class QLineEdit;
class DisplayDock;

/**
 * @brief 坏点工具对话框 - 整合读取、添加、删除坏点功能
 * 
 * 左侧栏：读取的坏点列表，底部有读取按钮
 * 右侧栏：待添加的坏点列表，底部有添加按钮
 * 支持在图像显示区域点击拾取坏点，也可手动输入坐标
 */
class BadPixelToolDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BadPixelToolDialog(DisplayDock *displayDock, QWidget *parent = nullptr);
    ~BadPixelToolDialog() override;

    // 获取待添加的坏点列表
    QVector<QPoint> pendingAddPoints() const { return addPoints_; }

protected:
    void closeEvent(QCloseEvent *event) override;

signals:
    // 请求读取相机存储的坏点
    void readBadPixelsRequested();
    // 请求添加坏点（x, y）
    void addBadPixelRequested(unsigned int x, unsigned int y);
    // 请求删除坏点（x, y）
    void removeBadPixelRequested(unsigned int x, unsigned int y);
    // 请求保存当前坏点坐标
    void saveBadPixelsRequested();

public slots:
    // 接收从相机读取的坏点数据
    void onBadPixelCoordinatesReceived(const QByteArray &data);

private slots:
    void onPointPicked(const QPoint &imagePos);
    void onReadClicked();
    void onAddClicked();
    void onDeleteReadClicked();
    void onDeleteAddClicked();
    void onSaveClicked();
    void onGenerateDcpClicked();
    void onReadDcpClicked();

    void showReadListContextMenu(const QPoint &pos);
    void showAddListContextMenu(const QPoint &pos);
    void handleCopy(QListWidget *listWidget);
    void handleCut(QListWidget *listWidget, QVector<QPoint> &pointsList, bool isAddList);
    void handlePaste(QVector<QPoint> &pointsList, bool isAddList);
    void onToggleMarkersClicked();

private:
    void setPickMode(bool enabled);
    void refreshAddList();
    void refreshReadList();
    void updateMarkers();

    DisplayDock *displayDock_{nullptr};
    
    // 从相机读取的坏点列表
    QVector<QPoint> readPoints_;
    // 待添加的坏点列表
    QVector<QPoint> addPoints_;

    // 左侧（读取）UI
    QLabel *readHintLabel_{nullptr};
    QListWidget *readListWidget_{nullptr};
    QPushButton *readButton_{nullptr};
    QPushButton *deleteReadButton_{nullptr};

    // 右侧（添加）UI
    QLabel *addHintLabel_{nullptr};
    QListWidget *addListWidget_{nullptr};
    QPushButton *addButton_{nullptr};         // 发送添加指令
    QPushButton *deleteAddButton_{nullptr};   // 删除选中
    QPushButton *saveButton_{nullptr};        // 保存到相机
    QPushButton *generateDcpButton_{nullptr}; // 生成dcp
    QPushButton *readDcpButton_{nullptr};     // 读取dcp
    QPushButton *toggleMarkersButton_{nullptr}; // 显示/隐藏坏点标记

    bool markersVisible_{true};
};

#endif // BAD_PIXEL_TOOL_DIALOG_H
