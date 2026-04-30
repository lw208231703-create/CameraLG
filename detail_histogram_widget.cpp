#include "detail_histogram_widget.h"

#include <QPainter>
#include <QPalette>
#include <QSizePolicy>
#include <QToolTip>
#include <QPolygonF>
#include <QLocale>
#include <QFont>
#include <cmath>

#include <opencv2/opencv.hpp>

DetailHistogramWidget::DetailHistogramWidget(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(400, 300);
    m_histogramData.resize(65536);
    m_histogramData.fill(0);
    
    // 初始化颜色 - 暗色主题
    backgroundColor_ = QColor(30, 30, 30);
    textColor_ = QColor(220, 220, 220);
    borderColor_ = QColor(100, 100, 100);
    gridColor_ = QColor(100, 100, 100);
    barColor_ = QColor(38, 192, 166);
    hoverColor_ = QColor(80, 80, 80);
}

void DetailHistogramWidget::setData(const cv::Mat& hist, double minVal, double maxVal)
{
    Q_UNUSED(minVal)
    Q_UNUSED(maxVal)

    m_histogramData.fill(0);
    m_histogramData.resize(65536);

    if (!hist.empty()) {
        for (int i = 0; i < hist.rows; ++i) {
            float binVal = hist.at<float>(i);
            m_histogramData[i] = static_cast<int>(binVal);
        }
    }

    m_maxCount = 0;
    for (int count : m_histogramData) {
        if (count > m_maxCount) {
            m_maxCount = count;
        }
    }

    m_minValIndex = 0;
    m_maxValIndex = m_histogramData.size() - 1;

    for (int i = 0; i < m_histogramData.size(); ++i) {
        if (m_histogramData[i] > 0) {
            m_minValIndex = i;
            break;
        }
    }

    for (int i = m_histogramData.size() - 1; i >= 0; --i) {
        if (m_histogramData[i] > 0) {
            m_maxValIndex = i;
            break;
        }
    }

    if (m_maxValIndex < m_minValIndex) {
        m_minValIndex = 0;
        m_maxValIndex = m_histogramData.size() - 1;
    }

    int range = m_maxValIndex - m_minValIndex;
    int padding = qMax(1, range / 20);

    m_minValIndex = qMax(0, m_minValIndex - padding);
    m_maxValIndex = qMin(m_histogramData.size() - 1, m_maxValIndex + padding);

    update();
}

void DetailHistogramWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), backgroundColor_);

    painter.setPen(QPen(borderColor_, 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    if (m_maxCount <= 0) {
        painter.setPen(textColor_);
        painter.drawText(rect(), Qt::AlignCenter, tr("无数据"));
        return;
    }

    int leftMargin = 60;
    int bottomMargin = 40;
    int rightMargin = 20;
    int topMargin = 40;

    QRect plotRect(leftMargin, topMargin, width() - leftMargin - rightMargin, height() - topMargin - bottomMargin);

    painter.setPen(textColor_);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(QRect(0, 0, width(), topMargin), Qt::AlignCenter,
                     QString("像素值分布 (范围: %1 - %2)").arg(m_minValIndex).arg(m_maxValIndex));

    painter.setPen(QPen(textColor_, 1));
    painter.drawLine(plotRect.topLeft(), plotRect.bottomLeft());
    painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());

    int targetTicks = 10;
    int step = 1;
    if (m_maxCount > 0) {
        double rawStep = static_cast<double>(m_maxCount) / targetTicks;
        double mag = std::pow(10, std::floor(std::log10(rawStep)));
        double normalizedStep = rawStep / mag;
        if (normalizedStep <= 1.0) step = 1 * mag;
        else if (normalizedStep <= 2.0) step = 2 * mag;
        else if (normalizedStep <= 5.0) step = 5 * mag;
        else step = 10 * mag;
    }
    if (step < 1) step = 1;

    int yAxisMax = ((m_maxCount + step - 1) / step) * step;
    if (yAxisMax == 0) yAxisMax = 1;

    painter.setPen(QPen(gridColor_, 1, Qt::DashLine));
    painter.setFont(QFont("Arial", 8));

    for (int v = step; v <= yAxisMax; v += step) {
        double normalizedHeight = static_cast<double>(v) / yAxisMax * plotRect.height();
        int y = plotRect.bottom() - qRound(normalizedHeight);

        painter.drawLine(plotRect.left(), y, plotRect.right(), y);

        painter.setPen(textColor_);
        painter.drawText(QRect(0, y - 10, leftMargin - 5, 20), Qt::AlignRight | Qt::AlignVCenter, QString::number(v));
        painter.setPen(QPen(gridColor_, 1, Qt::DashLine));
    }

    int nonZeroCount = 0;
    for (int cnt : m_histogramData) if (cnt > 0) ++nonZeroCount;

    painter.setPen(Qt::NoPen);
    painter.setBrush(barColor_);

    int displayRange = m_maxValIndex - m_minValIndex + 1;
    if (displayRange < 1) displayRange = 1;

    double barWidth = static_cast<double>(plotRect.width()) / displayRange;

    if (displayRange > plotRect.width()) {
        painter.setPen(QPen(barColor_, 1));
        painter.setBrush(Qt::NoBrush);

        QPolygonF points;
        points.append(plotRect.bottomLeft());

        for (int x = 0; x < plotRect.width(); ++x) {
            int startIdx = m_minValIndex + (x * displayRange / plotRect.width());
            int endIdx = m_minValIndex + ((x +1) * displayRange / plotRect.width());

            int maxVal = 0;
            for (int i = startIdx; i < endIdx && i <= m_maxValIndex; ++i) {
                if (i >= 0 && i < m_histogramData.size()) {
                    if (m_histogramData[i] > maxVal) maxVal = m_histogramData[i];
                }
            }

            double normalizedHeight = static_cast<double>(maxVal) / yAxisMax * plotRect.height();
            points.append(QPointF(plotRect.left() + x, plotRect.bottom() - normalizedHeight));
        }
        points.append(plotRect.bottomRight());

        QColor fillColor = barColor_;
        fillColor.setAlpha(100);
        painter.setBrush(fillColor);
        painter.drawPolygon(points);

    } else {
        if (nonZeroCount <= 4) {
            painter.setPen(QPen(barColor_.darker(), 2));
            for (int i = 0; i < displayRange; ++i) {
                int dataIdx = m_minValIndex + i;
                if (dataIdx >= 0 && dataIdx < m_histogramData.size() && m_histogramData[dataIdx] > 0) {
                    double normalizedHeight = static_cast<double>(m_histogramData[dataIdx]) / yAxisMax * plotRect.height();
                    int x = plotRect.left() + static_cast<int>((i + 0.5) * barWidth);
                    int yTop = plotRect.bottom() - static_cast<int>(normalizedHeight);
                    painter.drawLine(x, plotRect.bottom(), x, yTop);
                    painter.fillRect(x - 3, yTop - 3, 6, 6, barColor_.darker());
                }
            }
        } else {
            for (int i = 0; i < displayRange; ++i) {
                int dataIdx = m_minValIndex + i;
                if (dataIdx >= 0 && dataIdx < m_histogramData.size() && m_histogramData[dataIdx] > 0) {
                    double normalizedHeight = static_cast<double>(m_histogramData[dataIdx]) / yAxisMax * plotRect.height();
                    int barHeight = static_cast<int>(normalizedHeight);
                    int xCenter = plotRect.left() + static_cast<int>((i + 0.5) * barWidth);
                    int w = qMax(1, static_cast<int>(barWidth));
                    int x = xCenter - w / 2;
                    int y = plotRect.bottom() - barHeight;

                    painter.fillRect(x, y, w, barHeight, painter.brush());
                }
            }
        }
    }

    painter.setPen(textColor_);
    QLocale locale = QLocale::system();
    int leftTickVal = m_minValIndex;
    int rightTickVal = m_maxValIndex;
    int range = rightTickVal - leftTickVal;

    struct Tick { int x; int val; };
    QVector<Tick> ticks;

    int tickCount = 5;
    if (range > 1000) tickCount = 7;
    else if (range > 500) tickCount = 6;

    for (int i = 0; i < tickCount; ++i) {
        int val = leftTickVal + (range * i) / (tickCount - 1);
        int x = plotRect.left() + (plotRect.width() * i) / (tickCount - 1);
        ticks.append({ x, val });
    }

    for (const Tick &t : ticks) {
        painter.drawLine(t.x, plotRect.bottom(), t.x, plotRect.bottom() + 5);
        QString label = locale.toString(t.val);
        QRect textRect(t.x - 40, plotRect.bottom() + 5, 80, 20);
        painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, label);
    }

    if (m_hoverIndex >= 0 && m_hoverIndex < m_histogramData.size()) {
        int count = m_histogramData[m_hoverIndex];

        double barWidth = static_cast<double>(plotRect.width()) / displayRange;
        int offset = m_hoverIndex - m_minValIndex;
        int x = plotRect.left() + static_cast<int>((offset + 0.5) * barWidth);

        if (x >= plotRect.left() && x <= plotRect.right()) {
            painter.setPen(QPen(barColor_, 1, Qt::DashLine));
            painter.drawLine(x, plotRect.top(), x, plotRect.bottom());

            double normalizedHeight = static_cast<double>(count) / yAxisMax * plotRect.height();
            int y = plotRect.bottom() - static_cast<int>(normalizedHeight);

            painter.setBrush(barColor_);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPoint(x, y), 3, 3);

            QString text = QString("像素值: %1\n统计数: %2").arg(m_hoverIndex).arg(count);
            QToolTip::showText(mapToGlobal(m_hoverPos), text, this);
        }
    } else {
        QToolTip::hideText();
    }
}

void DetailHistogramWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_hoverPos = event->pos();

    int leftMargin = 60;
    int bottomMargin = 40;
    int rightMargin = 20;
    int topMargin = 40;
    QRect plotRect(leftMargin, topMargin, width() - leftMargin - rightMargin, height() - topMargin - bottomMargin);

    if (plotRect.contains(m_hoverPos)) {
        int displayRange = m_maxValIndex - m_minValIndex + 1;
        if (displayRange < 1) displayRange = 1;

        if (displayRange > plotRect.width()) {
            int x = m_hoverPos.x() - plotRect.left();

            int startIdx = m_minValIndex + (x * displayRange / plotRect.width());
            int endIdx = m_minValIndex + ((x + 1) * displayRange / plotRect.width());

            int maxVal = -1;
            int maxIdx = -1;

            for (int i = startIdx; i < endIdx && i <= m_maxValIndex; ++i) {
                if (i >= 0 && i < m_histogramData.size()) {
                    if (m_histogramData[i] > maxVal) {
                        maxVal = m_histogramData[i];
                        maxIdx = i;
                    }
                }
            }

            if (maxIdx != -1) {
                m_hoverIndex = maxIdx;
            } else {
                double ratio = static_cast<double>(x) / plotRect.width();
                int offset = static_cast<int>(ratio * displayRange);
                m_hoverIndex = m_minValIndex + offset;
            }
        } else {
            double ratio = static_cast<double>(m_hoverPos.x() - plotRect.left()) / plotRect.width();
            ratio = qBound(0.0, ratio, 1.0);

            int offset = static_cast<int>(ratio * displayRange);
            if (offset >= displayRange) offset = displayRange - 1;

            m_hoverIndex = m_minValIndex + offset;
        }
    } else {
        m_hoverIndex = -1;
    }

    update();
}

void DetailHistogramWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    m_hoverIndex = -1;
    update();
}
