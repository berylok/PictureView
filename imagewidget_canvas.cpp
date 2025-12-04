// imagewidget_canvas.cpp
#include "imagewidget.h"
#include <QScreen>
#include <QGuiApplication>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

void ImageWidget::createControlPanel()
{
    if (!controlPanel) {
        controlPanel = new CanvasControlPanel();
        connect(controlPanel, &CanvasControlPanel::exitCanvasMode, this,
                &ImageWidget::onExitCanvasMode);
        positionControlPanel();
        controlPanel->show();
        qDebug() << "控制面板已创建";
    }
}

void ImageWidget::destroyControlPanel()
{
    if (controlPanel) {
        controlPanel->hide();
        controlPanel->deleteLater();
        controlPanel = nullptr;
        qDebug() << "控制面板已销毁";
    }
}

void ImageWidget::positionControlPanel()
{
    if (controlPanel) {
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect screenGeometry = screen->availableGeometry();

        // 放置在屏幕右上角，留出一些边距
        int x = screenGeometry.width() - controlPanel->width() - 10;
        int y = 10;

        controlPanel->move(x, y);
        qDebug() << "控制面板已定位到:" << x << "," << y;
    }
}

void ImageWidget::onExitCanvasMode()
{
    qDebug() << "通过控制面板退出画布模式";
    disableCanvasMode();
}

void ImageWidget::toggleCanvasMode()
{
    if (canvasMode) {
        disableCanvasMode();
    } else {
        enableCanvasMode();
    }
}

void ImageWidget::enableCanvasMode()
{
    qDebug() << "启用画布模式";
    canvasMode = true;

    // 先隐藏窗口
    hide();

    // 保存当前窗口状态
    bool wasMaximized = isMaximized();
    QRect normalGeometry = geometry();

    // 设置窗口标志
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // 设置窗口背景为透明
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet("background-color: rgba(0, 0, 0, 0);");

    // 设置初始半透明状态
    setWindowOpacity(0.7);

    // 恢复窗口显示
    if (wasMaximized) {
        showMaximized();
    } else {
        if (normalGeometry.isValid() && !normalGeometry.isEmpty()) {
            setGeometry(normalGeometry);
        } else {
            QScreen *screen = QGuiApplication::primaryScreen();
            QRect screenGeometry = screen->availableGeometry();
            setGeometry(screenGeometry);
        }
        showNormal();
    }

    // 启用鼠标穿透
    enableMousePassthrough();

    // 创建控制面板
    createControlPanel();

    // 确保窗口获得焦点
    activateWindow();
    raise();
}

void ImageWidget::disableCanvasMode()
{
    qDebug() << "禁用画布模式";
    canvasMode = false;

    // 禁用鼠标穿透
    disableMousePassthrough();
    setWindowOpacity(1.0);

    // 销毁控制面板
    destroyControlPanel();

    // 恢复正常的窗口属性
    loadConfiguration();
    update();
}

void ImageWidget::enableMousePassthrough()
{
#ifdef Q_OS_WIN
    SetWindowLongPtr((HWND)winId(), GWL_EXSTYLE,
                     GetWindowLongPtr((HWND)winId(), GWL_EXSTYLE) | WS_EX_TRANSPARENT);
#endif

#ifndef Q_OS_LINUX
#endif

    mousePassthrough = true;
}

// 在 #ifdef Q_OS_LINUX 块之外，添加一个通用的实现（适用于 Windows 或其他平台）

void ImageWidget::disableMousePassthrough()
{
    qDebug() << "禁用鼠标穿透 - Windows 或其他平台";

#ifdef Q_OS_WIN
    // Windows 专用逻辑
    SetWindowLongPtr((HWND)winId(), GWL_EXSTYLE,
                     GetWindowLongPtr((HWND)winId(), GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);

    // 清除透明鼠标事件属性
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
#endif

#ifndef Q_OS_LINUX
#endif
    mousePassthrough = false;
}


bool ImageWidget::isCanvasModeEnabled()
{
    return canvasMode;
}
