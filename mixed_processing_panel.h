#ifndef MIXED_PROCESSING_PANEL_H
#define MIXED_PROCESSING_PANEL_H

#include <QWidget>
#include <QDialog>
#include <QListWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QMimeData>
#include <QDrag>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QTimer>
#include <QMutex>
#include <QImage>
#include <QVector>
#include <QVariantMap>
#include <QThread>
#include <QElapsedTimer>
#include <QIcon>
#include <QPixmap>
#include <QTransform>
#include <QPainter>
#include <QColor>
#include <atomic>
#include <opencv2/opencv.hpp>

#include "image_algorithm_base.h"
#include "zoomable_image_widget.h"

class ImageAlgorithmManager;
class MixedProcessingWorker;

struct PipelineStep {
    QString algorithmId;
    QVariantMap params;
    bool enabled;
};

struct CachedAlgorithm {
    QString algorithmId;
    QVariantMap lastParams;
    int lastInputType = -1;
    QSharedPointer<ImageAlgorithmBase> instance;
};

/**
 * @brief Worker class for mixed processing that runs on a dedicated thread
 */
class MixedProcessingWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit MixedProcessingWorker(QObject *parent = nullptr);
    ~MixedProcessingWorker();
    
public slots:
    void processImage(cv::Mat input, QVector<PipelineStep> steps);
    void stop();
    
signals:
    void processingFinished(cv::Mat result, qint64 elapsedMs);
    void processingError(QString error);
    
private:
    cv::Mat runPipeline(cv::Mat input, const QVector<PipelineStep> &steps);
    
    // Helper to update/create cached algorithm
    void updateAlgorithmCache(int index, const PipelineStep &step, int inputType);
    // Helper to execute algorithm
#ifdef CAMERUI_ENABLE_CUDA
    cv::cuda::GpuMat executeAlgorithmCuda(int index, const cv::cuda::GpuMat &input, const PipelineStep &step);
#endif

    std::atomic<bool> m_isProcessing{false};
    std::atomic<bool> m_shouldStop{false};

    // Optimization members
#ifdef CAMERUI_ENABLE_CUDA
    cv::Ptr<cv::cuda::Stream> m_cudaStream;
#endif
    QVector<CachedAlgorithm> m_algorithmCache;

    // Cache hardware availability to avoid per-frame driver queries
    bool m_cudaAvailable{false};
    bool m_openclAvailable{false};
};

/**
 * @brief Draggable algorithm item widget in the available algorithms list
 */
class DraggableAlgorithmItem : public QWidget
{
    Q_OBJECT
    
public:
    explicit DraggableAlgorithmItem(const AlgorithmInfo &info, QWidget *parent = nullptr);
    
    const AlgorithmInfo& algorithmInfo() const { return m_info; }
    
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    
private:
    AlgorithmInfo m_info;
    QPoint m_dragStartPosition;
};

/**
 * @brief Algorithm block widget placed in the processing pipeline
 * 
 * Represents a single algorithm in the processing chain with its parameters
 */
class AlgorithmBlockWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit AlgorithmBlockWidget(const AlgorithmInfo &info, QWidget *parent = nullptr);
    ~AlgorithmBlockWidget();
    
    const AlgorithmInfo& algorithmInfo() const { return m_info; }
    QString algorithmId() const { return m_info.id; }
    
    QVariantMap getParameters() const;
    void setParameters(const QVariantMap &params);
    
    bool isEnabled() const { return m_enabled; }
    void setBlockEnabled(bool enabled);
    
signals:
    void removeRequested();
    void parametersChanged();
    void enabledChanged(bool enabled);
    void moveUpRequested();
    void moveDownRequested();
    
private slots:
    void onParameterChanged();
    
private:
    void setupUI();
    QWidget* createParameterWidget(const AlgorithmParameter &param);
    
    AlgorithmInfo m_info;
    bool m_enabled{true};
    
    QVBoxLayout *m_mainLayout{nullptr};
    QLabel *m_titleLabel{nullptr};
    QCheckBox *m_enableCheckBox{nullptr};
    QWidget *m_paramsContainer{nullptr};
    QVBoxLayout *m_paramsLayout{nullptr};
    QPushButton *m_removeButton{nullptr};
    QPushButton *m_moveUpButton{nullptr};
    QPushButton *m_moveDownButton{nullptr};
    
    QMap<QString, QWidget*> m_paramWidgets;
};

/**
 * @brief Drop area for algorithm blocks - the pipeline workspace
 */
class PipelineDropArea : public QScrollArea
{
    Q_OBJECT
    
public:
    explicit PipelineDropArea(QWidget *parent = nullptr);
    
    void addAlgorithmBlock(AlgorithmBlockWidget *block);
    void removeAlgorithmBlock(AlgorithmBlockWidget *block);
    void moveBlockUp(AlgorithmBlockWidget *block);
    void moveBlockDown(AlgorithmBlockWidget *block);
    void clear();
    
