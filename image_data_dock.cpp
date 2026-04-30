#include "image_data_dock.h"
#include "thread_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QRadioButton>
#include <QButtonGroup>

//HistogramWidget实现

HistogramWidget::HistogramWidget(QWidget *parent)
    : QWidget(parent)
{
    // 取消强制的最小尺寸限制，允许窗口在垂直方向上自由收缩
    setMinimumSize(0, 0);
    // 垂直方向也允许扩展/收缩，避免固定高度限制
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true); // 开启鼠标追踪
    m_histogramData.resize(256);
    m_histogramData.fill(0);
    
    // 初始化颜色 - 暗色主题
    backgroundColor_ = QColor(30, 30, 30);
    textColor_ = QColor(220, 220, 220);
    borderColor_ = QColor(100, 100, 100);
    gridColor_ = QColor(100, 100, 100);
    barColor_ = QColor(38, 192, 166);
    hoverColor_ = QColor(80, 80, 80);
}

void HistogramWidget::setFixedRange(bool fixed, int bitDepth)
{
    m_isFixedRange = fixed;
    m_bitDepth = bitDepth;
    // 重新计算范围并更新
    setHistogramData(m_histogramData);
}

void HistogramWidget::setHistogramData(const QVector<int> &data)
{
    if (data.isEmpty()) {
        return;
    }
    
    m_histogramData = data;
    
    // 找最大值用于归一化
    m_maxCount = 0;
    for (int count : m_histogramData) {
        if (count > m_maxCount) {
            m_maxCount = count;
        }
    }
    
    if (m_isFixedRange) {
        // 固定范围 0 到 (2^bitDepth - 1)
        m_minValIndex = 0;
        m_maxValIndex = (1 << m_bitDepth) - 1;
    } else {
        // 自动计算横轴范围（找到第一个和最后一个非零值）
        m_minValIndex = 0;
        m_maxValIndex = data.size() - 1;
        
        // 寻找最小值索引
        for (int i = 0; i < data.size(); ++i) {
            if (data[i] > 0) {
                m_minValIndex = i;
                break;
            }
        }
        
        // 寻找最大值索引
        for (int i = data.size() - 1; i >= 0; --i) {
            if (data[i] > 0) {
                m_maxValIndex = i;
                break;
            }
        }
        
        // 如果全是0或者范围异常，重置为全范围
        if (m_maxValIndex < m_minValIndex) {
            m_minValIndex = 0;
            m_maxValIndex = data.size() - 1;
        }
        
        // 稍微扩展一点范围，让显示不那么局促
        int range = m_maxValIndex - m_minValIndex;
        int padding = qMax(1, range / 20); // 5% padding
        
        m_minValIndex = qMax(0, m_minValIndex - padding);
        m_maxValIndex = qMin(data.size() - 1, m_maxValIndex + padding);
    }
    
    update();
}

void HistogramWidget::clearHistogram()
{
    m_histogramData.fill(0);
    m_maxCount = 0;
    m_minValIndex = 0;
    m_maxValIndex = 255;
    m_hoverIndex = -1;
    update();
}

void HistogramWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_hoverPos = event->pos();
    m_isHovering = true;
    
    // 布局参数 (需与paintEvent保持一致)
    int leftMargin = 50;
    int bottomMargin = 40;
    int rightMargin = 20;
    int topMargin = 20;
    QRect plotRect(leftMargin, topMargin, width() - leftMargin - rightMargin, height() - topMargin - bottomMargin);
    
    if (plotRect.contains(m_hoverPos)) {
        int displayRange = m_maxValIndex - m_minValIndex + 1;
        if (displayRange < 1) displayRange = 1;
        
        double ratio = static_cast<double>(m_hoverPos.x() - plotRect.left()) / plotRect.width();
        ratio = qBound(0.0, ratio, 1.0);
        
        int offset = static_cast<int>(ratio * displayRange);
        if (offset >= displayRange) offset = displayRange - 1;
        
        m_hoverIndex = m_minValIndex + offset;
    } else {
        m_hoverIndex = -1;
    }
    
    update();
}

void HistogramWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_isHovering = false;
    m_hoverIndex = -1;
    update();
}

void HistogramWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 背景
    painter.fillRect(rect(), backgroundColor_);
    
    // 绘制边框
    painter.setPen(QPen(borderColor_, 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    
    if (m_maxCount <= 0) {
        painter.setPen(textColor_);
        painter.drawText(rect(), Qt::AlignCenter, tr("无数据"));
        return;
    }
    
    // 布局参数
    int leftMargin = 50;   // Y轴标签空间
    int bottomMargin = 40; // X轴标签空间
    int rightMargin = 20;
    int topMargin = 20;
    
    QRect plotRect(leftMargin, topMargin, width() - leftMargin - rightMargin, height() - topMargin - bottomMargin);
    
    // 绘制坐标轴
    painter.setPen(QPen(textColor_, 1));
    // Y轴
    painter.drawLine(plotRect.topLeft(), plotRect.bottomLeft());
    // X轴
    painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());
    
    // 绘制网格线 (水平)
    painter.setPen(QPen(gridColor_, 1, Qt::DashLine));
    int numGridLines = 5;
    for (int i = 1; i <= numGridLines; ++i) {
        int y = plotRect.bottom() - (plotRect.height() * i / numGridLines);
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);
        
        // Y轴刻度值
        int val = m_maxCount * i / numGridLines;
        painter.setPen(textColor_);
        painter.drawText(QRect(0, y - 10, leftMargin - 5, 20), Qt::AlignRight | Qt::AlignVCenter, QString::number(val));
        painter.setPen(QPen(gridColor_, 1, Qt::DashLine));
    }
    
    // 统计非零bin数量
    m_nonZeroCount = 0;
    for (int cnt : m_histogramData) if (cnt > 0) ++m_nonZeroCount;

    // 绘制直方图
    painter.setPen(Qt::NoPen);
    painter.setBrush(barColor_);
    
    // 计算当前显示的范围
    int displayRange = m_maxValIndex - m_minValIndex + 1;
    if (displayRange < 1) displayRange = 1;
    
    double barWidth = static_cast<double>(plotRect.width()) / displayRange;
    
    // 如果显示范围内的数据点多于像素点，需要降采样绘制或者绘制线条
    if (displayRange > plotRect.width()) {
        // 绘制轮廓线
        painter.setPen(QPen(barColor_, 1));
        painter.setBrush(Qt::NoBrush);
        
        QPolygonF points;
        points.append(plotRect.bottomLeft());
        
        for (int x = 0; x < plotRect.width(); ++x) {
            // 映射 x 坐标到数据索引范围
            int startIdx = m_minValIndex + (x * displayRange / plotRect.width());
            int endIdx = m_minValIndex + ((x +1) * displayRange / plotRect.width());
            
            // 找到对应数据范围内的最大值
            int maxVal = 0;
            for (int i = startIdx; i < endIdx && i <= m_maxValIndex; ++i) {
                if (i >= 0 && i < m_histogramData.size()) {
                    if (m_histogramData[i] > maxVal) maxVal = m_histogramData[i];
                }
            }
            
            double normalizedHeight = static_cast<double>(maxVal) / m_maxCount * plotRect.height();
            points.append(QPointF(plotRect.left() + x, plotRect.bottom() - normalizedHeight));
        }
        points.append(plotRect.bottomRight());
        
        QColor fillColor = barColor_;
        fillColor.setAlpha(100);
        painter.setBrush(fillColor);
        painter.drawPolygon(points);
        
    } else {
        // 如果非零bins很少（如单像素模式），使用细线和小标记绘制，避免大面积红色矩形
        if (m_nonZeroCount <= 4) {
            painter.setPen(QPen(barColor_.darker(), 2));
            for (int i = 0; i < displayRange; ++i) {
                int dataIdx = m_minValIndex + i;
                if (dataIdx >= 0 && dataIdx < m_histogramData.size() && m_histogramData[dataIdx] > 0) {
                    double normalizedHeight = static_cast<double>(m_histogramData[dataIdx]) / m_maxCount * plotRect.height();
                    int x = plotRect.left() + static_cast<int>((i + 0.5) * barWidth);
                    int yTop = plotRect.bottom() - static_cast<int>(normalizedHeight);
                    // 细竖线
                    painter.drawLine(x, plotRect.bottom(), x, yTop);
                    // 小矩形标记在顶部
                    painter.fillRect(x - 3, yTop - 3, 6, 6, barColor_.darker());
                }
            }
        } else {
            // 绘制柱状图，柱子中心对齐刻度
            for (int i = 0; i < displayRange; ++i) {
                int dataIdx = m_minValIndex + i;
                if (dataIdx >= 0 && dataIdx < m_histogramData.size() && m_histogramData[dataIdx] > 0) {
                    double normalizedHeight = static_cast<double>(m_histogramData[dataIdx]) / m_maxCount * plotRect.height();
                    int barHeight = static_cast<int>(normalizedHeight);
                    // 柱子中心位置
                    int xCenter = plotRect.left() + static_cast<int>((i + 0.5) * barWidth);
                    int w = qMax(1, static_cast<int>(barWidth));
                    int x = xCenter - w / 2;  // 左边缘 = 中心 - 宽度/2
                    int y = plotRect.bottom() - barHeight;
                    
                    painter.fillRect(x, y, w, barHeight, painter.brush());
                }
            }
        }
    }
    
    // 绘制X轴刻度：动态显示多个刻度值，避免拥挤
    painter.setPen(textColor_);
    QLocale locale = QLocale::system();
    int leftTickVal = m_minValIndex;
    int rightTickVal = m_maxValIndex;
    int range = rightTickVal - leftTickVal;

    struct Tick { int x; int val; };
    QVector<Tick> ticks;
    
    // 根据范围大小动态决定显示的刻度数量
    int tickCount = 5;  // 默认显示5个刻度（左、中、右）
    if (range > 1000) {
        tickCount = 7;  // 范围大时显示7个刻度
    } else if (range > 500) {
        tickCount = 6;  // 中等范围显示6个刻度
    }
    
    // 生成均匀分布的刻度值
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
    
    // 绘制轴标题
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    
    // X轴标题
    painter.drawText(QRect(plotRect.left(), height() - 20, plotRect.width(), 20), Qt::AlignCenter, tr("像素值"));
    
    // 绘制鼠标悬停信息
    if (m_isHovering && m_hoverIndex != -1 && plotRect.contains(m_hoverPos)) {
        int displayRange = m_maxValIndex - m_minValIndex + 1;
        if (displayRange < 1) displayRange = 1;
        
        // 计算当前悬停位置对应的X坐标（柱子中心）
        double barWidth = static_cast<double>(plotRect.width()) / displayRange;
        int barIndex = m_hoverIndex - m_minValIndex;
        int x = plotRect.left() + static_cast<int>((barIndex + 0.5) * barWidth);
        
        // 绘制垂直指示线
        painter.setPen(QPen(barColor_, 1, Qt::DashLine));
        painter.drawLine(x, plotRect.top(), x, plotRect.bottom());
        
        // 获取当前值和计数
        int value = m_hoverIndex;
        int count = 0;
        if (value >= 0 && value < m_histogramData.size()) {
            count = m_histogramData[value];
        }
        
        // 绘制数据点
        if (count > 0) {
            double normalizedHeight = static_cast<double>(count) / m_maxCount * plotRect.height();
            int y = plotRect.bottom() - static_cast<int>(normalizedHeight);
            painter.setBrush(barColor_);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPoint(x, y), 4, 4);
        }
        
        // 绘制提示框
        QString text = QString("Val: %1\nCount: %2").arg(value).arg(count);
        
        // 计算提示框位置 (跟随鼠标，但防止出界)
        int tipWidth = 100;
        int tipHeight = 40;
        int tipX = m_hoverPos.x() + 10;
        int tipY = m_hoverPos.y() - 20;
        
        if (tipX + tipWidth > width()) tipX = m_hoverPos.x() - tipWidth - 10;
        if (tipY < 0) tipY = 0;
        
        QRect tipRect(tipX, tipY, tipWidth, tipHeight);
        
        // 绘制半透明背景
        QColor tipColor = backgroundColor_;
        tipColor.setAlpha(230);
        painter.setBrush(tipColor);
        painter.setPen(QPen(borderColor_, 1));
        painter.drawRect(tipRect);
        
        // 绘制文本
        painter.setPen(textColor_);
        painter.drawText(tipRect, Qt::AlignCenter, text);
    }
}

