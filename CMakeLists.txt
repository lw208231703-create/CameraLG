cmake_minimum_required(VERSION 3.16)

project(camerui VERSION 0.1 LANGUAGES CXX)

# 将生成的执行文件和其依赖项放到一个单独的文件夹中，保持发行版干净
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/AppDist")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/AppDist")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/AppDist")

# 优先使用工程上级 ../CUDA（可被 -D 覆盖）
# 兼容 modern CUDAToolkit 和 legacy FindCUDA 变量
if(NOT DEFINED CUDAToolkit_ROOT)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../CUDA/v13.1")
    set(CUDAToolkit_ROOT "${CMAKE_SOURCE_DIR}/../CUDA/v13.1" CACHE PATH "CUDAToolkit root (prefers ../CUDA/v13.1)")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/../CUDA")
    set(CUDAToolkit_ROOT "${CMAKE_SOURCE_DIR}/../CUDA" CACHE PATH "CUDAToolkit root (prefers ../CUDA)")
  endif()
endif()

if(NOT DEFINED CUDA_TOOLKIT_ROOT_DIR)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../CUDA/v13.1")
    set(CUDA_TOOLKIT_ROOT_DIR "${CMAKE_SOURCE_DIR}/../CUDA/v13.1" CACHE PATH "CUDA toolkit root")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/../CUDA")
    set(CUDA_TOOLKIT_ROOT_DIR "${CMAKE_SOURCE_DIR}/../CUDA" CACHE PATH "CUDA toolkit root")
  endif()
endif()

# 兼容性：设置 CUDA_PATH
if(NOT DEFINED CUDA_PATH)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../CUDA/v13.1")
    set(CUDA_PATH "${CMAKE_SOURCE_DIR}/../CUDA/v13.1" CACHE PATH "CUDA_PATH")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/../CUDA")
    set(CUDA_PATH "${CMAKE_SOURCE_DIR}/../CUDA" CACHE PATH "CUDA_PATH")
  endif()
endif()

# MVS SDK（与 CUDA 同级目录，允许 -D 覆盖）
if(NOT DEFINED MVS_ROOT)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../MVS")
    set(MVS_ROOT "${CMAKE_SOURCE_DIR}/../MVS" CACHE PATH "MVS SDK root")
  endif()
endif()

if(WIN32 AND DEFINED MVS_ROOT)
  set(MVS_INCLUDE_DIR "${MVS_ROOT}/Includes")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MVS_LIB_DIR "${MVS_ROOT}/Libraries/win64")
    set(MVS_BIN_DIR "${MVS_ROOT}/Bin/win64")
    set(MVS_RUNTIME_DIR "${MVS_ROOT}/Runtime/Win64_x64")
  else()
    set(MVS_LIB_DIR "${MVS_ROOT}/Libraries/win32")
    set(MVS_BIN_DIR "${MVS_ROOT}/Bin/win32")
    set(MVS_RUNTIME_DIR "${MVS_ROOT}/Runtime/Win32_i86")
  endif()
endif()

# Windows 下把 CUDA bin 加入运行时 PATH，便于运行期找到 DLL
if(WIN32 AND EXISTS "${CUDAToolkit_ROOT}/bin")
  set(ENV{PATH} "${CUDAToolkit_ROOT}/bin;$ENV{PATH}")
endif()

if(WIN32 AND DEFINED MVS_BIN_DIR AND EXISTS "${MVS_BIN_DIR}")
  set(ENV{PATH} "${MVS_BIN_DIR};$ENV{PATH}")
endif()

# 禁止 Windows 定义 min/max 宏，防止与 OpenCV 函数名冲突
add_compile_definitions(NOMINMAX)

# ------------------------------
# 可配置的宏开关（集中管理）
# 说明：这些宏也可以直接通过编译参数 -DXXX=0/1 覆盖。
# ------------------------------
option(ANALYZE_RAW_DATA "Analyze raw acquisition data (may be 16-bit) instead of display image" ON)
option(DISPLAYDOCK_ENABLE_RENDER_THROTTLE "Enable DisplayDock render throttle" OFF)
option(DISPLAYDOCK_ENABLE_PARTIAL_PAINT "Enable DisplayDock partial paint render path" ON)
option(FORCE_2D_MODE "Force 2D mode in noise/pixel 3D widgets" ON)
option(NOISE3D_STACK_VOXEL_MODE "Enable voxel stack mode for noise 3D view" ON)

