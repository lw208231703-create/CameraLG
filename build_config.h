#ifndef CAMERA_PRJ_BUILD_CONFIG_H
#define CAMERA_PRJ_BUILD_CONFIG_H

// 统一管理工程内“宏开关/宏配置”的默认值。
// - 允许在 CMake 或编译命令行通过 -DXXX=0/1 或 -DXXX=... 覆盖
// - 代码侧尽量使用 #if XXX（而不是 #ifdef XXX），这样 XXX=0 时能正确关闭

// ------------------------
// 数据分析路径开关
// ------------------------
// 1: 统计/分析原始采集数据（可能是16位）
// 0: 统计/分析当前显示图像（通常是8位）
#ifndef ANALYZE_RAW_DATA
#define ANALYZE_RAW_DATA 1
#endif

// ------------------------
// DisplayDock 渲染路径开关
// ------------------------
// 1: 开启显示节流（连续采集 + 放大时避免UI线程被过度重绘拖死）
// 0: 关闭
#ifndef DISPLAYDOCK_ENABLE_RENDER_THROTTLE
#define DISPLAYDOCK_ENABLE_RENDER_THROTTLE 0
#endif

// 1: 新渲染路径（推荐）——按需绘制可见区域
// 0: 旧路径——生成缩放后的整幅图再显示
#ifndef DISPLAYDOCK_ENABLE_PARTIAL_PAINT
#define DISPLAYDOCK_ENABLE_PARTIAL_PAINT 1
#endif

// ------------------------
// Raw16 对齐检测(qDebug)开关
// ------------------------
// 1: 在首次收到 raw16 图像时，用采样统计判断 16bit 容器里的有效位是 LSB 对齐还是 MSB 左移对齐，并 qDebug 打印一次
// 0: 关闭（默认）
#ifndef OPENCVQTBRIDGE_ENABLE_RAW16_ALIGNMENT_QDEBUG
#define OPENCVQTBRIDGE_ENABLE_RAW16_ALIGNMENT_QDEBUG 0
#endif

// ------------------------
// 噪声/像素 3D 视图开关
// ------------------------
// 强制2D模式（用于弱GPU环境或调试）
#ifndef FORCE_2D_MODE
#define FORCE_2D_MODE 1
#endif

// 3D视图背景色（RGBA）
#ifndef NOISE3D_BACKGROUND_R
#define NOISE3D_BACKGROUND_R 0.10f
#endif
#ifndef NOISE3D_BACKGROUND_G
#define NOISE3D_BACKGROUND_G 0.10f
#endif
#ifndef NOISE3D_BACKGROUND_B
#define NOISE3D_BACKGROUND_B 0.15f
#endif
#ifndef NOISE3D_BACKGROUND_A
#define NOISE3D_BACKGROUND_A 1.00f
#endif

// 体素点云模式：将每个点渲染为小立方体
#ifndef NOISE3D_STACK_VOXEL_MODE
#define NOISE3D_STACK_VOXEL_MODE 1
#endif

// PixelValue3D 渲染形式
// 0=曲面 1=散点 2=柱状
#ifndef PIXEL_VALUE_3D_RENDER_STYLE
#define PIXEL_VALUE_3D_RENDER_STYLE 0
#endif
#ifndef PIXEL_VALUE_3D_RENDER_STYLE_SURFACE
#define PIXEL_VALUE_3D_RENDER_STYLE_SURFACE 0
#endif
#ifndef PIXEL_VALUE_3D_RENDER_STYLE_SCATTER
#define PIXEL_VALUE_3D_RENDER_STYLE_SCATTER 1
#endif
#ifndef PIXEL_VALUE_3D_RENDER_STYLE_BARS
#define PIXEL_VALUE_3D_RENDER_STYLE_BARS 2
#endif

// ------------------------
// 光斑检测功能开关
// ------------------------
// 1: 开启光斑检测功能
// 0: 关闭光斑检测功能（屏蔽相关UI和逻辑）
#ifndef ENABLE_SPOT_DETECTION
#define ENABLE_SPOT_DETECTION 0
#endif

// ------------------------
// 图像显示背景图开关
// ------------------------
// 1: 显示背景图
// 0: 关闭背景图
#ifndef DISPLAYDOCK_ENABLE_BACKGROUND
#define DISPLAYDOCK_ENABLE_BACKGROUND 0
#endif

// ------------------------
// 主题系统开关
// ------------------------
// 1: 启用主题系统
// 0: 禁用主题系统（使用默认样式）
#ifndef ENABLE_THEME
#define ENABLE_THEME 0
#endif

#endif // CAMERA_PRJ_BUILD_CONFIG_H