    QVector<AlgorithmBlockWidget*> getBlocks() const { return m_blocks; }
    int blockCount() const { return m_blocks.size(); }
    
signals:
    void pipelineChanged();
    void blockAdded(AlgorithmBlockWidget *block);
    void blockRemoved(AlgorithmBlockWidget *block);
    
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    
private:
    void updateLayout();
    
    QWidget *m_container{nullptr};
    QHBoxLayout *m_layout{nullptr};
    QVector<AlgorithmBlockWidget*> m_blocks;
    
    QLabel *m_inputArrow{nullptr};
    QLabel *m_outputArrow{nullptr};
    QLabel *m_placeholderLabel{nullptr};
};

/**
 * @brief Available algorithms list widget
 */
class AlgorithmListWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit AlgorithmListWidget(QWidget *parent = nullptr);
    
    void setAlgorithmInfos(const QVector<AlgorithmInfo> &infos);
    
private:
    void setupUI();
    
    QVBoxLayout *m_mainLayout{nullptr};
    QScrollArea *m_scrollArea{nullptr};
    QWidget *m_container{nullptr};
};

/**
 * @brief Mixed Processing Dialog
 * 
 * Main dialog window for mixed processing with:
 * - Original and processed image comparison at the top
 * - Available algorithms list at the bottom
 * - Pipeline workspace in the middle
 */
class MixedProcessingDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit MixedProcessingDialog(QWidget *parent = nullptr);
    ~MixedProcessingDialog();
    
    void setAlgorithmManager(ImageAlgorithmManager *manager);
    
    enum class PrecisionMode {
        Standard8Bit = 0,
        HighPrecision16Bit = 1
    };
    
    void setPrecisionMode(PrecisionMode mode);
    PrecisionMode precisionMode() const { return m_precisionMode; }
    
public slots:
    /**
     * @brief Process raw image data
     */
    void processRawImage(const QVector<uint16_t> &rawData, int width, int height, int bitDepth);
    
    /**
     * @brief Process 8-bit image
     */
    void processImage(const QImage &image);
    
signals:
    void closed();
    void precisionModeChanged(PrecisionMode mode);
    
protected:
    void closeEvent(QCloseEvent *event) override;
    
private slots:
    void onPipelineChanged();
    void onPrecisionModeChanged(int index);
    void onProcessingTimerTimeout();
    void onApplyClicked();
    void onClearClicked();
    void onFrameRateLimitChanged();
    void onProcessingFinished(cv::Mat result, qint64 elapsedMs);
    void onProcessingError(QString error);
    
private:
    void setupUI();
    void populateAlgorithms();
    void processCurrentImage();
    void updateDisplay(const cv::Mat &original, const cv::Mat &processed);
    
    ImageAlgorithmManager *m_algorithmManager{nullptr};
    PrecisionMode m_precisionMode{PrecisionMode::Standard8Bit};
    
    // Current input image
    // We buffer the latest input and only convert/clone when we actually run processing
    QVector<uint16_t> m_latestRawData;
    int m_latestRawWidth{0};
    int m_latestRawHeight{0};
    int m_latestRawBitDepth{0};
    bool m_hasRawInput{false};

    QImage m_latestImage;
    bool m_hasQImageInput{false};

    // The last submitted input (for display update after async processing completes)
    cv::Mat m_lastSubmittedInputMat;
    int m_lastSubmittedBitDepth{0};

    QMutex m_imageMutex;
    bool m_needsProcessing{false};
    
    // Processing timer for debouncing
    QTimer *m_processTimer{nullptr};
    
    // Background processing with dedicated worker thread
    QThread *m_workerThread{nullptr};
    MixedProcessingWorker *m_worker{nullptr};
    std::atomic<bool> m_isProcessing{false};
    
    // Frame rate throttling
    int m_fpsLimit{30};               // 0 = unlimited
    QElapsedTimer m_throttleClock;
    qint64 m_lastProcessedNs{0};
    
    // UI components
    QVBoxLayout *m_mainLayout{nullptr};
    
    // Top: Image comparison
    QHBoxLayout *m_imagesLayout{nullptr};
    ZoomableImageWidget *m_originalImageWidget{nullptr};
    ZoomableImageWidget *m_processedImageWidget{nullptr};
    QLabel *m_originalInfoLabel{nullptr};
    QLabel *m_processedInfoLabel{nullptr};
    
    // Middle: Pipeline workspace
    PipelineDropArea *m_pipelineArea{nullptr};
    
    // Bottom: Algorithm list
    AlgorithmListWidget *m_algorithmList{nullptr};
    
    // Controls
    QComboBox *m_precisionCombo{nullptr};
    QLabel *m_fpsLabel{nullptr};
    QLabel *m_statsLabel{nullptr};
    QPushButton *m_clearButton{nullptr};
    QPushButton *m_closeButton{nullptr};
    
    // Frame rate controls
    QLabel *m_frameRateLabel{nullptr};
    QComboBox *m_frameRateCombo{nullptr};
    QSpinBox *m_customFpsSpin{nullptr};

    // FPS calculation
    qint64 m_lastFpsUpdateTime{0};
    int m_frameCount{0};
};

#endif // MIXED_PROCESSING_PANEL_H
