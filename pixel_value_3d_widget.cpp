#include "pixel_value_3d_widget.h"
#include <QPainter>
#include <cmath>
#include <algorithm>

PixelValue3DWidget::PixelValue3DWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    setFormat(format);
    
    setMouseTracking(true);
    
    // 初始化颜色
    backgroundColorR_ = 0.10f;
    backgroundColorG_ = 0.10f;
    backgroundColorB_ = 0.15f;
    backgroundColorA_ = 1.00f;
}

PixelValue3DWidget::~PixelValue3DWidget()
{
    makeCurrent();
    doneCurrent();
}

void PixelValue3DWidget::initializeGL()
{
    initializeOpenGLFunctions();
    
    glClearColor(backgroundColorR_, backgroundColorG_, backgroundColorB_, backgroundColorA_);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
}

void PixelValue3DWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void PixelValue3DWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (mode2D_) {
        // 2D mode: draw a simple 2D line chart
        draw2DPlot();
    } else {
        // 3D mode: draw the 3D surface
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        
        float aspect = static_cast<float>(width()) / static_cast<float>(height());
        float fov = 45.0f;
        float zNear = 0.1f;
        float zFar = 100.0f;
        float f = 1.0f / std::tan(fov * 3.14159265f / 360.0f);
        
        float mat[16] = {0};
        mat[0] = f / aspect;
        mat[5] = f;
        mat[10] = (zFar + zNear) / (zNear - zFar);
        mat[11] = -1.0f;
        mat[14] = (2.0f * zFar * zNear) / (zNear - zFar);
        glMultMatrixf(mat);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        glTranslatef(0.0f, 0.0f, -3.5f * zoom_);
        
        glRotatef(rotationX_, 1.0f, 0.0f, 0.0f);
        glRotatef(rotationY_, 0.0f, 1.0f, 0.0f);
        glRotatef(rotationZ_, 0.0f, 0.0f, 1.0f);
        
        drawAxes();
        drawGrid();
        drawSurface();
        
        drawOverlay();
    }
}

void PixelValue3DWidget::setData(const QVector<QVector<uint16_t>> &frames, int roiWidth, int roiHeight, int roiStartX, int roiStartY)
{
    frames_ = frames;
    roiWidth_ = roiWidth;
    roiHeight_ = roiHeight;
    roiStartX_ = roiStartX;
    roiStartY_ = roiStartY;
    frameCount_ = frames.size();
    pixelCount_ = roiWidth * roiHeight;
    
    generateMesh();
    update();
}

void PixelValue3DWidget::generateMesh()
{
    vertices_.clear();
    indices_.clear();
    
    if (frameCount_ <= 0 || pixelCount_ <= 0) return;
    
    // Find min/max for normalization
    bool inited = false;
    for (const auto &frame : frames_) {
        for (uint16_t val : frame) {
            if (!inited) {
                minValue_ = maxValue_ = val;
                inited = true;
            } else {
                if (val < minValue_) minValue_ = val;
                if (val > maxValue_) maxValue_ = val;
            }
        }
    }
    
    if (std::abs(maxValue_ - minValue_) < 1e-6) {
        maxValue_ = minValue_ + 1.0;
    }
    
    axisXMin_ = 0;
    axisXMax_ = frameCount_ - 1;
    axisYMin_ = 0;
    axisYMax_ = pixelCount_ - 1;
    axisZMin_ = minValue_;
    axisZMax_ = maxValue_;
    
    // Generate vertices
    // X: Frame Index (0 to frameCount-1) -> mapped to [-1, 1]
    // Y: Pixel Index (0 to pixelCount-1) -> mapped to [-1, 1]
    // Z: Value -> mapped to [-1, 1]
    
    float xDen = std::max(1, frameCount_ - 1);
    float yDen = std::max(1, pixelCount_ - 1);
    
    // We are creating a grid where rows are frames and cols are pixels
    // Or rows are pixels and cols are frames.
    // Let's say X is frame index (columns), Y is pixel index (rows).
    
    // To avoid too many vertices if pixelCount is huge, we might need to downsample.
    // But for now, let's assume it fits or user handles ROI size.
    // If pixelCount is 10000 (100x100), and frames 100 -> 1M vertices.
    // This is fine for simple rendering.
    
    for (int p = 0; p < pixelCount_; ++p) {
        for (int f = 0; f < frameCount_; ++f) {
            // Check if frame has enough data
            if (p >= frames_[f].size()) continue;
            
            uint16_t val = frames_[f][p];
            
            float nx = static_cast<float>(f) / xDen;
            float ny = static_cast<float>(p) / yDen;
            float nz = static_cast<float>((val - minValue_) / (maxValue_ - minValue_));
            
            VertexData v;
            v.x = -1.0f + nx * 2.0f;
            v.y = -1.0f + ny * 2.0f;
            v.z = -1.0f + nz * 2.0f;
            v.valueNorm = nz;
            v.color = getColorForValue(nz);
            
            vertices_.append(v);
        }
    }
    
    // Generate indices for grid mesh
    // Grid dimensions: width = frameCount_, height = pixelCount_
    // But we only added valid vertices. Assuming rectangular data.
    
    for (int p = 0; p < pixelCount_ - 1; ++p) {
        for (int f = 0; f < frameCount_ - 1; ++f) {
            int topLeft = p * frameCount_ + f;
            int topRight = topLeft + 1;
            int bottomLeft = (p + 1) * frameCount_ + f;
            int bottomRight = bottomLeft + 1;
            
            // Triangle 1
            indices_.append(topLeft);
            indices_.append(bottomLeft);
            indices_.append(topRight);
            
            // Triangle 2
            indices_.append(topRight);
            indices_.append(bottomLeft);
            indices_.append(bottomRight);
        }
    }
}

