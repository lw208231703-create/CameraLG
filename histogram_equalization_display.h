#ifndef HISTOGRAM_EQUALIZATION_DISPLAY_H
#define HISTOGRAM_EQUALIZATION_DISPLAY_H

#include <QWidget>
#include <QLabel>
#include <QImage>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QCheckBox>
#include "zoomable_image_widget.h"

/**
 * @brief 直方图均衡化结果显示窗口
 * 
 * 独立窗口，实时显示均衡化处理前后的图像对比
 */
class HistogramEqualizationDisplay : public QWidget
{
    Q_OBJECT
    
public:
    explicit HistogramEqualizationDisplay(QWidget *parent = nullptr);
    ~HistogramEqualizationDisplay();
    
public slots:
    /**
     * @brief 更新显示的图像
     * @param equalizedImage 均衡化后的图像
     * @param originalImage 原始图像
     */
    void updateImages(const QImage &equalizedImage, const QImage &originalImage);
    
    /**
     * @brief 更新统计信息
     * @param processingTime 处理耗时(ms)
     * @param fps 处理帧率
     */
    void updateStatistics(double processingTime, double fps);
    
    /**
     * @brief 显示错误信息
     * @param error 错误信息
     */
    void showError(const QString &error);
    
    /**
     * @brief 切换窗口固定状态
     */
    void togglePin();
    
signals:
    /**
     * @brief 窗口关闭信号
     */
    void windowClosed();
    
protected:
    void closeEvent(QCloseEvent *event) override;
    
private:
    void setupUI();
    
    // UI组件
    QVBoxLayout *mainLayout_;
    QHBoxLayout *imagesLayout_;
    
    // 原始图像显示
    ZoomableImageWidget *originalImageWidget_;
    QLabel *originalInfoLabel_;
    
    // 均衡化图像显示
    ZoomableImageWidget *equalizedImageWidget_;
    QLabel *equalizedInfoLabel_;
    
    // 统计信息
    QLabel *processingTimeLabel_;
    QLabel *fpsLabel_;
    QLabel *errorLabel_;
    
    // 控制按钮
    QHBoxLayout *buttonLayout_;
    QPushButton *closeButton_;
    QPushButton *pinButton_;
    
    // 固定状态
    bool isPinned_;
    
    // 缓存图像
    QImage currentEqualizedImage_;
    QImage currentOriginalImage_;
};

#endif // HISTOGRAM_EQUALIZATION_DISPLAY_H
