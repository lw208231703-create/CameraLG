#include "noise_3d_surface_widget.h"
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>
#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>
#include <cmath>
#include <QEvent>
#include <QCheckBox>
#include <QSpinBox>

// ============================================================================
// Noise3DSurfaceWidget Implementation
// ============================================================================

Noise3DSurfaceWidget::Noise3DSurfaceWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    
    // 初始化颜色
    backgroundColorR_ = 0.10f;
    backgroundColorG_ = 0.10f;
    backgroundColorB_ = 0.15f;
    backgroundColorA_ = 1.00f;
}

Noise3DSurfaceWidget::~Noise3DSurfaceWidget()
{
}

void Noise3DSurfaceWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 创建分割器
    splitter_ = new QSplitter(Qt::Horizontal, this);
    
    // 创建OpenGL渲染小部件 (左侧)
    glWidget_ = new GLWidget(this);
    glWidget_->installEventFilter(this);
    // 移除固定的最小尺寸，让它自适应布局
    glWidget_->setMinimumSize(300, 250);
    splitter_->addWidget(glWidget_);
    
    // 创建新的像素值3D小部件 (右侧)
    pixelValueWidget_ = new PixelValue3DWidget(this);
    pixelValueWidget_->installEventFilter(this);
    pixelValueWidget_->setMinimumSize(300, 250);
    splitter_->addWidget(pixelValueWidget_);
    
#if FORCE_2D_MODE
    glWidget_->setVisible(false);
#endif

    // 设置分割器比例
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter_, 1);
    
    // 控制面板
    QHBoxLayout *controlLayout = new QHBoxLayout();
    
    // 信息标签
    lblInfo_ = new QLabel(tr("数据: 0x0, 范围: [0.0, 0.0]"), this);
    controlLayout->addWidget(lblInfo_);
    
    controlLayout->addStretch();
    
#if !FORCE_2D_MODE
    // 颜色方案选择
    QLabel *lblColorScheme = new QLabel(tr("配色方案:"), this);
    controlLayout->addWidget(lblColorScheme);
    
    comboColorScheme_ = new QComboBox(this);
    comboColorScheme_->addItem(tr("彩虹色"), static_cast<int>(Rainbow));
    comboColorScheme_->addItem(tr("热力图"), static_cast<int>(HeatMap));
    comboColorScheme_->addItem(tr("灰度"), static_cast<int>(GrayScale));
    comboColorScheme_->addItem(tr("蓝红渐变"), static_cast<int>(BlueRed));
    comboColorScheme_->addItem(tr("Viridis"), static_cast<int>(Viridis));
    comboColorScheme_->setCurrentIndex(0);
    controlLayout->addWidget(comboColorScheme_);
    
    connect(comboColorScheme_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        ColorScheme scheme = static_cast<ColorScheme>(comboColorScheme_->currentData().toInt());
        setColorScheme(scheme);
    });
    
    // 重置视角按钮
    btnResetView_ = new QPushButton(tr("重置视角"), this);
    connect(btnResetView_, &QPushButton::clicked, this, &Noise3DSurfaceWidget::resetView);
    controlLayout->addWidget(btnResetView_);
#else
    // 避免编译错误，初始化未使用的指针
    comboColorScheme_ = nullptr;
    btnResetView_ = nullptr;
