#ifndef ICON_CACHE_H
#define ICON_CACHE_H

#include <QIcon>
#include <QPixmap>
#include <QString>

namespace IconCache {

inline const QIcon &applicationIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/launch_icon.png"));
    return icon;
}

inline const QPixmap &applicationPixmap()
{
    static const QPixmap pixmap(QStringLiteral(":/icons/launch_icon.png"));
    return pixmap;
}

inline const QIcon &continuousCaptureIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/Continuous_Capture.svg"));
    return icon;
}

inline const QPixmap &curtainFillPixmap()
{
    static const QPixmap pixmap(QStringLiteral(":/icons/Curtain_Fill.png"));
    return pixmap;
}

inline const QIcon &dataInputIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/Data_input.svg"));
    return icon;
}

inline const QIcon &saveSingleImageIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/Save_a_single_image.svg"));
    return icon;
}

inline const QIcon &saveMultipleImagesIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/Save_multiple_images.svg"));
    return icon;
}

inline const QIcon &oneToOneIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/one-to-one.svg"));
    return icon;
}

inline const QIcon &scanIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/Scan.svg"));
    return icon;
}

inline const QIcon &singleCaptureIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/Single capture.svg"));
    return icon;
}

inline const QIcon &stopCaptureIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/Stop_capture.svg"));
    return icon;
}

inline const QIcon &viewSettingsIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/View_Settings.svg"));
    return icon;
}

inline const QIcon &leftIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/left.svg"));
    return icon;
}

inline const QIcon &closeIcon()
{
    static const QIcon icon(QStringLiteral(":/icons/close.svg"));
    return icon;
}

} // namespace IconCache

#endif // ICON_CACHE_H
