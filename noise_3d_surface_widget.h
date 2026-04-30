#ifndef NOISE_3D_SURFACE_WIDGET_H
#define NOISE_3D_SURFACE_WIDGET_H

#include "build_config.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVector>
#include <QMatrix4x4>
#include <QVector3D>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QCheckBox>
#include <QSpinBox>
#include "pixel_value_3d_widget.h"



// 3D表面网格点数据
struct SurfacePoint {
    float x, y, z;
    float valueNorm; // [0,1]：用于配色重算，避免使用已缩放的z
    float sizeNorm;  // [0,1]：用于体积/尺寸编码
    QVector3D color;
};

// 3D噪声表面可视化小部件
class Noise3DSurfaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit Noise3DSurfaceWidget(QWidget *parent = nullptr);
    ~Noise3DSurfaceWidget() override;

    enum ViewMode {
        BothViews,
        SurfaceOnly,
        PixelOnly
    };

    void setViewMode(ViewMode mode);

    // 设置数据 (stdDevs是标准差数组，width和height是数据的维度)
    void setData(const QVector<double> &stdDevs, int width, int height);

    // 设置像素栈数据：X=张数(帧序号)，Y/Z=ROI坐标；点的颜色代表像素值
    void setStackData(const QVector<QVector<uint16_t>> &frames, int roiWidth, int roiHeight,
                      int roiStartX = 0, int roiStartY = 0);
    
    // 设置颜色方案
    enum ColorScheme {
        Rainbow,      // 彩虹色
        HeatMap,      // 热力图
        GrayScale,    // 灰度
        BlueRed,      // 蓝红渐变
        Viridis       // Viridis配色
    };
    
    void setColorScheme(ColorScheme scheme);
    
    // 重置视角
    void resetView();
    
signals:
    void surfaceViewDoubleClicked();
    void pixelViewDoubleClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    class GLWidget;
    GLWidget *glWidget_;
    PixelValue3DWidget *pixelValueWidget_;

    QSplitter *splitter_;
    
    // 控制面板
    QPushButton *btnResetView_;
    QComboBox *comboColorScheme_;
    QLabel *lblInfo_;
    QCheckBox *chk2DMode_;
    QSpinBox *spin2DPixelX_;
    QSpinBox *spin2DPixelY_;
    
    // 2D模式Y轴控制
    QCheckBox *chkAutoYRange_;
    QSpinBox *spinYMin_;
    QSpinBox *spinYMax_;
    
    void setupUI();
    void updateInfoLabel();
    void update2DMode();
    
    int dataWidth_{0};
    int dataHeight_{0};
    double minValue_{0.0};
    double maxValue_{0.0};
    double avgValue_{0.0};

    int roiStartX_{0};
    int roiStartY_{0};
    int frameCount_{0};

    float backgroundColorR_{0.1f};
    float backgroundColorG_{0.1f};
    float backgroundColorB_{0.15f};
    float backgroundColorA_{1.0f};
};

// OpenGL渲染小部件
class Noise3DSurfaceWidget::GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GLWidget(Noise3DSurfaceWidget *parent);
    ~GLWidget() override;

    void setData(const QVector<double> &stdDevs, int width, int height);
    void setStackData(const QVector<QVector<uint16_t>> &frames, int roiWidth, int roiHeight, int roiStartX, int roiStartY);
    void setColorScheme(Noise3DSurfaceWidget::ColorScheme scheme);
    void resetView();
    
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    Noise3DSurfaceWidget *parentWidget_;
    
    // 视角控制
    float rotationX_{-30.0f};
    float rotationY_{0.0f};
    float rotationZ_{0.0f};
    float zoom_{1.0f};
    QPoint lastMousePos_;
    
    // 数据
    QVector<SurfacePoint> vertices_;
    QVector<unsigned int> indices_;
    int dataWidth_{0};
    int dataHeight_{0};
    double minZ_{0.0};
    double maxZ_{1.0};

    enum class RenderMode {
        SurfaceStdDev,
        PointCloudStack
    };
    RenderMode renderMode_{RenderMode::SurfaceStdDev};
    int frameCount_{0};

    int roiStartX_{0};
    int roiStartY_{0};

    // 轴范围（用于刻度/标签）
    int axisXMin_{0}, axisXMax_{0};
    int axisYMin_{0}, axisYMax_{0};
    int axisZMin_{0}, axisZMax_{0};
    int pointStride_{1};
    
    Noise3DSurfaceWidget::ColorScheme colorScheme_{Noise3DSurfaceWidget::Rainbow};
    
    // 主题颜色
    float backgroundColorR_{0.1f};
    float backgroundColorG_{0.1f};
    float backgroundColorB_{0.15f};
    float backgroundColorA_{1.0f};
    
    // 辅助函数
    void generateSurfaceMesh(const QVector<double> &stdDevs, int width, int height);
    void generatePointCloud(const QVector<QVector<uint16_t>> &frames, int roiWidth, int roiHeight);
    QVector3D getColorForValue(double value) const;
    void drawGrid();
    void drawAxes();
    void drawSurface();
    void drawPointCloud();
    void drawVoxelSpheres();
    void drawOverlay();
    void initializeSphereVBO();
    QPointF projectToScreen(float x, float y, float z, bool *ok = nullptr);

    // stack点云统计
    double meanValue_{0.0};
    double minValueRaw_{0.0};
    double maxValueRaw_{1.0};
    float voxelBaseHalf_{0.015f};

    // GPU加速：球面网格VBO
    GLuint sphereVBO_{0};
    QVector<QVector3D> sphereVertices_;
    QVector<QVector3D> sphereNormals_;
    int sphereVertexCount_{0};
    bool sphereVBOInitialized_{false};
};

#endif // NOISE_3D_SURFACE_WIDGET_H