// ============ ImageDataDock 实现 ============

ImageDataDock::ImageDataDock(ThreadManager *threadManager, QWidget *parent)
    : QDockWidget(tr("图像数据"), parent)
    , contentWidget_(new QWidget(this))
    , contentLayout_(new QVBoxLayout)
{
    setObjectName(QStringLiteral("imageDataDock"));
    // 允许内容区域自由收缩，取消任何最小高度限制
    if (contentWidget_) {
        contentWidget_->setMinimumSize(0, 0);
        contentWidget_->setMinimumHeight(0);
    }

    contentLayout_->setContentsMargins(6, 6, 6, 6);
    contentLayout_->setSpacing(6);
    contentWidget_->setLayout(contentLayout_);
    setWidget(contentWidget_);
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    setupUI();
    
    // 注册类型
    qRegisterMetaType<QVector<uint16_t>>("QVector<uint16_t>");
    
    // 使用ThreadManager管理的分析工作线程
    if (threadManager) {
        analysisThread_ = threadManager->imageAnalysisThread();
        analysisWorker_ = threadManager->imageAnalysisWorker();
        
        // 连接信号
        connect(analysisWorker_, &ImageAnalysisWorker::analysisComplete,
                this, &ImageDataDock::onAnalysisComplete, Qt::QueuedConnection);
    } else {
        qWarning() << "ImageDataDock: ThreadManager is null, analysis functionality disabled";
    }
}