#endif
    
    mainLayout->addLayout(controlLayout);
    
    // 2D模式控制面板
    QHBoxLayout *mode2DLayout = new QHBoxLayout();
    
    chk2DMode_ = new QCheckBox(tr("启用2D绘图"), this);
    mode2DLayout->addWidget(chk2DMode_);
    
    QLabel *lblPixelX = new QLabel(tr("像素X:"), this);
    mode2DLayout->addWidget(lblPixelX);
    
    spin2DPixelX_ = new QSpinBox(this);
    spin2DPixelX_->setMinimum(0);
    spin2DPixelX_->setMaximum(9999);
    spin2DPixelX_->setValue(0);
    spin2DPixelX_->setEnabled(false);
    mode2DLayout->addWidget(spin2DPixelX_);
    
    QLabel *lblPixelY = new QLabel(tr("像素Y:"), this);
    mode2DLayout->addWidget(lblPixelY);
    
    spin2DPixelY_ = new QSpinBox(this);
    spin2DPixelY_->setMinimum(0);
    spin2DPixelY_->setMaximum(9999);
    spin2DPixelY_->setValue(0);
    spin2DPixelY_->setEnabled(false);
    mode2DLayout->addWidget(spin2DPixelY_);
    
    // Y轴范围控制
    mode2DLayout->addSpacing(20);
    
    chkAutoYRange_ = new QCheckBox(tr("Y轴自动"), this);
    chkAutoYRange_->setChecked(true);
    mode2DLayout->addWidget(chkAutoYRange_);
    
    QLabel *lblYMin = new QLabel(tr("Min:"), this);
    mode2DLayout->addWidget(lblYMin);
    
    spinYMin_ = new QSpinBox(this);
    spinYMin_->setRange(0, 65535);
    spinYMin_->setValue(0);
    spinYMin_->setEnabled(false);
    mode2DLayout->addWidget(spinYMin_);
    
    QLabel *lblYMax = new QLabel(tr("Max:"), this);
    mode2DLayout->addWidget(lblYMax);
    
    spinYMax_ = new QSpinBox(this);
    spinYMax_->setRange(0, 65535);
    spinYMax_->setValue(65535);
    spinYMax_->setEnabled(false);
    mode2DLayout->addWidget(spinYMax_);
    
    mode2DLayout->addStretch();
    
    connect(chk2DMode_, &QCheckBox::toggled, this, [this](bool checked) {
        spin2DPixelX_->setEnabled(checked);
        spin2DPixelY_->setEnabled(checked);
        
        // Y轴控制也受2D模式开关影响
        bool yControlsEnabled = checked && !chkAutoYRange_->isChecked();
        chkAutoYRange_->setEnabled(checked);
        spinYMin_->setEnabled(yControlsEnabled);
        spinYMax_->setEnabled(yControlsEnabled);
        
        update2DMode();
    });
    
    connect(spin2DPixelX_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        update2DMode();
    });
    
    // Y轴控制连接
    connect(chkAutoYRange_, &QCheckBox::toggled, this, [this](bool checked) {
        spinYMin_->setEnabled(!checked && chk2DMode_->isChecked());
        spinYMax_->setEnabled(!checked && chk2DMode_->isChecked());
        if (pixelValueWidget_) {
            pixelValueWidget_->setYAxisRange(checked, spinYMin_->value(), spinYMax_->value());
        }
    });
    
    connect(spinYMin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (pixelValueWidget_) {
            pixelValueWidget_->setYAxisRange(chkAutoYRange_->isChecked(), val, spinYMax_->value());
        }
    });
    
    connect(spinYMax_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (pixelValueWidget_) {
            pixelValueWidget_->setYAxisRange(chkAutoYRange_->isChecked(), spinYMin_->value(), val);
        }
    });
    
    connect(spin2DPixelY_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        update2DMode();
    });
    
#if FORCE_2D_MODE
    chk2DMode_->setChecked(true);
    chk2DMode_->setVisible(false);
    spin2DPixelX_->setEnabled(true);
    spin2DPixelY_->setEnabled(true);
#endif
    
    mainLayout->addLayout(mode2DLayout);
    
    setLayout(mainLayout);
}

void Noise3DSurfaceWidget::setData(const QVector<double> &stdDevs, int width, int height)
{
    dataWidth_ = width;
    dataHeight_ = height;
    frameCount_ = 0;
    
    // 计算统计信息
    if (!stdDevs.isEmpty()) {
        minValue_ = stdDevs[0];
        maxValue_ = stdDevs[0];
        double sum = 0.0;
        
        for (double val : stdDevs) {
            if (val < minValue_) minValue_ = val;
            if (val > maxValue_) maxValue_ = val;
            sum += val;
        }
        
        avgValue_ = sum / stdDevs.size();
    }
    
    updateInfoLabel();
    glWidget_->setData(stdDevs, width, height);
}

void Noise3DSurfaceWidget::setStackData(const QVector<QVector<uint16_t>> &frames, int roiWidth, int roiHeight,
                                        int roiStartX, int roiStartY)
{
    dataWidth_ = roiWidth;
    dataHeight_ = roiHeight;
    roiStartX_ = roiStartX;
    roiStartY_ = roiStartY;
    frameCount_ = frames.size();

    // 统计信息：像素值范围/平均
    bool inited = false;
    double sum = 0.0;
    qint64 count = 0;
    for (const auto &frame : frames) {
        for (uint16_t v : frame) {
            if (!inited) {
                minValue_ = maxValue_ = static_cast<double>(v);
                inited = true;
            } else {
                minValue_ = std::min(minValue_, static_cast<double>(v));
                maxValue_ = std::max(maxValue_, static_cast<double>(v));
            }
            sum += static_cast<double>(v);
            ++count;
        }
    }
    avgValue_ = (count > 0) ? (sum / static_cast<double>(count)) : 0.0;

    updateInfoLabel();
    glWidget_->setStackData(frames, roiWidth, roiHeight, roiStartX_, roiStartY_);
    pixelValueWidget_->setData(frames, roiWidth, roiHeight, roiStartX_, roiStartY_);
    
    // Update the 2D spin box ranges
    spin2DPixelX_->setMaximum(roiWidth > 0 ? roiWidth - 1 : 0);
    spin2DPixelY_->setMaximum(roiHeight > 0 ? roiHeight - 1 : 0);
}