QVector3D PixelValue3DWidget::getColorForValue(float valueNorm)
{
    // Simple Heatmap: Blue -> Green -> Red
    float value = std::max(0.0f, std::min(1.0f, valueNorm));
    
    if (value < 0.5f) {
        float t = value * 2.0f;
        return QVector3D(0.0f, t, 1.0f - t); // Blue to Green
    } else {
        float t = (value - 0.5f) * 2.0f;
        return QVector3D(t, 1.0f - t, 0.0f); // Green to Red
    }
}

void PixelValue3DWidget::drawAxes()
{
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    // X - Red
    glColor3f(1, 0, 0); glVertex3f(-1, -1, -1); glVertex3f(1, -1, -1);
    // Y - Green
    glColor3f(0, 1, 0); glVertex3f(-1, -1, -1); glVertex3f(-1, 1, -1);
    // Z - Blue
    glColor3f(0, 0, 1); glVertex3f(-1, -1, -1); glVertex3f(-1, -1, 1);
    glEnd();
    glLineWidth(1.0f);
}

void PixelValue3DWidget::drawGrid()
{
    glColor4f(0.5f, 0.5f, 0.5f, 0.6f);
    glBegin(GL_LINES);
    float z = -1.0f;
    for (float i = -1.0f; i <= 1.0f + 1e-5; i += 0.2f) {
        glVertex3f(-1, i, z); glVertex3f(1, i, z);
        glVertex3f(i, -1, z); glVertex3f(i, 1, z);
    }
    glEnd();
}

void PixelValue3DWidget::drawSurface()
{
    if (vertices_.isEmpty()) return;

#if PIXEL_VALUE_3D_RENDER_STYLE == PIXEL_VALUE_3D_RENDER_STYLE_SCATTER
    drawPointCloud();
    return;
#elif PIXEL_VALUE_3D_RENDER_STYLE == PIXEL_VALUE_3D_RENDER_STYLE_BARS
    drawBars();
    return;
#else
    // 默认：曲面（原逻辑保持不变）

    if (indices_.isEmpty()) return;

    // Draw filled triangles with colors
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < indices_.size(); ++i) {
        int idx = indices_[i];
        if (idx < vertices_.size()) {
            const VertexData &v = vertices_[idx];
            glColor3f(v.color.x(), v.color.y(), v.color.z());
            glVertex3f(v.x, v.y, v.z);
        }
    }
    glEnd();

    // Draw wireframe lines to make mesh structure more visible
    glColor4f(0.2f, 0.2f, 0.2f, 0.4f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < indices_.size(); i += 3) {
        for (int j = 0; j < 3; ++j) {
            int idx1 = indices_[i + j];
            int idx2 = indices_[i + (j + 1) % 3];
            if (idx1 < vertices_.size() && idx2 < vertices_.size()) {
                const VertexData &v1 = vertices_[idx1];
                const VertexData &v2 = vertices_[idx2];
                glVertex3f(v1.x, v1.y, v1.z);
                glVertex3f(v2.x, v2.y, v2.z);
            }
        }
    }
    glEnd();
