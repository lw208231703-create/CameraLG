#ifndef PIXEL_VALUE_3D_WIDGET_H
#define PIXEL_VALUE_3D_WIDGET_H

#include "build_config.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVector>
#include <QMatrix4x4>
#include <QVector3D>
#include <QMouseEvent>
#include <QWheelEvent>



// Pixel Value 3D Graph Widget
// X-axis: Frame Index (Time)
// Y-axis: Pixel 1D Index (Flattened ROI)
// Z-axis: Pixel Value
class PixelValue3DWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit PixelValue3DWidget(QWidget *parent = nullptr);
    ~PixelValue3DWidget() override;

    // Set data: frames contains the pixel values for each frame
    // Each frame is a vector of pixel values (flattened ROI)
    void setData(const QVector<QVector<uint16_t>> &frames, int roiWidth, int roiHeight, int roiStartX, int roiStartY);

    // Set 2D mode with specific pixel coordinate
    void set2DMode(bool enabled, int pixelX = 0, int pixelY = 0);
    
    // Set Y-axis range for 2D plot
    void setYAxisRange(bool autoRange, double min, double max);
    
    void resetView();
    QImage grabFramebuffer();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    struct VertexData {
        float x, y, z;
        float valueNorm;
        QVector3D color;
    };

    void generateMesh();
    void drawAxes();
    void drawGrid();
    void drawSurface();
    void drawPointCloud();
    void drawBars();
    void drawOverlay();
    void draw2DPlot();
    QPointF projectToScreen(float x, float y, float z, bool *ok);
    QVector3D getColorForValue(float valueNorm);

    // Data
    QVector<QVector<uint16_t>> frames_;
    int roiWidth_{0};
    int roiHeight_{0};
    int roiStartX_{0};
    int roiStartY_{0};
    int frameCount_{0};
    int pixelCount_{0};

    double minValue_{0.0};
    double maxValue_{0.0};

    // Rendering
    QVector<VertexData> vertices_;
    QVector<unsigned int> indices_;

    // Camera / View
    float rotationX_{-30.0f};
    float rotationY_{0.0f};
    float rotationZ_{0.0f}; // Usually 0 for this type of graph
    float zoom_{1.0f};
    QPoint lastMousePos_;

    // Axis ranges for display
    float axisXMin_{0};
    float axisXMax_{0};
    float axisYMin_{0};
    float axisYMax_{0};
    double axisZMin_{0};
    double axisZMax_{0};
    
    // 2D Plot Y-axis settings
    bool autoYRange_{true};
    double manualYMin_{0.0};
    double manualYMax_{65535.0};
    
    // 2D mode
    bool mode2D_{false};
    int pixel2DX_{0};
    int pixel2DY_{0};

    // 2D hover picking
    bool hoverActive_{false};
    int hoverFrameIndex_{-1};
    uint16_t hoverValue_{0};
    QPoint hoverMousePos_;
    
    // 主题颜色
    float backgroundColorR_{0.1f};
    float backgroundColorG_{0.1f};
    float backgroundColorB_{0.15f};
    float backgroundColorA_{1.0f};
};

#endif // PIXEL_VALUE_3D_WIDGET_H
