#include "device_dock.h"

#include "device_selector_widget.h"

DeviceDock::DeviceDock(QWidget *parent)
    : QDockWidget(tr("驱动配置"), parent)
    , selector_(new DeviceSelectorWidget(this))
{
    setObjectName(QStringLiteral("deviceDock"));
    selector_->setStyleSheet("QGroupBox { border: none; margin: 0; padding: 0; } QGroupBox::title { color: transparent; }");
    setWidget(selector_);
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
}

DeviceSelectorWidget *DeviceDock::selector() const
{
    return selector_;
}