void Noise3DSurfaceWidget::setColorScheme(ColorScheme scheme)
{
    glWidget_->setColorScheme(scheme);
}

void Noise3DSurfaceWidget::resetView()
{
    glWidget_->resetView();
    pixelValueWidget_->resetView();
}

void Noise3DSurfaceWidget::update2DMode()
{
    bool enabled = chk2DMode_->isChecked();
    int pixelX = spin2DPixelX_->value();
    int pixelY = spin2DPixelY_->value();
    pixelValueWidget_->set2DMode(enabled, pixelX, pixelY);
    
    // 确保Y轴设置也被应用
    pixelValueWidget_->setYAxisRange(chkAutoYRange_->isChecked(), 
                                     spinYMin_->value(), 
                                     spinYMax_->value());
}

void Noise3DSurfaceWidget::updateInfoLabel()
{
    if (frameCount_ > 0) {
        lblInfo_->setText(tr("采样数: %1, ROI: %2x%3 (起点: %4,%5)")
                          .arg(frameCount_)
                          .arg(dataWidth_)
                          .arg(dataHeight_)
                          .arg(roiStartX_)
                          .arg(roiStartY_));
    } else {
        lblInfo_->setText(tr("数据: %1x%2, 范围: [%3, %4], 平均: %5")
                          .arg(dataWidth_)
                          .arg(dataHeight_)
                          .arg(minValue_, 0, 'f', 4)
                          .arg(maxValue_, 0, 'f', 4)
                          .arg(avgValue_, 0, 'f', 4));
    }
}

void Noise3DSurfaceWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

bool Noise3DSurfaceWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (!event || event->type() != QEvent::MouseButtonDblClick) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == glWidget_) {
        emit surfaceViewDoubleClicked();
        return true;
    }

    if (watched == pixelValueWidget_) {
        emit pixelViewDoubleClicked();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void Noise3DSurfaceWidget::setViewMode(ViewMode mode)
{
    if (!glWidget_ || !pixelValueWidget_) {
        return;
    }

    switch (mode) {
    case BothViews:
        glWidget_->setVisible(true);
        pixelValueWidget_->setVisible(true);
        break;
    case SurfaceOnly:
        glWidget_->setVisible(true);
        pixelValueWidget_->setVisible(false);
        break;
    case PixelOnly:
        glWidget_->setVisible(false);
        pixelValueWidget_->setVisible(true);
        break;
    }

    if (splitter_) {
        splitter_->update();
    }
}

// ============================================================================
// GLWidget Implementation
// ============================================================================

Noise3DSurfaceWidget::GLWidget::GLWidget(Noise3DSurfaceWidget *parent)
    : QOpenGLWidget(parent)
    , parentWidget_(parent)
{
    // Set OpenGL format explicitly to ensure proper initialization
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    setFormat(format);
    
    setMouseTracking(true);
}

Noise3DSurfaceWidget::GLWidget::~GLWidget()
{
    makeCurrent();
    // 清理资源
    doneCurrent();
}

void Noise3DSurfaceWidget::GLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    
    glClearColor(backgroundColorR_, backgroundColorG_, backgroundColorB_, backgroundColorA_);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.0f);

    // 初始化球面VBO用于GPU加速渲染
    initializeSphereVBO();
}

void Noise3DSurfaceWidget::GLWidget::initializeSphereVBO()
{
    if (sphereVBOInitialized_) return;

    const int stacks = 50;
    const int slices = 50;

    // 预计算球面顶点（半径1.0）
    sphereVertices_.clear();
    sphereNormals_.clear();

    for (int i = 0; i < stacks; ++i) {
        float phi1 = static_cast<float>(i) / stacks * M_PI;
        float phi2 = static_cast<float>(i + 1) / stacks * M_PI;

        for (int j = 0; j <= slices; ++j) {
            float theta = static_cast<float>(j) / slices * 2.0f * M_PI;

            // 顶点1
            float x1 = sinf(phi1) * cosf(theta);
            float y1 = sinf(phi1) * sinf(theta);
            float z1 = cosf(phi1);
            sphereVertices_.append(QVector3D(x1, y1, z1));
            sphereNormals_.append(QVector3D(x1, y1, z1));

            // 顶点2
            float x2 = sinf(phi2) * cosf(theta);
            float y2 = sinf(phi2) * sinf(theta);
            float z2 = cosf(phi2);
            sphereVertices_.append(QVector3D(x2, y2, z2));
            sphereNormals_.append(QVector3D(x2, y2, z2));
        }
    }

    sphereVertexCount_ = sphereVertices_.size();

    // 创建VBO (OpenGL 2.1兼容)
    glGenBuffers(1, &sphereVBO_);

    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO_);

    // 分配足够的空间：顶点 + 法线
    int totalSize = (sphereVertices_.size() + sphereNormals_.size()) * sizeof(QVector3D);
    glBufferData(GL_ARRAY_BUFFER, totalSize, nullptr, GL_STATIC_DRAW);

    // 上传顶点数据
    glBufferSubData(GL_ARRAY_BUFFER, 0, sphereVertices_.size() * sizeof(QVector3D), sphereVertices_.constData());
    // 上传法线数据
    glBufferSubData(GL_ARRAY_BUFFER, sphereVertices_.size() * sizeof(QVector3D),
                    sphereNormals_.size() * sizeof(QVector3D), sphereNormals_.constData());

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    sphereVBOInitialized_ = true;
}