ImageDataDock::~ImageDataDock()
{
    // 停止工作线程
    if (analysisWorker_) {
        analysisWorker_->stop();
    }
    
    // 线程由ThreadManager管理，不需要在这里清理
    // 也不需要删除analysisWorker_，它由线程的deleteLater处理
}

void ImageDataDock::setupUI()
{
    // 标题和控制区域
    auto *headerWidget = new QWidget(contentWidget_);
    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    
    // 直方图标题
    auto *histogramLabel = new QLabel(tr("像素-数量直方图"), headerWidget);
    histogramLabel->setStyleSheet("font-weight: bold; color: #d4d4d4;");
    headerLayout->addWidget(histogramLabel);
    
    headerLayout->addStretch();
    
    // 范围控制
    autoScaleRadio_ = new QRadioButton(tr("自动调节"), headerWidget);
    fixedScaleRadio_ = new QRadioButton(tr("固定长度"), headerWidget);
    fixedScaleRadio_->setChecked(true);
    
    rangeGroup_ = new QButtonGroup(this);
    rangeGroup_->addButton(autoScaleRadio_, 0);
    rangeGroup_->addButton(fixedScaleRadio_, 1);
    
    connect(rangeGroup_, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &ImageDataDock::onRangeModeChanged);
            
    headerLayout->addWidget(autoScaleRadio_);
    headerLayout->addWidget(fixedScaleRadio_);
    
    contentLayout_->addWidget(headerWidget);
    
    // 直方图显示区域
    histogramWidget_ = new HistogramWidget(contentWidget_);
    contentLayout_->addWidget(histogramWidget_);
    
    // 统计数据区域（三列布局，列间用竖线分隔）
    auto *statsWidget = new QWidget(contentWidget_);
    auto *outerLayout = new QHBoxLayout(statsWidget);
    outerLayout->setContentsMargins(6, 6, 6, 6);
    outerLayout->setSpacing(8);

    const int titleMinWidth = 0;

    // 列1： 最小值 / 最大值 / 平均值
    auto *col1 = new QWidget(statsWidget);
    auto *col1Layout = new QVBoxLayout(col1);
    col1Layout->setContentsMargins(0, 0, 0, 0);
    col1Layout->setSpacing(8);

    auto addRow = [&](QVBoxLayout *vlayout, QLabel *&valueLabel, const QString &title){
        auto *row = new QWidget(col1);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);

        auto *titleLabel = new QLabel(title, row);
        titleLabel->setStyleSheet("color: #d4d4d4;");
        if (titleMinWidth > 0) titleLabel->setMinimumWidth(titleMinWidth);
        titleLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        valueLabel = new QLabel("--", row);
        valueLabel->setStyleSheet("font-weight: bold; color: #ffffff;");
        valueLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        h->addWidget(titleLabel);
        h->addWidget(valueLabel);
        vlayout->addWidget(row);
    };

    addRow(col1Layout, minValueLabel_, tr("最小值:"));
    addRow(col1Layout, maxValueLabel_, tr("最大值:"));
    addRow(col1Layout, meanLabel_, tr("平均值:"));

    outerLayout->addWidget(col1, 1);

    QFrame *vline1 = new QFrame(statsWidget);
    vline1->setFrameShape(QFrame::VLine);
    vline1->setFrameShadow(QFrame::Sunken);
    vline1->setLineWidth(1);
    outerLayout->addWidget(vline1);

    auto *col2 = new QWidget(statsWidget);
    auto *col2Layout = new QVBoxLayout(col2);
    col2Layout->setContentsMargins(0, 0, 0, 0);
    col2Layout->setSpacing(8);

    addRow(col2Layout, stdDevLabel_, tr("标准差:"));
    addRow(col2Layout, rangeLabel_, tr("极　差:"));
    addRow(col2Layout, varianceLabel_, tr("方　差:"));

    outerLayout->addWidget(col2, 1);

    QFrame *vline2 = new QFrame(statsWidget);
    vline2->setFrameShape(QFrame::VLine);
    vline2->setFrameShadow(QFrame::Sunken);
    vline2->setLineWidth(1);
    outerLayout->addWidget(vline2);

    auto *col3 = new QWidget(statsWidget);
    auto *col3Layout = new QVBoxLayout(col3);
    col3Layout->setContentsMargins(0, 0, 0, 0);
    col3Layout->setSpacing(8);
    outerLayout->addWidget(col3, 1);

    contentLayout_->addWidget(statsWidget);
}

