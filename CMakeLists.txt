cmake_minimum_required(VERSION 3.16)
project(PictureView VERSION 1.5.2 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt6 包
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent LinguistTools)

# 查找 X11 库 (仅Linux/Unix系统需要，且非macOS)
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    # 可选但推荐：也查找 Xext 扩展库，某些X11功能需要它
    find_package(X11 COMPONENTS Xext)
endif()

# 启用 Qt 的自动处理
qt_standard_project_setup()

# 查找 libarchive
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)

# 设置源文件
set(SOURCES
    src/main.cpp
    src/archivehandler.cpp
    src/canvascontrolpanel.cpp
    src/configmanager.cpp
    src/imagewidget_archive.cpp
    src/imagewidget_canvas.cpp
    src/imagewidget_config.cpp
    src/imagewidget_core.cpp
    src/imagewidget_file.cpp
    src/imagewidget_fileops.cpp
    src/imagewidget_help.cpp
    src/imagewidget_keyboard.cpp
    src/imagewidget_menu.cpp
    src/imagewidget_mouse.cpp
    src/imagewidget_shortcuts.cpp
    src/imagewidget_slideshow.cpp
    src/imagewidget_transform.cpp
    src/imagewidget_view.cpp
    src/imagewidget_viewmode.cpp
    src/thumbnailwidget.cpp
)

# 设置头文件
set(HEADERS
    src/archivehandler.h
    src/canvascontrolpanel.h
    src/configmanager.h
    src/imagewidget.h
    src/thumbnailwidget.h
)

# 创建可执行文件
qt_add_executable(PictureView
    ${SOURCES}
    ${HEADERS}
    src/platform_compat.h
    src/canvasoverlay.h
    src/canvasoverlay.cpp


)

# 设置资源前缀（必须在 qt_add_resources 之前）
set_target_properties(PictureView PROPERTIES
    QT_RESOURCE_PREFIX "/"
)

# 添加资源文件 - 指定 PREFIX
qt_add_resources(PictureView "app_resources"
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

# # 在Linux平台单独链接X11库
# if(UNIX AND NOT APPLE AND X11_FOUND)
#     target_link_libraries(PictureView PRIVATE ${X11_LIBRARIES})
#     if(X11_Xext_FOUND)
#         target_link_libraries(PictureView PRIVATE ${X11_Xext_LIBRARY})
#     endif()
# endif()
if(UNIX AND NOT APPLE AND X11_FOUND)
    # 明确链接 X11 库，确保 X11 库在 Qt 库之后链接
    target_link_libraries(PictureView PRIVATE
        ${X11_LIBRARIES}
    )
    # 如果找到 Xext，也需要链接
    if(X11_Xext_FOUND)
        target_link_libraries(PictureView PRIVATE ${X11_Xext_LIBRARY})
    endif()
endif()

# 包含目录
target_include_directories(PictureView PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBARCHIVE_INCLUDE_DIRS}
)

# 翻译文件设置（可选，可暂时注释）
set(TS_FILES
    translations/PictureView_zh_CN.ts
    translations/PictureView_en_US.ts
    translations/PictureView_ja_JP.ts
    translations/PictureView_ko_KR.ts
    translations/PictureView_fr_FR.ts
    translations/PictureView_de_AT.ts
    translations/PictureView_es_AR.ts
    translations/PictureView_ru_RU.ts
    translations/PictureView_pt_BR.ts
    translations/PictureView_pt_PT.ts
    translations/PictureView_zh_TW.ts
    translations/PictureView_fr_CA.ts
    translations/PictureView_it_IT.ts
    translations/PictureView_en_CA.ts
    translations/PictureView_ar_SA.ts
    translations/PictureView_hi_IN.ts
    translations/PictureView_id_ID.ts
    translations/PictureView_nl_NL.ts
    translations/PictureView_pl_PL.ts
    translations/PictureView_tr_TR.ts
    translations/PictureView_th_TH.ts
    translations/PictureView_vi_VN.ts
)

# 添加翻译目标，指定 .qm 文件
qt6_add_translations(PictureView
    TS_FILES ${TS_FILES}
)
# 构建后处理：将 .qm 文件移动到 translations 子目录，并清理根目录残留
add_custom_command(TARGET PictureView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PictureView>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/*.qm"
        "$<TARGET_FILE_DIR:PictureView>/translations/"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_CURRENT_BINARY_DIR}/*.qm"
    COMMENT "Moving .qm files to translations folder and cleaning build root"
)

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
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icons/PictureView.png")
        set_target_properties(PictureView PROPERTIES
            MACOSX_BUNDLE_ICON_FILE PictureView.png
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
