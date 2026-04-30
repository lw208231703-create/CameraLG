#ifndef DEVICE_DOCK_H
#define DEVICE_DOCK_H

#include <QDockWidget>

class DeviceSelectorWidget;

class DeviceDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit DeviceDock(QWidget *parent = nullptr);

    DeviceSelectorWidget *selector() const;

private:
    DeviceSelectorWidget *selector_ = nullptr;
};

#endif // DEVICE_DOCK_H