#endif
}

void PixelValue3DWidget::drawPointCloud()
{
    if (vertices_.isEmpty()) return;

    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < vertices_.size(); ++i) {
        const VertexData &v = vertices_[i];
        glColor3f(v.color.x(), v.color.y(), v.color.z());
        glVertex3f(v.x, v.y, v.z);
    }
    glEnd();
}

void PixelValue3DWidget::drawBars()
{
    if (vertices_.isEmpty()) return;

    // 柱状图：从基准面 z=-1 向上画柱子到每个点的 z
    const float baseZ = -1.0f;

    const float dx = (frameCount_ > 1) ? (2.0f / float(frameCount_ - 1)) : 2.0f;
    const float dy = (pixelCount_ > 1) ? (2.0f / float(pixelCount_ - 1)) : 2.0f;
    const float hx = std::max(0.005f, dx * 0.35f);
    const float hy = std::max(0.005f, dy * 0.35f);

    // 防止数据量过大导致卡死：做一个简单抽样
    constexpr int kMaxBars = 40000;
    const int total = static_cast<int>(vertices_.size());
    const int step = std::max(1, (total + kMaxBars - 1) / kMaxBars);

    glBegin(GL_QUADS);
    for (int i = 0; i < vertices_.size(); i += step) {
        const VertexData &v = vertices_[i];
        const float topZ = std::max(baseZ, v.z);

        const float x0 = v.x - hx;
        const float x1 = v.x + hx;
        const float y0 = v.y - hy;
        const float y1 = v.y + hy;

        // 颜色：沿用热力图颜色，侧面略微变暗
        const float r = v.color.x();
        const float g = v.color.y();
        const float b = v.color.z();

        // Front (y1)
        glColor4f(r * 0.85f, g * 0.85f, b * 0.85f, 0.95f);
        glVertex3f(x0, y1, baseZ);
        glVertex3f(x1, y1, baseZ);
        glVertex3f(x1, y1, topZ);
        glVertex3f(x0, y1, topZ);

        // Back (y0)
        glColor4f(r * 0.70f, g * 0.70f, b * 0.70f, 0.95f);
        glVertex3f(x1, y0, baseZ);
        glVertex3f(x0, y0, baseZ);
        glVertex3f(x0, y0, topZ);
        glVertex3f(x1, y0, topZ);

        // Left (x0)
        glColor4f(r * 0.75f, g * 0.75f, b * 0.75f, 0.95f);
        glVertex3f(x0, y0, baseZ);
        glVertex3f(x0, y1, baseZ);
        glVertex3f(x0, y1, topZ);
        glVertex3f(x0, y0, topZ);

        // Right (x1)
        glColor4f(r * 0.78f, g * 0.78f, b * 0.78f, 0.95f);
        glVertex3f(x1, y1, baseZ);
        glVertex3f(x1, y0, baseZ);
        glVertex3f(x1, y0, topZ);
        glVertex3f(x1, y1, topZ);

        // Top
        glColor4f(r, g, b, 0.98f);
        glVertex3f(x0, y0, topZ);
        glVertex3f(x1, y0, topZ);
        glVertex3f(x1, y1, topZ);
        glVertex3f(x0, y1, topZ);
    }
    glEnd();

    // 轮廓线：稍微提高清晰度
    glColor4f(0.15f, 0.15f, 0.15f, 0.35f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < vertices_.size(); i += step) {
        const VertexData &v = vertices_[i];
        const float topZ = std::max(baseZ, v.z);

        const float x0 = v.x - hx;
        const float x1 = v.x + hx;
        const float y0 = v.y - hy;
        const float y1 = v.y + hy;

        // 四条竖边
        glVertex3f(x0, y0, baseZ); glVertex3f(x0, y0, topZ);
        glVertex3f(x1, y0, baseZ); glVertex3f(x1, y0, topZ);
        glVertex3f(x1, y1, baseZ); glVertex3f(x1, y1, topZ);
        glVertex3f(x0, y1, baseZ); glVertex3f(x0, y1, topZ);
    }
    glEnd();
}

