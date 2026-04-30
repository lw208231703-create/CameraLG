#include "spot_tracking_chart_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <cmath>

// ============================================================================
// SpotTrackingChartDialog 实现
// ============================================================================

SpotTrackingChartDialog::SpotTrackingChartDialog(int spotCount, QWidget *parent)
    : QDialog(parent)
    , spotCount_(qBound(1, spotCount, 3))
    , maxPoints_(10)
    , autoYRange_(true)
    , yMin_(0.0)
    , yMax_(100.0)
    , frameRateLimited_(true)
    , targetFrameRate_(30)
    , mainLayout_(nullptr)
    , chartsScrollArea_(nullptr)
    , chartsContainer_(nullptr)
    , chartsLayout_(nullptr)
    , settingsGroup_(nullptr)
    , maxPointsSpinBox_(nullptr)
    , autoYRangeCheckBox_(nullptr)
    , yMinSpinBox_(nullptr)
    , yMaxSpinBox_(nullptr)
    , frameRateComboBox_(nullptr)
    , customFrameRateSpinBox_(nullptr)
    , updateTimer_(nullptr)
{
    setWindowTitle(tr("光斑跟踪分析图"));
    setMinimumSize(600, 400);
    resize(800, 600);

    setupUI();
    createCharts();
    
    // 图表在 addDataPoint 时实时刷新，无需定时器
    updateTimer_ = nullptr;
}

SpotTrackingChartDialog::~SpotTrackingChartDialog()
{
    if (updateTimer_) {
        updateTimer_->stop();
    }
}

void SpotTrackingChartDialog::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(10, 10, 10, 10);
    mainLayout_->setSpacing(10);

    // 设置区域 - 单行布局
    auto *settingsWidget = new QWidget(this);
    auto *settingsLayout = new QHBoxLayout(settingsWidget);
    settingsLayout->setContentsMargins(5, 5, 5, 5);
    settingsLayout->setSpacing(12);

    // X轴点数设置
    auto *xAxisLabel = new QLabel(tr("数据点数:"), settingsWidget);
    maxPointsSpinBox_ = new QSpinBox(settingsWidget);
    maxPointsSpinBox_->setRange(5, 100);
    maxPointsSpinBox_->setValue(maxPoints_);
    maxPointsSpinBox_->setSuffix(tr(" 个"));
    maxPointsSpinBox_->setMinimumWidth(70);
    connect(maxPointsSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SpotTrackingChartDialog::onMaxPointsChanged);

    // Y轴范围设置
    autoYRangeCheckBox_ = new QCheckBox(tr("自动Y轴"), settingsWidget);
    autoYRangeCheckBox_->setChecked(autoYRange_);
    connect(autoYRangeCheckBox_, &QCheckBox::toggled,
            this, &SpotTrackingChartDialog::onYAxisAutoChanged);

    auto *yRangeLabel = new QLabel(tr("Y范围:"), settingsWidget);
    yMinSpinBox_ = new QDoubleSpinBox(settingsWidget);
    yMinSpinBox_->setRange(-10000.0, 10000.0);
    yMinSpinBox_->setValue(yMin_);
    yMinSpinBox_->setDecimals(2);
    yMinSpinBox_->setMinimumWidth(75);
    yMinSpinBox_->setEnabled(!autoYRange_);
    connect(yMinSpinBox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpotTrackingChartDialog::onYAxisMinChanged);

    auto *yRangeSep = new QLabel(tr("~"), settingsWidget);
    
    yMaxSpinBox_ = new QDoubleSpinBox(settingsWidget);
    yMaxSpinBox_->setRange(-10000.0, 10000.0);
    yMaxSpinBox_->setValue(yMax_);
    yMaxSpinBox_->setDecimals(2);
    yMaxSpinBox_->setMinimumWidth(75);
    yMaxSpinBox_->setEnabled(!autoYRange_);
    connect(yMaxSpinBox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpotTrackingChartDialog::onYAxisMaxChanged);

    // 帧率设置
    auto *frameRateLabel = new QLabel(tr("帧率:"), settingsWidget);
    frameRateComboBox_ = new QComboBox(settingsWidget);
    frameRateComboBox_->addItem(tr("30 FPS"), 30);
    frameRateComboBox_->addItem(tr("60 FPS"), 60);
    frameRateComboBox_->addItem(tr("120 FPS"), 120);
    frameRateComboBox_->addItem(tr("360 FPS"), 360);
    frameRateComboBox_->addItem(tr("无限制"), 0);
    frameRateComboBox_->addItem(tr("自定义"), -1);
    frameRateComboBox_->setMinimumWidth(90);
    frameRateComboBox_->setCurrentIndex(0);
    connect(frameRateComboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpotTrackingChartDialog::onFrameRatePresetChanged);
    
    customFrameRateSpinBox_ = new QSpinBox(settingsWidget);
    customFrameRateSpinBox_->setRange(1, 1000);
    customFrameRateSpinBox_->setValue(30);
    customFrameRateSpinBox_->setSuffix(tr(" FPS"));
    customFrameRateSpinBox_->setMinimumWidth(80);
    customFrameRateSpinBox_->setVisible(false);
    connect(customFrameRateSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SpotTrackingChartDialog::onCustomFrameRateChanged);

    // 布局
    settingsLayout->addWidget(xAxisLabel);
    settingsLayout->addWidget(maxPointsSpinBox_);
    settingsLayout->addSpacing(8);
    settingsLayout->addWidget(autoYRangeCheckBox_);
    settingsLayout->addSpacing(8);
    settingsLayout->addWidget(yRangeLabel);
    settingsLayout->addWidget(yMinSpinBox_);
    settingsLayout->addWidget(yRangeSep);
    settingsLayout->addWidget(yMaxSpinBox_);
    settingsLayout->addStretch();
    settingsLayout->addWidget(frameRateLabel);
    settingsLayout->addWidget(frameRateComboBox_);
    settingsLayout->addWidget(customFrameRateSpinBox_);

    mainLayout_->addWidget(settingsWidget);

    // 图表区域（使用滚动区域以支持多个图表）
    chartsScrollArea_ = new QScrollArea(this);
    chartsScrollArea_->setWidgetResizable(true);
    chartsScrollArea_->setFrameShape(QFrame::NoFrame);

    chartsContainer_ = new QWidget(chartsScrollArea_);
    chartsLayout_ = new QVBoxLayout(chartsContainer_);
    chartsLayout_->setContentsMargins(0, 0, 0, 0);
    chartsLayout_->setSpacing(10);

    chartsScrollArea_->setWidget(chartsContainer_);
    mainLayout_->addWidget(chartsScrollArea_, 1);
}