# 0=surface 1=scatter 2=bars
set(PIXEL_VALUE_3D_RENDER_STYLE "0" CACHE STRING "PixelValue3D render style: 0=surface, 1=scatter, 2=bars")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Concurrent)

# OpenCV Configuration
# Override with: -DOpenCV_DIR=/path/to/opencv
if(NOT DEFINED OpenCV_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../opencv-cuda-build-13.1")
        set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../opencv-cuda-build-13.1" CACHE PATH "OpenCV build directory")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../opencv/build")
        set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../opencv/build" CACHE PATH "OpenCV build directory")
    endif()
endif()
find_package(OpenCV REQUIRED)

# QXlsx Configuration
# Override with: -DQXlsx_DIR=/path/to/QXlsx/QXlsx
if(NOT DEFINED QXlsx_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../QXlsx/QXlsx")
        set(QXlsx_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../QXlsx/QXlsx")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/QXlsx/QXlsx")
        set(QXlsx_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/QXlsx/QXlsx")
    else()
        message(FATAL_ERROR "QXlsx not found. Set -DQXlsx_DIR=/path/to/QXlsx/QXlsx")
    endif()
endif()
add_subdirectory("${QXlsx_DIR}" QXlsx_build)

# Sapera SDK Configuration (optional, CameraLink)
# Override with: -DSAPERA_SDK_DIR=/path/to/sapera
if(NOT DEFINED SAPERA_SDK_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../sapera")
        set(SAPERA_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sapera")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sapera")
        set(SAPERA_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sapera")
    endif()
endif()
if(DEFINED SAPERA_SDK_DIR AND EXISTS "${SAPERA_SDK_DIR}")
    set(SAPERA_LIB_DIR "${SAPERA_SDK_DIR}/Lib/Win64")
    set(SAPERA_INCLUDE_DIR "${SAPERA_SDK_DIR}/Include")
    set(SAPERA_CLASSES_DIR "${SAPERA_SDK_DIR}/Classes/Basic")
endif()

# ---- SDK 可选检测 (Phase 4: Open-source friendly) ----

# Sapera SDK detection (CameraLink)
find_path(SAPERA_CLASS_BASIC_DIR NAMES SapClassBasic.h PATHS "${SAPERA_CLASSES_DIR}")
if(SAPERA_CLASS_BASIC_DIR AND EXISTS "${SAPERA_LIB_DIR}")
    message(STATUS "Found Sapera SDK. Enabling CameraLink support.")
    set(USE_SAPERA ON)
else()
    message(WARNING "Sapera SDK NOT found. CameraLink support will be disabled.")
    set(USE_SAPERA OFF)
endif()

# Hikvision MVS SDK detection (GigE)
set(USE_HIK_GIGE OFF)
if(WIN32 AND DEFINED MVS_ROOT AND EXISTS "${MVS_INCLUDE_DIR}")
    find_path(MVS_CAMERA_CONTROL_DIR NAMES MvCameraControl.h PATHS "${MVS_INCLUDE_DIR}")
    if(MVS_CAMERA_CONTROL_DIR AND EXISTS "${MVS_LIB_DIR}/MvCameraControl.lib")
        message(STATUS "Found Hikvision MVS SDK. Enabling GigE support.")
        set(USE_HIK_GIGE ON)
    else()
        message(WARNING "Hikvision MVS SDK NOT found. GigE support will be disabled.")
    endif()
else()
    message(STATUS "MVS SDK not configured. GigE support disabled.")
endif()

set(PROJECT_SOURCES
    main.cpp
    mainwindow_refactored.cpp
    mainwindow_refactored.h
    mainwindow_acquisition.cpp
    mainwindow_ui.cpp
    mainwindow_menu.cpp
    mainwindow_state.cpp
    device_selector_widget.cpp
    device_selector_widget.h
    device_dock.cpp
    device_dock.h
    gige_dock.cpp
    gige_dock.h
    cameramanager.cpp
    cameramanager.h
    imagegrabber.cpp
    imagegrabber.h
    cameraparameter.cpp
    cameraparameter.h
    parameterwidget.cpp
    parameterwidget.h
    bit_depth_slider.cpp
    bit_depth_slider.h
    editable_number_label.cpp
    editable_number_label.h
    bad_pixel_picker_dialog.cpp
    bad_pixel_picker_dialog.h
    bad_pixel_tool_dialog.cpp
    bad_pixel_tool_dialog.h
    image_display_label.cpp
    image_display_label.h
    image_info_worker.cpp
    image_info_worker.h
    display_dock.cpp
    display_dock.h
    output_dock.cpp
    output_dock.h
    image_data_dock.cpp
    image_data_dock.h
    image_algorithm_dock.cpp
    image_algorithm_dock.h
    ui_state_utils.cpp
    save_results_utils.cpp
    readout_noise_page.cpp
    image_processing_page.cpp
    mixed_processing_page.cpp
    spot_detection_page.cpp
    detail_histogram_widget.cpp
    detail_histogram_widget.h
    noise_result_model.cpp
    noise_result_model.h
    histogram_equalization_worker.cpp
    histogram_equalization_worker.h
    histogram_equalization_display.cpp
    histogram_equalization_display.h
    zoomable_image_widget.cpp
    zoomable_image_widget.h
    image_analysis_worker.cpp
    image_analysis_worker.h
    noise_analysis_worker.cpp
    noise_analysis_worker.h
    spot_detection_worker.cpp
    spot_detection_worker.h
    spot_tracking_chart_dialog.cpp
    spot_tracking_chart_dialog.h
    mock_data_provider.cpp
    mock_data_provider.h
    thread_manager.cpp
    thread_manager.h
    image_save_worker.cpp
    image_save_worker.h
    noise_3d_surface_widget.cpp
    noise_3d_surface_widget.h
    pixel_value_3d_widget.cpp
    pixel_value_3d_widget.h
    image_algorithm_base.cpp
    image_algorithm_base.h
    image_algorithm_display.cpp
    image_algorithm_display.h
    image_algorithm_manager.cpp
    image_algorithm_manager.h
    image_algorithms_filtering.cpp
    image_algorithms_filtering.h
    image_algorithms_edge.cpp
    image_algorithms_edge.h
    image_algorithms_morphology.cpp
    image_algorithms_morphology.h
    image_algorithms_threshold.cpp
    image_algorithms_threshold.h
    image_algorithms_enhance.cpp
    image_algorithms_enhance.h
    image_algorithms_frequency.cpp
    image_algorithms_frequency.h
    image_algorithms_geometry.cpp
    image_algorithms_geometry.h
    image_algorithms_segment.cpp
    image_algorithms_segment.h
    image_processing_panel.cpp
    image_processing_panel.h
    # Core abstraction layer (always compiled, no SDK dependency)
    ImageFrameData.h
    ICameraDevice.h
    CameraFactory.h
    app_constants.h
    CameraFactory.cpp
    virtual_camera_device.h
    virtual_camera_device.cpp
)

# Conditionally add Sapera CameraLink support
if(USE_SAPERA)
    list(APPEND PROJECT_SOURCES
        sapera_camera_device.h
        sapera_camera_device.cpp
    )
    add_compile_definitions(ENABLE_SAPERA_CAMERA)
    message(STATUS "  SaperaCameraDevice included in build.")
endif()

# Conditionally add GigE support
if(USE_HIK_GIGE)
    list(APPEND PROJECT_SOURCES
        gige_camera_device.h
        gige_camera_device.cpp
    )
    add_compile_definitions(ENABLE_GIGE_CAMERA)
    message(STATUS "  GigECameraDevice included in build.")
endif()

set(PROJECT_RESOURCES
    icon/resources.qrc
)

if(WIN32)
    list(APPEND PROJECT_SOURCES camerui.rc)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(camerui
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        image_depth_converter.h image_depth_converter.cpp
        ${PROJECT_RESOURCES}
        mixed_processing_panel.cpp
        mixed_processing_panel.h
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET camerui APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(camerui SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(camerui
            ${PROJECT_SOURCES}
        )
    endif()
endif()
# 添加依赖库头文件包含路径
set(CAMERUI_INCLUDE_DIRS
    ${OpenCV_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/../QXlsx/QXlsx/header"
)
if(USE_SAPERA)
    list(APPEND CAMERUI_INCLUDE_DIRS ${SAPERA_INCLUDE_DIR} ${SAPERA_CLASSES_DIR})
endif()
if(USE_HIK_GIGE)
    if(MVS_INCLUDE_DIR)
        list(APPEND CAMERUI_INCLUDE_DIRS ${MVS_INCLUDE_DIR})
    endif()
endif()
target_include_directories(camerui PRIVATE ${CAMERUI_INCLUDE_DIRS})

# 把 CMake 选项映射为编译宏（会覆盖 build_config.h 中的默认值）
target_compile_definitions(camerui PRIVATE
    ANALYZE_RAW_DATA=$<IF:$<BOOL:${ANALYZE_RAW_DATA}>,1,0>
    DISPLAYDOCK_ENABLE_RENDER_THROTTLE=$<IF:$<BOOL:${DISPLAYDOCK_ENABLE_RENDER_THROTTLE}>,1,0>
    DISPLAYDOCK_ENABLE_PARTIAL_PAINT=$<IF:$<BOOL:${DISPLAYDOCK_ENABLE_PARTIAL_PAINT}>,1,0>
    FORCE_2D_MODE=$<IF:$<BOOL:${FORCE_2D_MODE}>,1,0>
    NOISE3D_STACK_VOXEL_MODE=$<IF:$<BOOL:${NOISE3D_STACK_VOXEL_MODE}>,1,0>
    PIXEL_VALUE_3D_RENDER_STYLE=${PIXEL_VALUE_3D_RENDER_STYLE}
)

# CUDA detection: enable CAMERUI_ENABLE_CUDA if toolkit is available
if(DEFINED CUDAToolkit_ROOT AND EXISTS "${CUDAToolkit_ROOT}")
    target_compile_definitions(camerui PRIVATE CAMERUI_ENABLE_CUDA)
    message(STATUS "  CUDA toolkit found. CAMERUI_ENABLE_CUDA defined.")
else()
    message(STATUS "  CUDA toolkit not found. CUDA acceleration disabled.")
endif()
# 为Windows平台添加SDK库链接（按 SDK 可用性条件编译）
if(WIN32)
    if(USE_SAPERA)
        target_link_directories(camerui PRIVATE ${SAPERA_LIB_DIR})
        target_link_libraries(camerui PRIVATE SapClassBasic corapi)
    endif()

    if(USE_HIK_GIGE)
        if(MVS_LIB_DIR)
            target_link_directories(camerui PRIVATE ${MVS_LIB_DIR})
        endif()
        if(EXISTS "${MVS_LIB_DIR}/MvCameraControl.lib")
            target_link_libraries(camerui PRIVATE MvCameraControl)
        endif()
    endif()
endif()

# 链接Qt模块和第三方库
target_link_libraries(camerui PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    ${OpenCV_LIBS}
    QXlsx::QXlsx
)

# Windows平台特定：链接系统OpenGL库
if(WIN32)
    target_link_libraries(camerui PRIVATE opengl32)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.camerui)
endif()
set_target_properties(camerui PROPERTIES
    OUTPUT_NAME "CameraLinkFrame"
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS camerui
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Copy Third-party DLLs to the executable directory
if(WIN32)
    # Generate a helper script to copy only DLLs (avoids pdb, exe, xml, etc.)
    set(DEPLOY_LIBS_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/deploy_libs.cmake")
    file(WRITE "${DEPLOY_LIBS_SCRIPT}" "
        macro(copy_dlls DIR)
            if(NOT \"\${DIR}\" STREQUAL \"\" AND EXISTS \"\${DIR}\")
                file(GLOB DLLS \"\${DIR}/*.dll\")
                if(DLLS)
                    file(COPY \${DLLS} DESTINATION \"\${TARGET_DIR}\")
                endif()
            endif()
        endmacro()
        macro(copy_mvs_runtime DIR)
            if(NOT \"\${DIR}\" STREQUAL \"\" AND EXISTS \"\${DIR}\")
                file(COPY \"\${DIR}/\" DESTINATION \"\${TARGET_DIR}\")
            endif()
        endmacro()
        macro(copy_file FILE_PATH)
            if(NOT \"\${FILE_PATH}\" STREQUAL \"\" AND EXISTS \"\${FILE_PATH}\")
                file(COPY \"\${FILE_PATH}\" DESTINATION \"\${TARGET_DIR}\")
            endif()
        endmacro()

        copy_dlls(\"\${OPENCV_BIN_DIR}\")
        copy_dlls(\"\${MVS_BIN_DIR}\")
        copy_mvs_runtime(\"\${MVS_RUNTIME_DIR}\")
        copy_file(\"\${SAPERA_INSTALLER}\")
        copy_dlls(\"\${CUDNN_BIN_DIR}\")
        copy_dlls(\"\${CUDA_BIN_DIR}\")
        copy_dlls(\"\${CUDA_BIN_X64_DIR}\")
    ")

    # cuDNN: prefer project-local bundle for portability, fall back to system install
    # Override with: -DCUDNN_BIN_DIR=/path/to/cudnn/bin
    if(NOT DEFINED CUDNN_BIN_DIR)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CUDNN/v9.17/bin/13.1")
            set(CUDNN_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../CUDNN/v9.17/bin/13.1")
        elseif(EXISTS "C:/Program Files/NVIDIA/CUDNN/v9.17/bin/13.1")
            set(CUDNN_BIN_DIR "C:/Program Files/NVIDIA/CUDNN/v9.17/bin/13.1")
        else()
            set(CUDNN_BIN_DIR "")
            message(STATUS "cuDNN binaries not found. GPU acceleration via cuDNN will not be available at runtime.")
        endif()
    endif()

    add_custom_command(TARGET camerui POST_BUILD
        COMMAND ${CMAKE_COMMAND} 
            -DTARGET_DIR=$<TARGET_FILE_DIR:camerui>
            -DOPENCV_BIN_DIR="${OpenCV_DIR}/bin/$<CONFIG>"
            -DMVS_BIN_DIR="${MVS_BIN_DIR}"
            -DMVS_RUNTIME_DIR="${MVS_RUNTIME_DIR}"
            -DSAPERA_INSTALLER="${CMAKE_CURRENT_SOURCE_DIR}/../sapera/SaperaLTRuntimeSetup.exe"
            -DCUDNN_BIN_DIR="${CUDNN_BIN_DIR}"
            -DCUDA_BIN_DIR="${CUDAToolkit_ROOT}/bin"
            -DCUDA_BIN_X64_DIR="${CUDAToolkit_ROOT}/bin/x64"
            -P "${DEPLOY_LIBS_SCRIPT}"
        COMMENT "Copying necessary DLLs to output directory"
    )

    # -------------------------------------------------------------------------
    # Deploy Qt Libraries using windeployqt
    # -------------------------------------------------------------------------
    # Find windeployqt
    if(TARGET Qt${QT_VERSION_MAJOR}::qmake)
        get_target_property(QT_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    elseif(TARGET Qt::qmake)
        get_target_property(QT_QMAKE_EXECUTABLE Qt::qmake IMPORTED_LOCATION)
    endif()

    if(QT_QMAKE_EXECUTABLE)
        get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    endif()

    if(WINDEPLOYQT_EXECUTABLE)
        # Note: --compiler-runtime copies MSVC runtime if available
        add_custom_command(TARGET camerui POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --dir "$<TARGET_FILE_DIR:camerui>"
                --plugindir "$<TARGET_FILE_DIR:camerui>/plugins"
                --no-translations
                --compiler-runtime
                --opengl
                "$<TARGET_FILE:camerui>"
            COMMENT "Running windeployqt to deploy Qt libraries..."
        )
    else()
        message(WARNING "windeployqt not found. Qt libraries will not be copied automatically.")
    endif()

endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(camerui)
endif()