QPointF PixelValue3DWidget::projectToScreen(float x, float y, float z, bool *ok)
{
    GLdouble model[16];
    GLdouble proj[16];
    GLint viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, viewport);

    auto mulMatVec = [](const GLdouble m[16], const GLdouble v[4], GLdouble out[4]) {
        for (int r = 0; r < 4; ++r) {
            out[r] = m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2] + m[12 + r] * v[3];
        }
    };

    GLdouble v[4] = {x, y, z, 1.0};
    GLdouble mv[4];
    GLdouble clip[4];
    mulMatVec(model, v, mv);
    mulMatVec(proj, mv, clip);

    if (std::abs(clip[3]) < 1e-9) {
        if (ok) *ok = false;
        return {};
    }

    GLdouble ndcX = clip[0] / clip[3];
    GLdouble ndcY = clip[1] / clip[3];

    double winX = viewport[0] + (ndcX + 1.0) * 0.5 * viewport[2];
    double winY = viewport[1] + (1.0 - (ndcY + 1.0) * 0.5) * viewport[3];

    if (ok) *ok = true;
    return QPointF(winX, winY);
}

void PixelValue3DWidget::drawOverlay()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(230, 230, 230));

    // Axis labels
    {
        bool ok1 = false;
        QPointF pX = projectToScreen(1.0f, -1.0f, -1.0f, &ok1);
        if (ok1) painter.drawText(pX + QPointF(6, -4), tr("帧数"));

        bool ok2 = false;
        QPointF pY = projectToScreen(-1.0f, 1.0f, -1.0f, &ok2);
        if (ok2) painter.drawText(pY + QPointF(6, -4), tr("像素索引"));

        bool ok3 = false;
        QPointF pZ = projectToScreen(-1.0f, -1.0f, 1.0f, &ok3);
        if (ok3) painter.drawText(pZ + QPointF(6, -4), tr("像素值"));
    }

    // Axis ticks
    auto drawAxisTicks = [&](char axis, float vmin, float vmax, const QString &suffix) {
        if (vmax <= vmin) return;
        int approxTicks = 5;
        float span = vmax - vmin;
        float step = std::max(1.0f, span / approxTicks);

        for (float v = vmin; v <= vmax; v += step) {
            float t = (v - vmin) / span;
            float px = -1.0f;
            float py = -1.0f;
            float pz = -1.0f;
            float tx = 0, ty = 0, tz = 0;
            if (axis == 'x') {
                px = -1.0f + t * 2.0f;
                py = -1.0f;
                pz = -1.0f;
                tx = 0;
                ty = 0.03f;
                tz = 0;
            } else if (axis == 'y') {
                px = -1.0f;
                py = -1.0f + t * 2.0f;
                pz = -1.0f;
                tx = 0.03f;
                ty = 0;
                tz = 0;
            } else {
                px = -1.0f;
                py = -1.0f;
                pz = -1.0f + t * 2.0f;
                tx = 0.03f;
                ty = 0;
                tz = 0;
            }

            glColor4f(0.8f, 0.8f, 0.8f, 0.7f);
            glBegin(GL_LINES);
            glVertex3f(px, py, pz);
            glVertex3f(px + tx, py + ty, pz + tz);
            glEnd();

            bool ok = false;
            QPointF sp = projectToScreen(px + tx, py + ty, pz + tz, &ok);
            if (ok) {
                painter.drawText(sp + QPointF(2, 10), QString::number(v, 'f', 0) + suffix);
            }
        }
    };

    drawAxisTicks('x', axisXMin_, axisXMax_, QString());
    drawAxisTicks('y', axisYMin_, axisYMax_, QString());
    drawAxisTicks('z', axisZMin_, axisZMax_, QString());
}

void PixelValue3DWidget::mousePressEvent(QMouseEvent *event)
{
    lastMousePos_ = event->pos();
}