void Noise3DSurfaceWidget::GLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void Noise3DSurfaceWidget::GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 设置投影矩阵
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    float aspect = static_cast<float>(width()) / static_cast<float>(height());
    float fov = 45.0f;
    float zNear = 0.1f;
    float zFar = 100.0f;
    float f = 1.0f / std::tan(fov * 3.14159265f / 360.0f);
    
    // 手动设置透视投影矩阵
    float mat[16] = {0};
    mat[0] = f / aspect;
    mat[5] = f;
    mat[10] = (zFar + zNear) / (zNear - zFar);
    mat[11] = -1.0f;
    mat[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    glMultMatrixf(mat);
    
    // 设置模型视图矩阵
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // 相机位置
    glTranslatef(0.0f, 0.0f, -3.5f * zoom_);
    
    // 应用旋转
    glRotatef(rotationX_, 1.0f, 0.0f, 0.0f);
    glRotatef(rotationY_, 0.0f, 1.0f, 0.0f);
    glRotatef(rotationZ_, 0.0f, 0.0f, 1.0f);
    
    // 绘制坐标轴
    drawAxes();
    
    // 绘制网格
    drawGrid();
    
    // 绘制主体
    if (renderMode_ == RenderMode::PointCloudStack) {
        drawVoxelSpheres();
    } else {
        drawSurface();
    }

    // 叠加刻度/标签
    drawOverlay();
}

void Noise3DSurfaceWidget::GLWidget::setData(const QVector<double> &stdDevs, int width, int height)
{
    renderMode_ = RenderMode::SurfaceStdDev;
    generateSurfaceMesh(stdDevs, width, height);
    update();
}

void Noise3DSurfaceWidget::GLWidget::setStackData(const QVector<QVector<uint16_t>> &frames, int roiWidth, int roiHeight, int roiStartX, int roiStartY)
{
    renderMode_ = RenderMode::PointCloudStack;
    roiStartX_ = roiStartX;
    roiStartY_ = roiStartY;
    generatePointCloud(frames, roiWidth, roiHeight);
    update();
}

void Noise3DSurfaceWidget::GLWidget::setColorScheme(Noise3DSurfaceWidget::ColorScheme scheme)
{
    colorScheme_ = scheme;
    
    // 重新生成表面网格以更新颜色
    if (!vertices_.isEmpty() && dataWidth_ > 0 && dataHeight_ > 0) {
        // 重新着色现有顶点
        for (int i = 0; i < vertices_.size(); ++i) {
            vertices_[i].color = getColorForValue(vertices_[i].valueNorm);
        }
    }
    
    update();
}

void Noise3DSurfaceWidget::GLWidget::resetView()
{
    rotationX_ = -30.0f;
    rotationY_ = 0.0f;
    rotationZ_ = 0.0f;
    zoom_ = 1.0f;
    update();
}