void SpotTrackingChartDialog::createCharts()
{
    // 清除现有图表
    for (auto *chart : chartWidgets_) {
        chartsLayout_->removeWidget(chart);
        chart->deleteLater();
    }
    chartWidgets_.clear();

    // 创建新图表
    for (int i = 0; i < spotCount_; ++i) {
        auto *chart = new SpotTrackingChartWidget(i, chartsContainer_);
        chart->setMaxPoints(maxPoints_);
        chart->setAutoYRange(autoYRange_);
        if (!autoYRange_) {
            chart->setYRange(yMin_, yMax_);
        }
        chart->setFrameRateLimit(frameRateLimited_, targetFrameRate_);
        chart->setMinimumHeight(200);
        chartWidgets_.append(chart);
        chartsLayout_->addWidget(chart);
    }

    chartsLayout_->addStretch();
}

void SpotTrackingChartDialog::setSpotCount(int count)
{
    int newCount = qBound(1, count, 3);
    if (newCount != spotCount_) {
        spotCount_ = newCount;
        createCharts();
    }
}

void SpotTrackingChartDialog::addDataPoint(int spotIndex, float x, float y)
{
    if (spotIndex >= 0 && spotIndex < chartWidgets_.size()) {
        chartWidgets_[spotIndex]->addDataPoint(x, y);
    }
}

void SpotTrackingChartDialog::onYAxisAutoChanged(bool autoRange)
{
    autoYRange_ = autoRange;
    yMinSpinBox_->setEnabled(!autoRange);
    yMaxSpinBox_->setEnabled(!autoRange);

    for (auto *chart : chartWidgets_) {
        chart->setAutoYRange(autoRange);
        if (!autoRange) {
            chart->setYRange(yMin_, yMax_);
        }
    }
}

void SpotTrackingChartDialog::onYAxisMinChanged(double min)
{
    yMin_ = min;
    if (!autoYRange_) {
        for (auto *chart : chartWidgets_) {
            chart->setYRange(yMin_, yMax_);
        }
    }
}

void SpotTrackingChartDialog::onYAxisMaxChanged(double max)
{
    yMax_ = max;
    if (!autoYRange_) {
        for (auto *chart : chartWidgets_) {
            chart->setYRange(yMin_, yMax_);
        }
    }
}

void SpotTrackingChartDialog::onMaxPointsChanged(int count)
{
    maxPoints_ = count;
    for (auto *chart : chartWidgets_) {
        chart->setMaxPoints(count);
    }
}

