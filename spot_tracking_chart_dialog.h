#ifndef SPOT_TRACKING_CHART_DIALOG_H
#define SPOT_TRACKING_CHART_DIALOG_H

#include <QDialog>
#include <QVector>
#include <QTimer>
#include <QElapsedTimer>

class QVBoxLayout;
class QHBoxLayout;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QGroupBox;
class QPushButton;
class QScrollArea;

// Forward declaration of chart widget
class SpotTrackingChartWidget;

// 光斑跟踪分析图对话框
// 功能：显示滚动折线图，用于查看光斑跟踪的稳定性
class SpotTrackingChartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpotTrackingChartDialog(int spotCount, QWidget *parent = nullptr);
    ~SpotTrackingChartDialog();

    // 添加新的数据点
    void addDataPoint(int spotIndex, float x, float y);

    // 设置图表数量（1-3）
    void setSpotCount(int count);

public slots:
    // 更新Y轴范围设置
    void onYAxisAutoChanged(bool autoRange);
    void onYAxisMinChanged(double min);
    void onYAxisMaxChanged(double max);

    // 更新X轴点数设置
    void onMaxPointsChanged(int count);
    
    // 更新帧率限制设置
    void onFrameRatePresetChanged(int index);
    void onCustomFrameRateChanged(int fps);

private:
    void setupUI();
    void createCharts();
    void updateChartsLayout();

    int spotCount_;                     // 光斑数量（1-3）
    int maxPoints_;                     // X轴显示的最大数据点数
    bool autoYRange_;                   // 是否自动计算Y轴范围
    double yMin_;                       // Y轴最小值
    double yMax_;                       // Y轴最大值
    
    // 帧率限制
    bool frameRateLimited_;             // 是否启用帧率限制
    int targetFrameRate_;               // 目标帧率（FPS）

    QVBoxLayout *mainLayout_;
    QScrollArea *chartsScrollArea_;
    QWidget *chartsContainer_;
    QVBoxLayout *chartsLayout_;
    QVector<SpotTrackingChartWidget*> chartWidgets_;

    // 设置控件
    QGroupBox *settingsGroup_;
    QSpinBox *maxPointsSpinBox_;
    QCheckBox *autoYRangeCheckBox_;
    QDoubleSpinBox *yMinSpinBox_;
    QDoubleSpinBox *yMaxSpinBox_;
    QComboBox *frameRateComboBox_;
    QSpinBox *customFrameRateSpinBox_;

    QTimer *updateTimer_;
};

// 单个光斑的滚动折线图控件
class SpotTrackingChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpotTrackingChartWidget(int spotIndex, QWidget *parent = nullptr);
    ~SpotTrackingChartWidget();

    // 添加数据点
    void addDataPoint(float x, float y);

    // 设置最大数据点数
    void setMaxPoints(int count);

    // 设置Y轴范围
    void setYRange(double min, double max);
    void setAutoYRange(bool autoRange);

    // 设置帧率限制
    void setFrameRateLimit(bool enabled, int fps);

    // 清除数据
    void clearData();

    // 获取光斑索引
    int spotIndex() const { return spotIndex_; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateYRange();
    QPointF dataToScreen(int index, float value, bool isX) const;

    int spotIndex_;                     // 光斑索引（0-2）
    int maxPoints_;                     // 最大数据点数
    bool autoYRange_;                   // 自动Y轴范围
    double yMin_;                       // Y轴最小值
    double yMax_;                       // Y轴最大值
    
    // 帧率限制
    bool frameRateLimited_;             // 是否启用帧率限制
    int minUpdateIntervalMs_;           // 最小更新间隔（毫秒）
    QElapsedTimer lastUpdateTimer_;     // 上次更新计时器

    QVector<float> xData_;              // X坐标数据
    QVector<float> yData_;              // Y坐标数据

    // 绘图边距
    static constexpr int MARGIN_LEFT = 60;
    static constexpr int MARGIN_RIGHT = 20;
    static constexpr int MARGIN_TOP = 30;
    static constexpr int MARGIN_BOTTOM = 40;
};

#endif // SPOT_TRACKING_CHART_DIALOG_H