void ImageDataDock::updateImage(const QImage &image)
{
    if (image.isNull() || !analysisWorker_) {
        return;
    }
    
    m_currentImage = image;
    m_currentRawData.clear(); // Clear raw data as we are using image
    
    triggerAnalysis();
}

void ImageDataDock::updateRawData(const QVector<uint16_t> &data, int width, int height, int bitDepth)
{
    if (data.isEmpty() || !analysisWorker_) {
        return;
    }
    
    m_currentRawData = data;
    m_rawWidth = width;
    m_rawHeight = height;
    m_rawBitDepth = bitDepth;
    m_currentImage = QImage(); // Clear image as we are using raw data
    
    triggerAnalysis();
}

void ImageDataDock::setRoi(const QRect &roi)
{
    m_currentRoi = roi;
    triggerAnalysis();
}

void ImageDataDock::setPinnedPoint(const QPoint &point, bool active)
{
    bool changed = (m_pinnedPoint != point) || (m_isPinned != active);
    m_pinnedPoint = point;
    m_isPinned = active;
    
    if (changed && analysisWorker_) {
        // 如果点改变了，重置历史数据
        QMetaObject::invokeMethod(analysisWorker_, "resetSinglePixelHistory", Qt::QueuedConnection);
    }
    
    triggerAnalysis();
}

