#include "image_algorithm_dock.h"

#include <QSettings>
#include <QSplitter>
#include <QVariantList>

void ImageAlgorithmDock::saveUiState()
{
    if (!mainSplitter_) {
        return;
    }
    const QList<int> sizes = mainSplitter_->sizes();
    if (sizes.isEmpty()) {
        return;
    }

    QVariantList list;
    list.reserve(sizes.size());
    for (int s : sizes) list.push_back(s);

    QSettings settings("CameraUI", "MainWindow");
    settings.setValue("splitters/imageAlgorithmDockMain", list);
}

void ImageAlgorithmDock::restoreUiState()
{
    if (!mainSplitter_) {
        return;
    }

    QSettings settings("CameraUI", "MainWindow");
    const QVariantList list = settings.value("splitters/imageAlgorithmDockMain").toList();
    if (list.size() < 2) {
        return;
    }
    QList<int> sizes;
    sizes.reserve(list.size());
    for (const QVariant &it : list) sizes.push_back(it.toInt());
    mainSplitter_->setSizes(sizes);
}