void SpotTrackingChartDialog::onFrameRatePresetChanged(int index)
{
    int value = frameRateComboBox_->currentData().toInt();
    
    if (value == -1) {
        // 自定义模式
        customFrameRateSpinBox_->setVisible(true);
        frameRateLimited_ = true;
        targetFrameRate_ = customFrameRateSpinBox_->value();
    } else if (value == 0) {
        // 无限制模式
        customFrameRateSpinBox_->setVisible(false);
        frameRateLimited_ = false;
        targetFrameRate_ = 0;
    } else {
        // 预设模式（30/60/120/360）
        customFrameRateSpinBox_->setVisible(false);
        frameRateLimited_ = true;
        targetFrameRate_ = value;
    }
    
    // 更新所有图表
    for (auto *chart : chartWidgets_) {
        chart->setFrameRateLimit(frameRateLimited_, targetFrameRate_);
    }
}

void SpotTrackingChartDialog::onCustomFrameRateChanged(int fps)
{
    if (frameRateComboBox_->currentData().toInt() == -1) {
        // 只有在自定义模式下才响应
        targetFrameRate_ = fps;
        frameRateLimited_ = true;
        
        for (auto *chart : chartWidgets_) {
            chart->setFrameRateLimit(true, fps);
        }
    }
}

// ============================================================================
// SpotTrackingChartWidget 实现
// ============================================================================

