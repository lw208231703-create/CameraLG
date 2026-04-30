
void ImageProcessingPanel::onAlgorithmEnabled(const QString &algorithmId)
{
    updateTreeItemState(algorithmId, true);
}

void ImageProcessingPanel::onAlgorithmDisabled(const QString &algorithmId)
{
    updateTreeItemState(algorithmId, false);
}
