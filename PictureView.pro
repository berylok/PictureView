QT += core gui widgets concurrent
CONFIG += c++17

# 在 Ubuntu 上使用系统安装的 libarchive
unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += libarchive
}

# Windows 或其他情况
win32 {
    LIBS += -larchive
    INCLUDEPATH += "c:\vcpkg\installed\x64-windows\include"  # 修改为实际路径
    LIBS += -L"c:\vcpkg\installed\x64-windows\lib"
}

SOURCES += src/main.cpp \
    src/archivehandler.cpp \
    src/canvascontrolpanel.cpp \
    src/canvasoverlay.cpp \
    src/configmanager.cpp \
    src/imagewidget_archive.cpp \
    src/imagewidget_canvas.cpp \
    src/imagewidget_config.cpp \
    src/imagewidget_core.cpp \
    src/imagewidget_file.cpp \
    src/imagewidget_fileops.cpp \
    src/imagewidget_help.cpp \
    src/imagewidget_keyboard.cpp \
    src/imagewidget_menu.cpp \
    src/imagewidget_mouse.cpp \
    src/imagewidget_shortcuts.cpp \
    src/imagewidget_slideshow.cpp \
    src/imagewidget_transform.cpp \
    src/imagewidget_view.cpp \
    src/imagewidget_viewmode.cpp \
    src/thumbnailwidget.cpp

HEADERS += \
    src/archivehandler.h \
    src/canvasoverlay.h \
    src/canvascontrolpanel.h \
    src/configmanager.h \
    src/imagewidget.h \
    src/platform_compat.h \
    src/thumbnailwidget.h


# 资源文件
RESOURCES += \
    app.qrc

# 翻译支持
TRANSLATIONS += \
    # translations/PictureView_zh_CN.ts \
    translations/PictureView_en_US.ts

# 设置默认语言
DEFAULT_LANG = zh_CN

# Linux 图标配置
linux {
    # 应用程序图标
    QMAKE_TARGET_ICON = icons/PictureView.png

    # 安装目标
    target.path = /usr/bin
    desktop.files = PictureView.desktop
    desktop.path = /usr/share/applications/
    icons.path = /usr/share/icons/hicolor/256x256/apps/
    icons.files = icons/PictureView.png

    INSTALLS += target desktop icons
}


RC_ICONS = icons/PictureView.ico

# 基本应用信息
VERSION = 1.4.2.1
QMAKE_TARGET_COMPANY = "berylok"
QMAKE_TARGET_DESCRIPTION = "berylok app"
QMAKE_TARGET_COPYRIGHT = "Copyright @ 2025 berylok"
QMAKE_TARGET_PRODUCT = "PictureView"

DISTFILES +=
