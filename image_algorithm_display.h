#ifndef IMAGE_ALGORITHM_DISPLAY_H
#define IMAGE_ALGORITHM_DISPLAY_H

#include <QWidget>
#include <QLabel>
#include <QImage>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include "zoomable_image_widget.h"

/**
 * @brief Generic algorithm result display window
 * 
 * Independent window that displays original and processed images side by side
 * for real-time comparison. Used by all image processing algorithms.
 */
class ImageAlgorithmDisplay : public QWidget
{
    Q_OBJECT
    
public:
    explicit ImageAlgorithmDisplay(const QString &algorithmName, QWidget *parent = nullptr);
    ~ImageAlgorithmDisplay();
    
public slots:
    /**
     * @brief Update displayed images
     * @param processedImage Processed image
     * @param originalImage Original image
     */
    void updateImages(const QImage &processedImage, const QImage &originalImage);
    
    /**
     * @brief Update processing statistics
     * @param processingTime Processing time in ms
     * @param fps Frames per second
     */
    void updateStatistics(double processingTime, double fps);
    
    /**
     * @brief Show error message
     * @param error Error message
     */
    void showError(const QString &error);
    
signals:
    /**
     * @brief Emitted when window is closed
     */
    void windowClosed();

    /**
     * @brief Frame rate limit changed.
     * @param fpsLimit 0 means unlimited; otherwise target FPS (e.g. 30/60/120/custom)
     */
    void frameRateLimitChanged(int fpsLimit);
    
protected:
    void closeEvent(QCloseEvent *event) override;
    
private:
    void setupUI();
    void emitCurrentFpsLimit();
    
    QString m_algorithmName;
    
    // UI components
    QVBoxLayout *mainLayout_;
    QHBoxLayout *imagesLayout_;
    
    // Original image display
    ZoomableImageWidget *originalImageWidget_;
    QLabel *originalInfoLabel_;
    
    // Processed image display
    ZoomableImageWidget *processedImageWidget_;
    QLabel *processedInfoLabel_;
    
    // Statistics
    QLabel *processingTimeLabel_;
    QLabel *fpsLabel_;
    QLabel *errorLabel_;

    // Frame rate control
    QLabel *frameRateLabel_{nullptr};
    QComboBox *frameRateCombo_{nullptr};
    QSpinBox *customFpsSpin_{nullptr};
    
    // Cached images
    QImage currentProcessedImage_;
    QImage currentOriginalImage_;
};

#endif // IMAGE_ALGORITHM_DISPLAY_H