void PixelValue3DWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (mode2D_) {
        // 2D mode hover picking
        const float marginLeft = 60.0f;
        const float marginRight = 20.0f;
        const float marginTop = 20.0f;
        const float marginBottom = 50.0f;
        const float plotWidth = width() - marginLeft - marginRight;
        const float plotHeight = height() - marginTop - marginBottom;
        
        const float x = event->pos().x();
        const float y = event->pos().y();

        const bool inPlot = (frameCount_ > 0)
            && (plotWidth > 1.0f)
            && (plotHeight > 1.0f)
            && (x >= marginLeft && x <= marginLeft + plotWidth)
            && (y >= marginTop && y <= marginTop + plotHeight);

        if (inPlot) {
            const float ratio = (x - marginLeft) / plotWidth;
            int frameIndex = static_cast<int>(std::round(ratio * (frameCount_ - 1)));
            frameIndex = std::max(0, std::min(frameCount_ - 1, frameIndex));

            const int pixelIndex = pixel2DY_ * roiWidth_ + pixel2DX_;
            if (pixelIndex >= 0
                && pixelIndex < pixelCount_
                && frameIndex >= 0
                && frameIndex < frames_.size()
                && pixelIndex < frames_[frameIndex].size()) {

                const uint16_t value = frames_[frameIndex][pixelIndex];

                hoverActive_ = true;
                hoverFrameIndex_ = frameIndex;
                hoverValue_ = value;
                hoverMousePos_ = event->pos();
                update();
            } else {
                if (hoverActive_) {
                    hoverActive_ = false;
                    hoverFrameIndex_ = -1;
                    update();
                }
            }
        } else {
            if (hoverActive_) {
                hoverActive_ = false;
                hoverFrameIndex_ = -1;
                update();
            }
        }
        return;
    }

    if (event->buttons() & Qt::LeftButton) {
        int dx = event->pos().x() - lastMousePos_.x();
        int dy = event->pos().y() - lastMousePos_.y();
        
        rotationX_ += dy * 0.5f;
        rotationZ_ += dx * 0.5f;
        
        update();
    } else if (event->buttons() & Qt::RightButton) {
        int dx = event->pos().x() - lastMousePos_.x();
        
        rotationY_ += dx * 0.5f;
        
        update();
    }
    
    lastMousePos_ = event->pos();
}

void PixelValue3DWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (mode2D_ && hoverActive_) {
        hoverActive_ = false;
        hoverFrameIndex_ = -1;
        update();
    }
}

void PixelValue3DWidget::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() / 120.0f;
    zoom_ *= (1.0f - delta * 0.1f);
    zoom_ = std::max(0.1f, std::min(5.0f, zoom_));
    update();
}

void PixelValue3DWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Do nothing to prevent opening new window
    Q_UNUSED(event);
}

void PixelValue3DWidget::resetView()
{
    rotationX_ = -30.0f;
    rotationY_ = 0.0f;
    rotationZ_ = 0.0f;
    zoom_ = 1.0f;
    update();
}

QImage PixelValue3DWidget::grabFramebuffer()
{
    return QOpenGLWidget::grabFramebuffer();
}

