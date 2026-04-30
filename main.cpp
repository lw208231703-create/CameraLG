#include "icon_cache.h"
#include "mainwindow_refactored.h"
#include "CameraFactory.h"
#include <QApplication>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QMessageBox>
#include <exception>
#include <QMetaType>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QDebug>

int main(int argc, char *argv[])
{
    // ===== GPU加速配置 =====
    // 1. 启用高DPI缩放（现代显示器支持）
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    
    // 2. 配置OpenGL渲染（GPU加速）
    QSurfaceFormat format;
    format.setVersion(3, 3);  // OpenGL 3.3
    format.setProfile(QSurfaceFormat::CoreProfile);  // 核心模式
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);  // 4x抗锯齿
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);  // 双缓冲
    format.setSwapInterval(1);  // 垂直同步
    format.setRenderableType(QSurfaceFormat::OpenGL);
    QSurfaceFormat::setDefaultFormat(format);
    
    // 3. 启用OpenGL共享上下文（多窗口性能优化）
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    
    // 4. 启用原生绘制（GPU加速图形）
    // 建议在没有独立显卡的机器上注释掉此行，让Qt自动选择最佳后端（可能是ANGLE或软件光栅化）
     QApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);  
    
    qRegisterMetaType<QVector<uint16_t>>("QVector<uint16_t>");

    QApplication a(argc, argv);

    // Register custom metatypes for cross-thread signal/slot (ICameraDevice types)
    CameraFactory::registerMetaTypes();
    
    // 加载并应用暗色主题（类似Visual Studio）
    // Load and apply dark theme (similar to Visual Studio)
    QFile styleFile(":/styles/dark_theme.qss");
    if (!styleFile.exists()) {
        // 如果资源文件不存在，尝试从文件系统加载
        // If resource file doesn't exist, try loading from filesystem
        styleFile.setFileName("dark_theme.qss");
    }
    
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream styleStream(&styleFile);
        QString styleSheet = styleStream.readAll();
        styleFile.close();
        
        // 验证样式表内容不为空
        // Validate stylesheet content is not empty
        if (!styleSheet.isEmpty()) {
            a.setStyleSheet(styleSheet);
        } else {
            qWarning() << "Dark theme file is empty";
        }
    } else {
        qWarning() << "Failed to load dark theme QSS file";
    }
    
    // 输出GPU信息
   // a.setStyle("windows");
    a.setWindowIcon(IconCache::applicationIcon());
    
    try {
        MainWindowRefactored w;
        w.show();
        return a.exec();
    } catch (const std::exception &e) {
        QMessageBox::critical(nullptr, "Startup Error", QString("An exception occurred:\n%1").arg(e.what()));
        return -1;
    } catch (...) {
        QMessageBox::critical(nullptr, "Startup Error", "An unknown error occurred during startup.");
        return -1;
    }
}
