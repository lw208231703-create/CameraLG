#ifndef DETAIL_HISTOGRAM_WIDGET_H
#define DETAIL_HISTOGRAM_WIDGET_H

#include <QWidget>
#include <QVector>
#include <QMouseEvent>
#include <QEvent>
#include <QPaintEvent>
#include <QColor>
#include <opencv2/opencv.hpp>

class DetailHistogramWidget : public QWidget
{
public:
    explicit DetailHistogramWidget(QWidget* parent = nullptr);

    void setData(const cv::Mat& hist, double minVal, double maxVal);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QVector<int> m_histogramData;
    int m_maxCount{0};
    int m_minValIndex{0};
    int m_maxValIndex{65535};
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

#endif // DETAIL_HISTOGRAM_WIDGET_H
