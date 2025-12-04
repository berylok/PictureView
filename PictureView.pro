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

SOURCES += main.cpp \
    archivehandler.cpp \
    canvascontrolpanel.cpp \
    configmanager.cpp \
    imagewidget_archive.cpp \
    imagewidget_canvas.cpp \
    imagewidget_config.cpp \
    imagewidget_core.cpp \
    imagewidget_file.cpp \
    imagewidget_fileops.cpp \
    imagewidget_help.cpp \
    imagewidget_keyboard.cpp \
    imagewidget_menu.cpp \
    imagewidget_mouse.cpp \
    imagewidget_shortcuts.cpp \
    imagewidget_slideshow.cpp \
    imagewidget_transform.cpp \
    imagewidget_view.cpp \
    imagewidget_viewmode.cpp \
    thumbnailwidget.cpp

HEADERS += \
    archivehandler.h \
    canvascontrolpanel.h \
    configmanager.h \
    imagewidget.h \
    thumbnailwidget.h

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