void Noise3DSurfaceWidget::GLWidget::generateSurfaceMesh(const QVector<double> &stdDevs, int width, int height)
{
    vertices_.clear();
    indices_.clear();
    
    if (stdDevs.isEmpty() || width <= 0 || height <= 0) {
        return;
    }
    
    dataWidth_ = width;
    dataHeight_ = height;
    
    // 找到最小和最大值用于归一化
    minZ_ = stdDevs[0];
    maxZ_ = stdDevs[0];
    for (double val : stdDevs) {
        if (val < minZ_) minZ_ = val;
        if (val > maxZ_) maxZ_ = val;
    }
    
    // 避免除以零
    if (std::abs(maxZ_ - minZ_) < 1e-10) {
        maxZ_ = minZ_ + 1.0;
    }

    // 轴范围：X=ROI X, Y=ROI Y, Z=标准差值
    axisXMin_ = roiStartX_;
    axisXMax_ = roiStartX_ + width - 1;
    axisYMin_ = roiStartY_;
    axisYMax_ = roiStartY_ + height - 1;
    axisZMin_ = 0;
    axisZMax_ = 100; // 仅用于显示刻度（标准差用颜色/高度表示，刻度用归一化百分比）
    
    // 生成顶点
    // 统一映射到[-1,1]，原点放在数据最小角：(-1,-1,-1)
    float xDen = std::max(1, width - 1);
    float yDen = std::max(1, height - 1);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            double value = stdDevs[idx];
            
            SurfacePoint point;
            float nx = static_cast<float>(x) / xDen;
            float ny = static_cast<float>(y) / yDen;
            float nz = static_cast<float>((value - minZ_) / (maxZ_ - minZ_));

            point.x = -1.0f + nx * 2.0f;
            point.y = -1.0f + ny * 2.0f;
            point.z = -1.0f + nz * 2.0f;
            point.valueNorm = nz;
            
            // 根据z值设置颜色
            point.color = getColorForValue(point.valueNorm);
            
            vertices_.append(point);
        }
    }
    
    // 生成索引 (三角形网格)
    for (int y = 0; y < height - 1; ++y) {
        for (int x = 0; x < width - 1; ++x) {
            int topLeft = y * width + x;
            int topRight = topLeft + 1;
            int bottomLeft = (y + 1) * width + x;
            int bottomRight = bottomLeft + 1;
            
            // 第一个三角形
            indices_.append(topLeft);
            indices_.append(bottomLeft);
            indices_.append(topRight);
            
            // 第二个三角形
            indices_.append(topRight);
            indices_.append(bottomLeft);
            indices_.append(bottomRight);
        }
    }
}

