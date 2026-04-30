#ifndef IMAGE_ALGORITHM_DOCK_H
#define IMAGE_ALGORITHM_DOCK_H

#include <QDockWidget>
#include <QThread>
#include <QObject>

#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVector>
#include <QTimer>

#include "noise_analysis_worker.h"
#include "thread_manager.h"

class QLabel;
class QCheckBox;
class QSpinBox;
class QPushButton;
class QGroupBox;
class QDoubleSpinBox;
class ImageAlgorithmManager;
class ImageProcessingPanel;
class QSplitter;
class MixedProcessingDialog;

class ImageAlgorithmWorker : public QObject
{
    Q_OBJECT
public:
    explicit ImageAlgorithmWorker(QObject *parent = nullptr);

public slots:
    void stop();

signals:
    void finished();
};

class ImageAlgorithmDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit ImageAlgorithmDock(ThreadManager *threadManager, QWidget *parent = nullptr);
    ~ImageAlgorithmDock() override;

    void setBitDepth(int bitDepth);
    void setBitShift(int shift) { bitShift_ = shift; }

    enum class ProcessingInputMode {
        Standard8Bit = 0,
        Raw16Bit = 1
    };
    void setProcessingInputMode(ProcessingInputMode mode) { processingInputMode_ = mode; }

    void setCameraSN(const QString &sn) { cameraSN_ = sn; }

    void onRawImageReceived(const QVector<uint16_t> &rawData, int width, int height, int bitDepth);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void stopWorkerRequested();

    void diagnosticMessage(const QString &message);

private:
    void setupUI();
    void setupThread();
    QWidget* createReadoutNoisePage();
    QWidget* createImageProcessingPage();
    QWidget* createMixedProcessingPage();
#if ENABLE_SPOT_DETECTION
    QWidget* createSpotDetectionPage();
#endif

    void saveUiState();
    void restoreUiState();

    void onLocalAnalysisClicked();
    void onGainAnalysisComplete(int gainIndex, const NoiseAnalysisResult &result);
    void onAllAnalysisComplete();
    void onAnalysisProgressUpdate(int gainIndex, int currentFrame, int totalFrames);
    void calculateAndDisplayResults();
    void cancelAnalysis();
    void onSaveResults();

    void onSaveNew();

    void saveSampleImages(const QString &baseDir, int analysisIndex);

    QWidget *contentWidget_{nullptr};
    QListWidget *navList_{nullptr};
    QStackedWidget *stackedPages_{nullptr};
    QHBoxLayout *mainLayout_{nullptr};

    QSplitter *mainSplitter_{nullptr};

    QLabel *placeholderLabel_{nullptr};
    QThread *workerThread_{nullptr};
    ImageAlgorithmWorker *worker_{nullptr};

    ThreadManager *threadManager_{nullptr};

    QThread *noiseAnalysisThread_{nullptr};
    NoiseAnalysisWorker *noiseAnalysisWorker_{nullptr};

    // Readout Noise Page UI Elements
    QSpinBox *spinSampleCount_{nullptr};
    QSpinBox *spinStartX_{nullptr};
    QSpinBox *spinStartY_{nullptr};
    QSpinBox *spinWidth_{nullptr};
    QSpinBox *spinHeight_{nullptr};

    QPushButton *btnLocalAnalysis_{nullptr};

    // 分析状态变量
    struct GainConfig {
        int gainType;
        int exposureTime;
        QString name;
    };
    QVector<GainConfig> gainConfigs_;
    int currentGainIndex_{-1};
    int currentSampleIndex_{0};
    int totalSamples_{0};
    QRect analysisRegion_;

    QVector<NoiseAnalysisResult> gainResults_;

    ImageAlgorithmManager *imageAlgorithmManager_{nullptr};
    ImageProcessingPanel *imageProcessingPanel_{nullptr};

    MixedProcessingDialog *mixedProcessingDialog_{nullptr};

#if ENABLE_SPOT_DETECTION
    QSpinBox *spinRoiSize_{nullptr};
    QDoubleSpinBox *spinThresholdRatio_{nullptr};
    QDoubleSpinBox *spinSaturationRatio_{nullptr};
    QCheckBox *chkBackgroundRemoval_{nullptr};
    QCheckBox *chkSquareWeights_{nullptr};
    QDoubleSpinBox *spinDt_{nullptr};
    QDoubleSpinBox *spinProcessNoise_{nullptr};
    QDoubleSpinBox *spinBaseR_{nullptr};
    QPushButton *btnStartSpotDetection_{nullptr};
    QPushButton *btnStopSpotDetection_{nullptr};
    QPushButton *btnResetKalman_{nullptr};
    QLabel *lblSpotResult_{nullptr};
    QLabel *lblSpotStats_{nullptr};

    bool spotDetectionRunning_{false};
    bool spotDetectionFirstFrame_{true};
#endif

    QTimer *commandDelayTimer_{nullptr};
    QTimer *captureTimeoutTimer_{nullptr};
    bool isAnalyzing_{false};
    bool isCapturing_{false};
    int analysisCount_{0};
    int bitDepth_{16};
    int bitShift_{6};
    ProcessingInputMode processingInputMode_{ProcessingInputMode::Standard8Bit};
    QString cameraSN_{"--"};
    QString lastSavePath_;
};

#endif // IMAGE_ALGORITHM_DOCK_H
