#ifndef MOCK_DATA_PROVIDER_H
#define MOCK_DATA_PROVIDER_H

#include <QStringList>
#include <QVector>

#include "device_selector_widget.h"

class MockDataProvider
{
public:
    static QVector<DeviceSelectorWidget::DeviceInfo> sampleDevices();
    static QStringList sampleConfigurations();
};

#endif // MOCK_DATA_PROVIDER_H
