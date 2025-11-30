cmake_minimum_required(VERSION 3.16)
project(PictureView VERSION 1.4.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt6 包
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# 启用 Qt 的自动处理
qt_standard_project_setup()

# 查找 libarchive
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)

# 设置源文件
set(SOURCES
    main.cpp
    archivehandler.cpp
    canvascontrolpanel.cpp
    configmanager.cpp
    imagewidget_archive.cpp
    imagewidget_canvas.cpp
    imagewidget_config.cpp
    imagewidget_core.cpp
    imagewidget_file.cpp
    imagewidget_fileops.cpp
    imagewidget_help.cpp
    imagewidget_keyboard.cpp
    imagewidget_menu.cpp
    imagewidget_mouse.cpp
    imagewidget_shortcuts.cpp
    imagewidget_slideshow.cpp
    imagewidget_transform.cpp
    imagewidget_view.cpp
    imagewidget_viewmode.cpp
    thumbnailwidget.cpp
)

# 设置头文件
set(HEADERS
    archivehandler.h
    canvascontrolpanel.h
    configmanager.h
    imagewidget.h
    thumbnailwidget.h
)

# 创建可执行文件
qt_add_executable(PictureView
    ${SOURCES}
    ${HEADERS}
    platform_compat.h
)

# 设置资源前缀（必须在 qt_add_resources 之前）
set_target_properties(PictureView PROPERTIES
    QT_RESOURCE_PREFIX "/"
)

# 添加资源文件 - 指定 PREFIX
qt_add_resources(PictureView "app_resources"
    PREFIX "/"
    FILES "app.qrc"
)

# 链接库
target_link_libraries(PictureView PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    ${LIBARCHIVE_LIBRARIES}
)

# 包含目录
target_include_directories(PictureView PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBARCHIVE_INCLUDE_DIRS}
)

# 翻译文件设置（可选，可暂时注释）
# set(TS_FILES
#     # translations/PictureView_zh_CN.ts
#     translations/PictureView_en_US.ts
# )
#
# # 添加翻译目标
# qt_add_translation(QM_FILES ${TS_FILES})
# add_custom_target(translations ALL DEPENDS ${QM_FILES})

# 设置应用程序属性
set_target_properties(PictureView PROPERTIES
    OUTPUT_NAME "PictureView"
    MACOSX_BUNDLE_BUNDLE_NAME "PictureView"
    MACOSX_BUNDLE_COPYRIGHT "Copyright @ 2025 berylok"
    MACOSX_BUNDLE_GUI_IDENTIFIER ""
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    WIN32_EXECUTABLE TRUE
)

# Linux 特定设置
if(UNIX AND NOT APPLE)
    # 设置图标
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icons/PictureView.ico")
        set_target_properties(PictureView PROPERTIES
            MACOSX_BUNDLE_ICON_FILE PictureView.ico
        )
    endif()

    # 安装配置
    install(TARGETS PictureView
        RUNTIME DESTINATION bin
    )

    # 安装桌面文件
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/PictureView.desktop")
        install(FILES PictureView.desktop
            DESTINATION share/applications
        )
    endif()

    # 安装图标
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icons/PictureView.ico")
        install(FILES icons/PictureView.png
            DESTINATION share/icons/hicolor/256x256/apps
            RENAME PictureView.png
        )
    endif()
endif()

# Windows 特定设置（保留以备将来使用）
if(WIN32)
    # 设置图标
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icons/PictureView.ico")
        set_target_properties(PictureView PROPERTIES
            RC_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/icons/PictureView.ico"
        )
    endif()
endif()

# 编译特性设置
target_compile_definitions(PictureView PRIVATE
    $<$<CONFIG:Debug>:QT_QML_DEBUG>
)

# 设置版本信息
set_target_properties(PictureView PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
