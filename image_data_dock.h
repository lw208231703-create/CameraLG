#ifndef IMAGE_DATA_DOCK_H
#define IMAGE_DATA_DOCK_H

#include <QDockWidget>
#include <QImage>
#include <QThread>
#include "image_analysis_worker.h"

class QLabel;
class QVBoxLayout;
class QWidget;
class QRadioButton;
class QButtonGroup;
class ThreadManager;

// 直方图绘制Widget
class HistogramWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HistogramWidget(QWidget *parent = nullptr);
    
    void setHistogramData(const QVector<int> &data);
    void clearHistogram();
    void setFixedRange(bool fixed, int bitDepth); // 设置是否固定范围

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QVector<int> m_histogramData;
    int m_maxCount{0};
    int m_minValIndex{0};
    int m_maxValIndex{255};
    int m_nonZeroCount{0};
    bool m_isFixedRange{false}; // 是否固定范围 (0-16383)
    int m_bitDepth{14}; // 当前位深
    
    // 交互数据
    bool m_isHovering{false};
    int m_hoverIndex{-1};
    QPoint m_hoverPos;
    
    // 主题颜色
    QColor backgroundColor_;
    QColor textColor_;
    QColor borderColor_;
    QColor gridColor_;
    QColor barColor_;
    QColor hoverColor_;
};

// 图像数据分析停靠窗口
class ImageDataDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit ImageDataDock(ThreadManager *threadManager, QWidget *parent = nullptr);
    ~ImageDataDock() override;

public slots:
    // 更新图像进行分析
    void updateImage(const QImage &image);
    
    // 更新原始数据进行分析
    void updateRawData(const QVector<uint16_t> &data, int width, int height, int bitDepth);
    
    // 设置感兴趣区域
    void setRoi(const QRect &roi);
    
    // 设置单像素分析点
    void setPinnedPoint(const QPoint &point, bool active);
    
    // 重置单点像素统计数据（只统计当前缓冲区数据）
    void resetSinglePixelStatistics();

    // 设置历史记录长度
    void setHistorySize(int size);

    // 获取是否处于单点分析模式（且没有ROI覆盖）
    bool isSinglePixelHistoryMode() const { return m_isPinned && m_currentRoi.isEmpty(); }

private slots:
    // 接收分析结果
    void onAnalysisComplete(const ImageStatistics &stats);
    // 范围模式改变
    void onRangeModeChanged(int id);

private:
    void setupUI();
    void updateStatisticsDisplay(const ImageStatistics &stats);
    void triggerAnalysis();
    
    QWidget *contentWidget_{nullptr};
    QVBoxLayout *contentLayout_{nullptr};
    
    // 范围控制
    QRadioButton *autoScaleRadio_{nullptr};
    QRadioButton *fixedScaleRadio_{nullptr};
    QButtonGroup *rangeGroup_{nullptr};
    
    // 直方图显示
    HistogramWidget *histogramWidget_{nullptr};
    
    // 统计数据标签
    QLabel *minValueLabel_{nullptr};
    QLabel *maxValueLabel_{nullptr};
    QLabel *rangeLabel_{nullptr};
    QLabel *meanLabel_{nullptr};
    QLabel *stdDevLabel_{nullptr};
    QLabel *varianceLabel_{nullptr};
    
    // 分析工作线程
    QThread *analysisThread_{nullptr};
    ImageAnalysisWorker *analysisWorker_{nullptr};
    
    int currentBitDepth_{14}; // 当前图像的位深
    
    // 缓存数据用于ROI分析
    QImage m_currentImage;
    QVector<uint16_t> m_currentRawData;
    int m_rawWidth{0};
    int m_rawHeight{0};
    int m_rawBitDepth{0};
    QRect m_currentRoi;
    
    // 单像素分析
    QPoint m_pinnedPoint;
    bool m_isPinned{false};
};

#endif // IMAGE_DATA_DOCK_H
