#ifndef BAD_PIXEL_PICKER_DIALOG_H
#define BAD_PIXEL_PICKER_DIALOG_H

#include <QDialog>
#include <QPoint>
#include <QVector>

class QLabel;
class QListWidget;
class QPushButton;
class QDialogButtonBox;
class DisplayDock;

class BadPixelPickerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BadPixelPickerDialog(DisplayDock *displayDock, QWidget *parent = nullptr);
    ~BadPixelPickerDialog() override;

    QVector<QPoint> points() const { return points_; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onPointPicked(const QPoint &imagePos);
    void onClearClicked();
    void onUndoClicked();

private:
    void setPickMode(bool enabled);
    void refreshList();

    DisplayDock *displayDock_{nullptr};
    QVector<QPoint> points_;

    QLabel *hintLabel_{nullptr};
    QListWidget *listWidget_{nullptr};
    QPushButton *clearButton_{nullptr};
    QPushButton *undoButton_{nullptr};
    QDialogButtonBox *buttonBox_{nullptr};
};

#endif // BAD_PIXEL_PICKER_DIALOG_H