void ImageDataDock::triggerAnalysis()
{
    if (!analysisWorker_) return;

    if (!m_currentRawData.isEmpty()) {
        if (!m_currentRoi.isEmpty()) {
            QMetaObject::invokeMethod(analysisWorker_, "analyzeRawData", 
                                      Qt::QueuedConnection, 
                                      Q_ARG(QVector<uint16_t>, m_currentRawData),
                                      Q_ARG(int, m_rawWidth),
                                      Q_ARG(int, m_rawHeight),
                                      Q_ARG(int, m_rawBitDepth),
                                      Q_ARG(QRect, m_currentRoi));
        } else if (m_isPinned) {
            QMetaObject::invokeMethod(analysisWorker_, "analyzeSinglePixelRaw", 
                                      Qt::QueuedConnection, 
                                      Q_ARG(QVector<uint16_t>, m_currentRawData),
                                      Q_ARG(int, m_rawWidth),
                                      Q_ARG(int, m_rawHeight),
                                      Q_ARG(int, m_rawBitDepth),
                                      Q_ARG(QPoint, m_pinnedPoint));
        } else {
            QMetaObject::invokeMethod(analysisWorker_, "analyzeRawData", 
                                      Qt::QueuedConnection, 
                                      Q_ARG(QVector<uint16_t>, m_currentRawData),
                                      Q_ARG(int, m_rawWidth),
                                      Q_ARG(int, m_rawHeight),
                                      Q_ARG(int, m_rawBitDepth),
                                      Q_ARG(QRect, QRect()));
        }
    } else if (!m_currentImage.isNull()) {
        if (!m_currentRoi.isEmpty()) {
            QMetaObject::invokeMethod(analysisWorker_, "analyzeImage", 
                                      Qt::QueuedConnection, 
                                      Q_ARG(QImage, m_currentImage),
                                      Q_ARG(QRect, m_currentRoi));
        } else if (m_isPinned) {
            QMetaObject::invokeMethod(analysisWorker_, "analyzeSinglePixel", 
                                      Qt::QueuedConnection, 
                                      Q_ARG(QImage, m_currentImage),
                                      Q_ARG(QPoint, m_pinnedPoint));
        } else {
            QMetaObject::invokeMethod(analysisWorker_, "analyzeImage", 
                                      Qt::QueuedConnection, 
                                      Q_ARG(QImage, m_currentImage),
                                      Q_ARG(QRect, QRect()));
        }
    }
}

void ImageDataDock::onAnalysisComplete(const ImageStatistics &stats)
{
    currentBitDepth_ = stats.bitDepth;
    updateStatisticsDisplay(stats);
}

void ImageDataDock::onRangeModeChanged(int id)
{
    if (histogramWidget_) {
        histogramWidget_->setFixedRange(id == 1, currentBitDepth_);
    }
}

void ImageDataDock::updateStatisticsDisplay(const ImageStatistics &stats)
{
    if (!stats.valid) {
        return;
    }
    
    // 更新直方图
    histogramWidget_->setHistogramData(stats.histogram);
    
    // 同步当前的范围模式设置
    if (rangeGroup_) {
        int currentMode = rangeGroup_->checkedId();
        histogramWidget_->setFixedRange(currentMode == 1, currentBitDepth_);
    }
    
    // 更新统计标签
    minValueLabel_->setText(QString::number(stats.minValue, 'f', 0));
    maxValueLabel_->setText(QString::number(stats.maxValue, 'f', 0));
    rangeLabel_->setText(QString::number(stats.range, 'f', 0));
    meanLabel_->setText(QString::number(stats.mean, 'f', 2));
    stdDevLabel_->setText(QString::number(stats.stdDev, 'f', 2));
    varianceLabel_->setText(QString::number(stats.variance, 'f', 2));
}

void ImageDataDock::resetSinglePixelStatistics()
{
    if (analysisWorker_) {
        // 重置单点像素的历史统计数据
        QMetaObject::invokeMethod(analysisWorker_, "resetSinglePixelHistory", Qt::QueuedConnection);
    }
}

void ImageDataDock::setHistorySize(int size)
{
    if (analysisWorker_) {
        analysisWorker_->setHistorySize(size);
    }
}
