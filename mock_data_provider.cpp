#include "mock_data_provider.h"


QVector<DeviceSelectorWidget::DeviceInfo> MockDataProvider::sampleDevices()
{
    return {
        {QStringLiteral("Areascan A"), QStringLiteral("CameraLink")},
        {QStringLiteral("Areascan B"), QStringLiteral("GigE Vision")},
        {QStringLiteral("Linecam C"), QStringLiteral("CoaXPress")}
    };
}

QStringList MockDataProvider::sampleConfigurations()
{
    return {
        QStringLiteral("Factory Default"),
        QStringLiteral("Low Noise"),
        QStringLiteral("High Throughput")
    };
}