void Noise3DSurfaceWidget::GLWidget::generatePointCloud(const QVector<QVector<uint16_t>> &frames, int roiWidth, int roiHeight)
{
    vertices_.clear();
    indices_.clear();

    frameCount_ = frames.size();
    dataWidth_ = roiWidth;
    dataHeight_ = roiHeight;

    if (frameCount_ <= 0 || roiWidth <= 0 || roiHeight <= 0) {
        return;
    }

    // 统计像素最小/最大用于归一化
    uint16_t minV = 0;
    uint16_t maxV = 0;
    bool inited = false;
    for (const auto &frame : frames) {
        if (frame.size() != roiWidth * roiHeight) {
            continue;
        }
        for (uint16_t v : frame) {
            if (!inited) {
                minV = maxV = v;
                inited = true;
            } else {
                minV = std::min(minV, v);
                maxV = std::max(maxV, v);
            }
        }
    }
    if (!inited || minV == maxV) {
        minV = 0;
        maxV = 1;
    }
    minZ_ = static_cast<double>(minV);
    maxZ_ = static_cast<double>(maxV);

    // 轴：X=帧序号(1..N)，Y=ROI X，Z=ROI Y
    axisXMin_ = 1;
    axisXMax_ = frameCount_;
    axisYMin_ = roiStartX_;
    axisYMax_ = roiStartX_ + roiWidth - 1;
    axisZMin_ = roiStartY_;
    axisZMax_ = roiStartY_ + roiHeight - 1;

    // 自动降采样：控制点数上限，避免卡死（体素模式更重）
#if NOISE3D_STACK_VOXEL_MODE
    const int maxPoints = 40000;
#else
    const int maxPoints = 200000;
#endif
    const qint64 total = static_cast<qint64>(frameCount_) * roiWidth * roiHeight;
    pointStride_ = 1;
    if (total > maxPoints) {
        double ratio = static_cast<double>(total) / static_cast<double>(maxPoints);
        pointStride_ = std::max(1, static_cast<int>(std::ceil(std::sqrt(ratio))));
    }

    auto toNorm = [](int v, int vmin, int vmax) -> float {
        if (vmax == vmin) return 0.0f;
        return static_cast<float>(v - vmin) / static_cast<float>(vmax - vmin);
    };

    vertices_.reserve(static_cast<int>(std::min<qint64>(maxPoints, total)));

    // 计算均值（按降采样后的点统计，避免巨量遍历）
    double sumV = 0.0;
    qint64 cntV = 0;
    for (int fi = 0; fi < frameCount_; ++fi) {
        const auto &frame = frames[fi];
        if (frame.size() != roiWidth * roiHeight) continue;
        for (int ry = 0; ry < roiHeight; ry += pointStride_) {
            for (int rx = 0; rx < roiWidth; rx += pointStride_) {
                sumV += static_cast<double>(frame[ry * roiWidth + rx]);
                ++cntV;
            }
        }
    }
    meanValue_ = (cntV > 0) ? (sumV / static_cast<double>(cntV)) : 0.0;
    minValueRaw_ = minZ_;
    maxValueRaw_ = maxZ_;

    // 体素基础半径：不超过单元间距的一半，保证不互相影响/不重叠
    float dx = (axisXMax_ > axisXMin_) ? (2.0f / static_cast<float>(axisXMax_ - axisXMin_)) : 2.0f;
    float dy = (axisYMax_ > axisYMin_) ? (2.0f / static_cast<float>(axisYMax_ - axisYMin_)) : 2.0f;
    float dz = (axisZMax_ > axisZMin_) ? (2.0f / static_cast<float>(axisZMax_ - axisZMin_)) : 2.0f;
    // stride 会增大点间隔
    dy *= static_cast<float>(pointStride_);
    dz *= static_cast<float>(pointStride_);
    float cell = std::min(dx, std::min(dy, dz));
    voxelBaseHalf_ = std::max(0.003f, cell * 0.45f);

    for (int fi = 0; fi < frameCount_; ++fi) {
        const auto &frame = frames[fi];
        if (frame.size() != roiWidth * roiHeight) {
            continue;
        }
        int frameLabel = fi + 1;
        float fx = toNorm(frameLabel, axisXMin_, axisXMax_);
        float x = -1.0f + fx * 2.0f;

        for (int ry = 0; ry < roiHeight; ry += pointStride_) {
            int absY = roiStartY_ + ry;
            float fz = toNorm(absY, axisZMin_, axisZMax_);
            float z = -1.0f + fz * 2.0f;
            for (int rx = 0; rx < roiWidth; rx += pointStride_) {
                int absX = roiStartX_ + rx;
                float fy = toNorm(absX, axisYMin_, axisYMax_);
                float y = -1.0f + fy * 2.0f;

                uint16_t v = frame[ry * roiWidth + rx];
                float vn = static_cast<float>(static_cast<double>(v - minV) / static_cast<double>(maxV - minV));
                vn = std::max(0.0f, std::min(1.0f, vn));

                // 以均值为中心的发散配色：>均值红递增，<均值蓝递增，均值处接近白
                double denom = std::max(std::abs(maxZ_ - meanValue_), std::abs(meanValue_ - minZ_));
                if (denom < 1e-9) denom = 1.0;
                double signedDev = (static_cast<double>(v) - meanValue_) / denom; // [-1,1]左右
                signedDev = std::max(-1.0, std::min(1.0, signedDev));
                float t = static_cast<float>(std::abs(signedDev));
                QVector3D diverging;
                if (signedDev >= 0.0) {
                    // 白 -> 红
                    diverging = QVector3D(1.0f, 1.0f - t, 1.0f - t);
                } else {
                    // 白 -> 蓝
                    diverging = QVector3D(1.0f - t, 1.0f - t, 1.0f);
                }

                SurfacePoint p;
                p.x = x;
                p.y = y;
                p.z = z;
                p.valueNorm = vn;
                // 颜色：默认用“均值发散红蓝”，但仍受配色方案影响（用户切换时）
                // 这里存放一种“基准颜色”；切换配色方案时会用 valueNorm 重新计算。
                p.color = (colorScheme_ == Noise3DSurfaceWidget::HeatMap ||
                           colorScheme_ == Noise3DSurfaceWidget::BlueRed ||
                           colorScheme_ == Noise3DSurfaceWidget::GrayScale ||
                           colorScheme_ == Noise3DSurfaceWidget::Viridis ||
                           colorScheme_ == Noise3DSurfaceWidget::Rainbow)
                              ? diverging
                              : diverging;
                // 体积/尺寸编码：用像素值大小（vn）编码，保证单元内不重叠
                p.sizeNorm = vn;
                vertices_.push_back(p);
            }
        }
    }
}