void PixelValue3DWidget::set2DMode(bool enabled, int pixelX, int pixelY)
{
    mode2D_ = enabled;
    pixel2DX_ = pixelX;
    pixel2DY_ = pixelY;
    update();
}
void PixelValue3DWidget::setYAxisRange(bool autoRange, double min, double max)
{
    autoYRange_ = autoRange;
    manualYMin_ = min;
    manualYMax_ = max;
    update();
}
void PixelValue3DWidget::draw2DPlot()
{
    if (frames_.isEmpty() || frameCount_ <= 0) {
        return;
    }
    
    // Set up 2D orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width(), height(), 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Calculate margins
    const float marginLeft = 60.0f;
    const float marginRight = 20.0f;
    const float marginTop = 20.0f;
    const float marginBottom = 50.0f;
    
    const float plotWidth = width() - marginLeft - marginRight;
    const float plotHeight = height() - marginTop - marginBottom;
    
    // Calculate the pixel index from 2D coordinates
    int pixelIndex = pixel2DY_ * roiWidth_ + pixel2DX_;
    
    // Check if the pixel is valid
    if (pixel2DX_ < 0 || pixel2DX_ >= roiWidth_ || pixel2DY_ < 0 || pixel2DY_ >= roiHeight_) {
        // Draw error message
        QPainter painter(this);
        painter.setPen(QColor(255, 0, 0));
        painter.drawText(rect(), Qt::AlignCenter, tr("无效的像素坐标: (%1, %2)").arg(pixel2DX_).arg(pixel2DY_));
        return;
    }
    
    // Extract pixel values for the selected coordinate
    QVector<uint16_t> pixelValues;
    pixelValues.reserve(frameCount_);
    
    double minVal = 0.0;
    double maxVal = 0.0;
    bool firstVal = true;
    double sumValues = 0.0;
    
    for (int f = 0; f < frameCount_; ++f) {
        if (frames_[f].size() > pixelIndex) {
            uint16_t val = frames_[f][pixelIndex];
            pixelValues.append(val);
            sumValues += val;
            
            if (firstVal) {
                minVal = maxVal = val;
                firstVal = false;
            } else {
                minVal = std::min(minVal, (double)val);
                maxVal = std::max(maxVal, (double)val);
            }
        } else {
            pixelValues.append(0);
        }
    }

    double averageValue = pixelValues.isEmpty() ? 0.0 : sumValues / pixelValues.size();
    
    // Add some padding to the value range
    if (autoYRange_) {
        double valueRange = maxVal - minVal;
        if (valueRange < 1.0) {
            valueRange = 1.0;
        }
        minVal -= valueRange * 0.1;
        maxVal += valueRange * 0.1;
    } else {
        minVal = manualYMin_;
        maxVal = manualYMax_;
    }
    
    // Draw background
    glColor3f(0.1f, 0.1f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(width(), 0);
    glVertex2f(width(), height());
    glVertex2f(0, height());
    glEnd();
    
    // Draw plot area background
    glColor3f(0.15f, 0.15f, 0.20f);
    glBegin(GL_QUADS);
    glVertex2f(marginLeft, marginTop);
    glVertex2f(marginLeft + plotWidth, marginTop);
    glVertex2f(marginLeft + plotWidth, marginTop + plotHeight);
    glVertex2f(marginLeft, marginTop + plotHeight);
    glEnd();
    
    // Draw grid lines
    glColor4f(0.5f, 0.5f, 0.5f, 0.6f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    
    // Horizontal grid lines
    for (int i = 0; i <= 5; ++i) {
        float y = marginTop + plotHeight * i / 5.0f;
        glVertex2f(marginLeft, y);
        glVertex2f(marginLeft + plotWidth, y);
    }
    
    // Vertical grid lines
    for (int i = 0; i <= 10; ++i) {
        float x = marginLeft + plotWidth * i / 10.0f;
        glVertex2f(x, marginTop);
        glVertex2f(x, marginTop + plotHeight);
    }
    
    glEnd();
    
    // Draw axes
    glColor3f(0.8f, 0.8f, 0.8f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    // Y axis
    glVertex2f(marginLeft, marginTop);
    glVertex2f(marginLeft, marginTop + plotHeight);
    // X axis
    glVertex2f(marginLeft, marginTop + plotHeight);
    glVertex2f(marginLeft + plotWidth, marginTop + plotHeight);
    glEnd();
    
    // Draw line chart
    if (pixelValues.size() > 1) {
        glColor3f(0.2f, 0.8f, 0.2f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        
        for (int f = 0; f < pixelValues.size(); ++f) {
            float x = marginLeft + plotWidth * f / (frameCount_ - 1);
            float normalizedValue = (pixelValues[f] - minVal) / (maxVal - minVal);
            float y = marginTop + plotHeight * (1.0f - normalizedValue);
            glVertex2f(x, y);
        }
        
        glEnd();
        
        // Draw points on the line
        glColor3f(1.0f, 1.0f, 0.2f);
        glPointSize(5.0f);
        glBegin(GL_POINTS);
        
        for (int f = 0; f < pixelValues.size(); ++f) {
            float x = marginLeft + plotWidth * f / (frameCount_ - 1);
            float normalizedValue = (pixelValues[f] - minVal) / (maxVal - minVal);
            float y = marginTop + plotHeight * (1.0f - normalizedValue);
            glVertex2f(x, y);
        }
        
        glEnd();
    }

    // Draw average line (Red)
    if (!pixelValues.isEmpty()) {
        float normalizedAvg = (averageValue - minVal) / (maxVal - minVal);
        // Only draw if within visible range
        if (normalizedAvg >= 0.0f && normalizedAvg <= 1.0f) {
            float y = marginTop + plotHeight * (1.0f - normalizedAvg);
            
            glColor3f(1.0f, 0.0f, 0.0f); // Red
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            glVertex2f(marginLeft, y);
            glVertex2f(marginLeft + plotWidth, y);
            glEnd();
        }
    }
    
    // Draw labels and title with QPainter
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(230, 230, 230));
    
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    
    // Title
    QString title = tr("像素值随帧变化 - ROI坐标: (%1, %2)")
                    .arg(roiStartX_ + pixel2DX_)
                    .arg(roiStartY_ + pixel2DY_);
    painter.drawText(QRectF(0, 0, width(), marginTop), Qt::AlignCenter, title);
    
    // X-axis label
    painter.drawText(QRectF(marginLeft, height() - marginBottom + 20, plotWidth, 30), Qt::AlignCenter, tr("帧索引"));
    
    // Y-axis label
    painter.save();
    painter.translate(10, marginTop + plotHeight / 2);
    painter.rotate(-90);
    painter.drawText(QRectF(-plotHeight / 2, 0, plotHeight, 20), Qt::AlignCenter, tr("像素值"));
    painter.restore();
    
    // X-axis tick labels
    font.setPointSize(8);
    painter.setFont(font);
    for (int i = 0; i <= 10; ++i) {
        int frameIndex = std::round((double)i * (frameCount_ - 1) / 10.0);
        float x = marginLeft + plotWidth * i / 10.0f;
        painter.drawText(QRectF(x - 20, height() - marginBottom + 5, 40, 15), Qt::AlignCenter, QString::number(frameIndex));
    }
    
    // Y-axis tick labels
    for (int i = 0; i <= 5; ++i) {
        double value = maxVal - (maxVal - minVal) * i / 5.0;
        float y = marginTop + plotHeight * i / 5.0f;
        painter.drawText(QRectF(5, y - 10, marginLeft - 10, 20), Qt::AlignRight | Qt::AlignVCenter, QString::number(value, 'f', 0));
    }

    // Draw average value label
    if (!pixelValues.isEmpty()) {
        float normalizedAvg = (averageValue - minVal) / (maxVal - minVal);
        if (normalizedAvg >= 0.0f && normalizedAvg <= 1.0f) {
            float y = marginTop + plotHeight * (1.0f - normalizedAvg);
            
            painter.setPen(QColor(255, 50, 50));
            painter.drawText(QRectF(marginLeft + plotWidth - 100, y - 20, 100, 20), 
                             Qt::AlignRight | Qt::AlignBottom, 
                             QString("Avg: %1").arg(averageValue, 0, 'f', 2));
        }
    }

    // Hover picking overlay (crosshair + highlighted point)
    if (hoverActive_ && frameCount_ > 1 && hoverFrameIndex_ >= 0 && hoverFrameIndex_ < frameCount_) {
        const float hoverX = marginLeft + plotWidth * hoverFrameIndex_ / (frameCount_ - 1);
        const float normalizedValue = static_cast<float>((static_cast<double>(hoverValue_) - minVal) / (maxVal - minVal));
        const float hoverY = marginTop + plotHeight * (1.0f - normalizedValue);

        // Clip to plot area
        painter.save();
        painter.setClipRect(QRectF(marginLeft, marginTop, plotWidth, plotHeight));

        QPen crossPen(QColor(38, 192, 166, 170));
        crossPen.setWidthF(2.0);
        painter.setPen(crossPen);
        painter.drawLine(QPointF(hoverX, marginTop), QPointF(hoverX, marginTop + plotHeight));
        painter.drawLine(QPointF(marginLeft, hoverY), QPointF(marginLeft + plotWidth, hoverY));

        painter.setPen(QPen(QColor(255, 220, 60), 2));
        painter.setBrush(QColor(255, 220, 60));
        painter.drawEllipse(QPointF(hoverX, hoverY), 4.0, 4.0);

        painter.restore();

        // Small label near cursor (not clipped)
        const QString hoverLabel = tr("帧 %1  ROI(%2,%3)  值 %4")
                                      .arg(hoverFrameIndex_)
                                      .arg(roiStartX_ + pixel2DX_)
                                      .arg(roiStartY_ + pixel2DY_)
                                      .arg(static_cast<int>(hoverValue_));
        QFont labelFont = painter.font();
        labelFont.setPointSize(9);
        painter.setFont(labelFont);
        const QFontMetrics fm(labelFont);
        const QSize labelSize(fm.horizontalAdvance(hoverLabel) + 10, fm.height() + 8);

        QPoint labelPos = hoverMousePos_ + QPoint(12, -labelSize.height() - 6);
        if (labelPos.x() + labelSize.width() > width()) labelPos.setX(width() - labelSize.width() - 2);
        if (labelPos.y() < 0) labelPos.setY(2);

        QRect labelRect(labelPos, labelSize);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 160));
        painter.drawRoundedRect(labelRect, 4, 4);
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(labelRect.adjusted(5, 4, -5, -4), Qt::AlignLeft | Qt::AlignVCenter, hoverLabel);
    }
}