SpotTrackingChartWidget::SpotTrackingChartWidget(int spotIndex, QWidget *parent)
    : QWidget(parent)
    , spotIndex_(spotIndex)
    , maxPoints_(10)
    , autoYRange_(true)
    , yMin_(0.0)
    , yMax_(100.0)
    , frameRateLimited_(false)
    , minUpdateIntervalMs_(16)
{
    setMinimumSize(400, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    lastUpdateTimer_.start();
}

SpotTrackingChartWidget::~SpotTrackingChartWidget() = default;

void SpotTrackingChartWidget::addDataPoint(float x, float y)
{
    xData_.append(x);
    yData_.append(y);

    // 保持数据点数不超过最大值
    while (xData_.size() > maxPoints_) {
        xData_.removeFirst();
        yData_.removeFirst();
    }

    if (autoYRange_) {
        updateYRange();
    }

    // 帧率限制
    if (frameRateLimited_) {
        if (lastUpdateTimer_.elapsed() >= minUpdateIntervalMs_) {
            update();
            lastUpdateTimer_.restart();
        }
        // 否则跳过此次更新，数据已保存，下次会绘制
    } else {
        // 无限制模式，每次都更新
        update();
    }
}

void SpotTrackingChartWidget::setMaxPoints(int count)
{
    maxPoints_ = qMax(5, count);
    
    // 裁剪现有数据
    while (xData_.size() > maxPoints_) {
        xData_.removeFirst();
        yData_.removeFirst();
    }

    if (autoYRange_) {
        updateYRange();
    }

    update();
}

void SpotTrackingChartWidget::setYRange(double min, double max)
{
    yMin_ = min;
    yMax_ = max;
    update();
}

void SpotTrackingChartWidget::setAutoYRange(bool autoRange)
{
    autoYRange_ = autoRange;
    if (autoRange) {
        updateYRange();
    }
    update();
}
void SpotTrackingChartWidget::setFrameRateLimit(bool enabled, int fps)
{
    frameRateLimited_ = enabled;
    if (enabled && fps > 0) {
        minUpdateIntervalMs_ = 1000 / fps;  // 转换为毫秒间隔
    }
    lastUpdateTimer_.restart();
}
void SpotTrackingChartWidget::clearData()
{
    xData_.clear();
    yData_.clear();
    if (autoYRange_) {
        yMin_ = 0.0;
        yMax_ = 100.0;
    }
    update();
}

void SpotTrackingChartWidget::updateYRange()
{
    if (xData_.isEmpty()) {
        yMin_ = 0.0;
        yMax_ = 100.0;
        return;
    }

    // 找出所有数据的最小最大值
    float minVal = std::numeric_limits<float>::max();
    float maxVal = std::numeric_limits<float>::lowest();

    for (float v : xData_) {
        minVal = qMin(minVal, v);
        maxVal = qMax(maxVal, v);
    }
    for (float v : yData_) {
        minVal = qMin(minVal, v);
        maxVal = qMax(maxVal, v);
    }

    // 添加一些边距
    float range = maxVal - minVal;
    if (range < 0.001f) {
        range = 1.0f;
    }
    float margin = range * 0.1f;
    yMin_ = minVal - margin;
    yMax_ = maxVal + margin;
}

QPointF SpotTrackingChartWidget::dataToScreen(int index, float value, bool /*isX*/) const
{
    int plotWidth = width() - MARGIN_LEFT - MARGIN_RIGHT;
    int plotHeight = height() - MARGIN_TOP - MARGIN_BOTTOM;

    // X位置：根据索引计算
    double xRatio = (xData_.size() > 1) ? static_cast<double>(index) / (xData_.size() - 1) : 0.5;
    double screenX = MARGIN_LEFT + xRatio * plotWidth;

    // Y位置：根据值计算
    double yRange = yMax_ - yMin_;
    if (std::abs(yRange) < 0.0001) {
        yRange = 1.0;
    }
    double yRatio = (value - yMin_) / yRange;
    double screenY = height() - MARGIN_BOTTOM - yRatio * plotHeight;

    return QPointF(screenX, screenY);
}

void SpotTrackingChartWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int plotLeft = MARGIN_LEFT;
    int plotTop = MARGIN_TOP;
    int plotWidth = width() - MARGIN_LEFT - MARGIN_RIGHT;
    int plotHeight = height() - MARGIN_TOP - MARGIN_BOTTOM;

    // 绘制背景
    painter.fillRect(rect(), palette().window());

    // 绘制图表区域背景
    QRect plotRect(plotLeft, plotTop, plotWidth, plotHeight);
    painter.fillRect(plotRect, QColor(30, 30, 30));

    // 绘制标题
    painter.setPen(QColor(212, 212, 212));
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter.setFont(titleFont);
    QString title = tr("光斑 %1 坐标跟踪").arg(spotIndex_ + 1);
    painter.drawText(QRect(0, 5, width(), 20), Qt::AlignCenter, title);

    // 绘制Y轴标签
    QFont labelFont = painter.font();
    labelFont.setBold(false);
    labelFont.setPointSize(8);
    painter.setFont(labelFont);

    // Y轴刻度
    const int yTicks = 5;
    for (int i = 0; i <= yTicks; ++i) {
        double yRatio = static_cast<double>(i) / yTicks;
        double yValue = yMin_ + yRatio * (yMax_ - yMin_);
        int yPos = height() - MARGIN_BOTTOM - static_cast<int>(yRatio * plotHeight);

        // 刻度线
        painter.setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));
        painter.drawLine(plotLeft, yPos, plotLeft + plotWidth, yPos);

        // 刻度值
        painter.setPen(QColor(212, 212, 212));
        QString label = QString::number(yValue, 'f', 1);
        painter.drawText(QRect(5, yPos - 10, MARGIN_LEFT - 10, 20), Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // X轴刻度（数据点序号）
    if (xData_.size() > 1) {
        const int xTicks = qMin(10, xData_.size());
        for (int i = 0; i < xTicks; ++i) {
            double xRatio = static_cast<double>(i) / (xTicks - 1);
            int xPos = plotLeft + static_cast<int>(xRatio * plotWidth);
            int dataIndex = static_cast<int>(xRatio * (xData_.size() - 1));

            // 刻度线
            painter.setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));
            painter.drawLine(xPos, plotTop, xPos, plotTop + plotHeight);

            // 刻度值
            painter.setPen(QColor(212, 212, 212));
            QString label = QString::number(dataIndex + 1);
            painter.drawText(QRect(xPos - 20, height() - MARGIN_BOTTOM + 5, 40, 20), Qt::AlignCenter, label);
        }
    }

    // 绘制数据曲线
    if (xData_.size() >= 2) {
        // X坐标曲线（蓝色）
        QPainterPath xPath;
        for (int i = 0; i < xData_.size(); ++i) {
            QPointF pt = dataToScreen(i, xData_[i], true);
            if (i == 0) {
                xPath.moveTo(pt);
            } else {
                xPath.lineTo(pt);
            }
        }
        painter.setPen(QPen(QColor(38, 192, 166), 2));
        painter.drawPath(xPath);

        // Y坐标曲线（红色）
        QPainterPath yPath;
        for (int i = 0; i < yData_.size(); ++i) {
            QPointF pt = dataToScreen(i, yData_[i], false);
            if (i == 0) {
                yPath.moveTo(pt);
            } else {
                yPath.lineTo(pt);
            }
        }
        painter.setPen(QPen(QColor(255, 100, 100), 2));
        painter.drawPath(yPath);
    }

    // 绘制图例
    int legendX = plotLeft + 10;
    int legendY = plotTop + 10;
    
    painter.setPen(QPen(QColor(38, 192, 166), 2));
    painter.drawLine(legendX, legendY, legendX + 20, legendY);
    painter.setPen(QColor(212, 212, 212));
    painter.drawText(legendX + 25, legendY + 4, tr("X坐标"));

    legendY += 15;
    painter.setPen(QPen(QColor(255, 100, 100), 2));
    painter.drawLine(legendX, legendY, legendX + 20, legendY);
    painter.setPen(QColor(212, 212, 212));
    painter.drawText(legendX + 25, legendY + 4, tr("Y坐标"));

    // 显示当前数据点数
    QString info = tr("数据点: %1/%2").arg(xData_.size()).arg(maxPoints_);
    painter.drawText(QRect(plotLeft + plotWidth - 100, plotTop + 5, 95, 20), Qt::AlignRight, info);
}

void SpotTrackingChartWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}