QVector3D Noise3DSurfaceWidget::GLWidget::getColorForValue(double value) const
{
    // 确保value在[0, 1]范围内
    value = std::max(0.0, std::min(1.0, value));
    
    switch (colorScheme_) {
    case Noise3DSurfaceWidget::Rainbow: {
        // 彩虹色: 蓝 -> 青 -> 绿 -> 黄 -> 红
        if (value < 0.25) {
            float t = value / 0.25f;
            return QVector3D(0, t, 1);
        } else if (value < 0.5) {
            float t = (value - 0.25f) / 0.25f;
            return QVector3D(0, 1, 1 - t);
        } else if (value < 0.75) {
            float t = (value - 0.5f) / 0.25f;
            return QVector3D(t, 1, 0);
        } else {
            float t = (value - 0.75f) / 0.25f;
            return QVector3D(1, 1 - t, 0);
        }
    }
    
    case Noise3DSurfaceWidget::HeatMap: {
        // 热力图: 黑 -> 红 -> 黄 -> 白
        if (value < 0.33) {
            float t = value / 0.33f;
            return QVector3D(t, 0, 0);
        } else if (value < 0.67) {
            float t = (value - 0.33f) / 0.34f;
            return QVector3D(1, t, 0);
        } else {
            float t = (value - 0.67f) / 0.33f;
            return QVector3D(1, 1, t);
        }
    }
    
    case Noise3DSurfaceWidget::GrayScale:
        // 灰度
        return QVector3D(value, value, value);
    
    case Noise3DSurfaceWidget::BlueRed:
        // 蓝到红的渐变
        return QVector3D(value, 0, 1 - value);
    
    case Noise3DSurfaceWidget::Viridis: {
        // Viridis配色 (近似)
        float r = 0.267f + value * (0.993f - 0.267f);
        float g = 0.005f + value * (0.906f - 0.005f);
        float b = 0.329f + value * (0.144f - 0.329f);
        
        // 使用分段线性插值获得更好的Viridis效果
        if (value < 0.5) {
            float t = value * 2.0f;
            r = 0.267f * (1 - t) + 0.128f * t;
            g = 0.005f * (1 - t) + 0.567f * t;
            b = 0.329f * (1 - t) + 0.551f * t;
        } else {
            float t = (value - 0.5f) * 2.0f;
            r = 0.128f * (1 - t) + 0.993f * t;
            g = 0.567f * (1 - t) + 0.906f * t;
            b = 0.551f * (1 - t) + 0.144f * t;
        }
        return QVector3D(r, g, b);
    }
    
    default:
        return QVector3D(value, value, value);
    }
}

void Noise3DSurfaceWidget::GLWidget::drawGrid()
{
    glColor4f(0.3f, 0.3f, 0.3f, 0.5f);
    glBegin(GL_LINES);
    
    // 在最小Z平面画网格（与坐标原点一致）
    const float z = -1.0f;
    for (float i = -1.0f; i <= 1.0f + 1e-6f; i += 0.2f) {
        glVertex3f(-1.0f, i, z);
        glVertex3f(1.0f, i, z);

        glVertex3f(i, -1.0f, z);
        glVertex3f(i, 1.0f, z);
    }
    
    glEnd();
}

void Noise3DSurfaceWidget::GLWidget::drawAxes()
{
    glLineWidth(2.0f);
    
    // X轴 - 红色
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(1.0f, -1.0f, -1.0f);
    glEnd();
    
    // Y轴 - 绿色
    glBegin(GL_LINES);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f, 1.0f, -1.0f);
    glEnd();
    
    // Z轴 - 蓝色
    glBegin(GL_LINES);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f, -1.0f, 1.0f);
    glEnd();
    
    glLineWidth(1.0f);
}

void Noise3DSurfaceWidget::GLWidget::drawSurface()
{
    if (vertices_.isEmpty() || indices_.isEmpty()) {
        return;
    }
    
    // 绘制三角形网格 (带颜色插值)
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < indices_.size(); i += 3) {
        for (int j = 0; j < 3; ++j) {
            int idx = indices_[i + j];
            const SurfacePoint &p = vertices_[idx];
            glColor3f(p.color.x(), p.color.y(), p.color.z());
            glVertex3f(p.x, p.y, p.z);
        }
    }
    glEnd();
    
    // 绘制网格线 (可选，使表面更清晰)
    glColor4f(0.2f, 0.2f, 0.2f, 0.3f);
    glBegin(GL_LINES);
    for (int i = 0; i < indices_.size(); i += 3) {
        for (int j = 0; j < 3; ++j) {
            int idx1 = indices_[i + j];
            int idx2 = indices_[i + (j + 1) % 3];
            const SurfacePoint &p1 = vertices_[idx1];
            const SurfacePoint &p2 = vertices_[idx2];
            glVertex3f(p1.x, p1.y, p1.z);
            glVertex3f(p2.x, p2.y, p2.z);
        }
    }
    glEnd();
}

void Noise3DSurfaceWidget::GLWidget::drawPointCloud()
{
    if (vertices_.isEmpty()) {
        return;
    }

#if NOISE3D_STACK_VOXEL_MODE
    drawVoxelSpheres();
#else
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (const auto &p : vertices_) {
        glColor3f(p.color.x(), p.color.y(), p.color.z());
        glVertex3f(p.x, p.y, p.z);
    }
    glEnd();
#endif
}

void Noise3DSurfaceWidget::GLWidget::drawVoxelSpheres()
{
    // 使用不透明或近似不透明，避免点之间“互相影响”(颜色叠加)
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // 绑定VBO (OpenGL 2.1兼容)
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO_);

    // 设置顶点指针
    glVertexPointer(3, GL_FLOAT, 0, nullptr);
    glEnableClientState(GL_VERTEX_ARRAY);

    // 设置法线指针（如果需要光照）
    glNormalPointer(GL_FLOAT, 0, reinterpret_cast<void*>(sphereVertices_.size() * sizeof(QVector3D)));
    glEnableClientState(GL_NORMAL_ARRAY);

    for (const auto &p : vertices_) {
        // sizeNorm:[0,1] -> radius: [0.15, 1.0] * baseHalf
        float r = voxelBaseHalf_ * (0.15f + 0.85f * std::max(0.0f, std::min(1.0f, p.sizeNorm)));

        glColor4f(p.color.x(), p.color.y(), p.color.z(), 0.95f);

        // 应用变换：缩放 + 平移
        glPushMatrix();
        glTranslatef(p.x, p.y, p.z);
        glScalef(r, r, r);

        // 使用VBO绘制球面
        glDrawArrays(GL_QUAD_STRIP, 0, sphereVertexCount_);

        glPopMatrix();
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

QPointF Noise3DSurfaceWidget::GLWidget::projectToScreen(float x, float y, float z, bool *ok)
{
    GLdouble model[16];
    GLdouble proj[16];
    GLint viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, viewport);

    // 手动做一次 (proj * model * vec)
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

void Noise3DSurfaceWidget::GLWidget::drawOverlay()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(230, 230, 230));

    // 轴端点标签
    {
        bool ok1 = false;
        QPointF pX = projectToScreen(1.0f, -1.0f, -1.0f, &ok1);
        if (ok1) painter.drawText(pX + QPointF(6, -4), renderMode_ == RenderMode::PointCloudStack ? tr("帧数") : tr("ROI X"));

        bool ok2 = false;
        QPointF pY = projectToScreen(-1.0f, 1.0f, -1.0f, &ok2);
        if (ok2) painter.drawText(pY + QPointF(6, -4), renderMode_ == RenderMode::PointCloudStack ? tr("ROI X") : tr("ROI Y"));

        bool ok3 = false;
        QPointF pZ = projectToScreen(-1.0f, -1.0f, 1.0f, &ok3);
        if (ok3) painter.drawText(pZ + QPointF(6, -4), renderMode_ == RenderMode::PointCloudStack ? tr("ROI Y") : tr("标准差"));
    }

    // 自动刻度（简单整数刻度）
    auto drawAxisTicks = [&](char axis, int vmin, int vmax, const QString &suffix) {
        if (vmax <= vmin) return;
        int approxTicks = 5;
        int span = vmax - vmin;
        int step = std::max(1, span / approxTicks);

        for (int v = vmin; v <= vmax; v += step) {
            float t = static_cast<float>(v - vmin) / static_cast<float>(vmax - vmin);
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

            // tick line
            glColor4f(0.8f, 0.8f, 0.8f, 0.7f);
            glBegin(GL_LINES);
            glVertex3f(px, py, pz);
            glVertex3f(px + tx, py + ty, pz + tz);
            glEnd();

            bool ok = false;
            QPointF sp = projectToScreen(px + tx, py + ty, pz + tz, &ok);
            if (ok) {
                painter.drawText(sp + QPointF(2, 10), QString::number(v) + suffix);
            }
        }
    };

    if (renderMode_ == RenderMode::PointCloudStack) {
        drawAxisTicks('x', axisXMin_, axisXMax_, QString());
        drawAxisTicks('y', axisYMin_, axisYMax_, tr("px"));
        drawAxisTicks('z', axisZMin_, axisZMax_, tr("px"));
    } else {
        drawAxisTicks('x', axisXMin_, axisXMax_, tr("px"));
        drawAxisTicks('y', axisYMin_, axisYMax_, tr("px"));
        painter.setPen(QColor(200, 200, 200));
        painter.drawText(QPointF(10, height() - 10), tr("Z: 标准差已归一化显示 (min=%1, max=%2)")
                         .arg(minZ_, 0, 'f', 4)
                         .arg(maxZ_, 0, 'f', 4));
    }
}

void Noise3DSurfaceWidget::GLWidget::mousePressEvent(QMouseEvent *event)
{
    lastMousePos_ = event->pos();
}

void Noise3DSurfaceWidget::GLWidget::mouseMoveEvent(QMouseEvent *event)
{
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

void Noise3DSurfaceWidget::GLWidget::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() / 120.0f;
    zoom_ *= (1.0f - delta * 0.1f);
    zoom_ = std::max(0.1f, std::min(5.0f, zoom_));
    update();
}
