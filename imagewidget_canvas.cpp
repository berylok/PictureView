// imagewidget_canvas.cpp
#include "imagewidget.h"
#include "canvasoverlay.h"
#include "qpainter.h"
#include <QScreen>
#include <QGuiApplication>

#ifdef _WIN32

#include <windows.h>

#else

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <QGuiApplication>
// **关键修复：包含必要的X11头文件**
extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
}

#endif



void ImageWidget::createControlPanel()
{
    if (!controlPanel) {
        // 注意：如果覆盖层存在，将控制面板作为覆盖层的子窗口
        QWidget* parentForPanel = canvasOverlay ? static_cast<QWidget*>(canvasOverlay) : static_cast<QWidget*>(this);

        controlPanel = new CanvasControlPanel(parentForPanel);
        connect(controlPanel, &CanvasControlPanel::exitCanvasMode, this,
                &ImageWidget::onExitCanvasMode);
        positionControlPanel();
        controlPanel->show();

        // 确保控制面板不会被穿透
        controlPanel->setAttribute(Qt::WA_TransparentForMouseEvents, false);

        qDebug() << "控制面板已创建，父窗口:" << (canvasOverlay ? "覆盖层" : "主窗口");
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
    qDebug() << "启用画布模式（使用覆盖层）";


    if (canvasMode) return;

    canvasMode = true;

    // 1. 创建 CanvasOverlay 的 DisplayState
    CanvasOverlay::DisplayState displayState;

    // 优先使用 pixmap，如果 pixmap 为空，尝试使用 currentImage
    if (!pixmap.isNull()) {
        displayState.image = pixmap.toImage();  // 将 QPixmap 转换为 QImage
        qDebug() << "使用 pixmap 创建 DisplayState，尺寸:" << pixmap.size();
    } else if (!currentImage.isNull()) {
        displayState.image = currentImage;
        qDebug() << "使用 currentImage 创建 DisplayState，尺寸:" << currentImage.size();
    } else {
        qWarning() << "pixmap 和 currentImage 都为空，无法显示图片";
    }

    displayState.scaleFactor = scaleFactor;
    displayState.panOffset = panOffset;

    // 计算图片显示区域
    if (!displayState.image.isNull()) {
        QSize scaledSize = displayState.image.size() * scaleFactor;
        QRect imageRect = QRect(QPoint(0, 0), scaledSize);
        imageRect.moveCenter(rect().center() + panOffset.toPoint());
        displayState.imageRect = imageRect;
        qDebug() << "计算出的图片显示区域:" << imageRect;
    }

    // 2. 创建并配置覆盖层窗口
    canvasOverlay = new CanvasOverlay(this);
    canvasOverlay->setGeometry(geometry());
    canvasOverlay->setDisplayState(displayState);


    // // 新增：根据主窗口当前透明度调整覆盖层透明度
    // double currentMainOpacity = windowOpacity();  // 获取主窗口当前透明度
    // double targetOpacity = (currentMainOpacity > 0.7) ? 0.7 : currentMainOpacity;
    // canvasOverlay->setWindowOpacity(targetOpacity);
    // qDebug() << "设置覆盖层透明度为:" << targetOpacity;

    // 3. 隐藏原窗口
    hide();

    // 4. 显示覆盖层
    canvasOverlay->show();

    // 强制覆盖层置顶并激活（确保在最前）
    canvasOverlay->raise();
    canvasOverlay->activateWindow();

    // 为覆盖层窗口启用完全鼠标穿透
    QTimer::singleShot(50, [this]() {
        if (canvasOverlay) {
            canvasOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
#ifdef Q_OS_LINUX
            if (QGuiApplication::platformName().contains("xcb")) {
                Display *display = XOpenDisplay(nullptr);
                if (display) {
                    Window windowId = (Window)canvasOverlay->winId();
                    if (windowId) {
                        // 完全穿透：清除所有输入区域
                        XShapeCombineRectangles(display, windowId, ShapeInput,
                                                0, 0, nullptr, 0, ShapeSet, YXBanded);
                        XFlush(display);
                        qDebug() << "CanvasOverlay 完全鼠标穿透已设置";
                    }
                    XCloseDisplay(display);
                }
            }
#endif
        }
    });

    // 5. 创建控制面板
    createControlPanel();

    // 6. 确保控制面板在最前面
    if (controlPanel) {
        controlPanel->raise();
        controlPanel->activateWindow();
    }

    qDebug() << "画布模式已启用（覆盖层方案）";
}

void ImageWidget::disableCanvasMode()
{
    qDebug() << "禁用画布模式（覆盖层方案）";

    if (!canvasMode) return;

    canvasMode = false;

    // 1. 销毁控制面板
    destroyControlPanel();

    // 2. 销毁覆盖层窗口
    if (canvasOverlay) {
        canvasOverlay->hide();
        canvasOverlay->deleteLater();
        canvasOverlay = nullptr;
    }

    // 3. 恢复原窗口显示
    show();
    activateWindow();
    raise();

    // 4. 恢复窗口属性
    setWindowOpacity(1.0);

    qDebug() << "画布模式已禁用";
}

void ImageWidget::enableMousePassthrough()
{
    qDebug() << "启用鼠标穿透 - Qt6 Linux/X11";
    setAttribute(Qt::WA_TransparentForMouseEvents, true);


#ifdef Q_OS_WIN
    SetWindowLongPtr((HWND)winId(), GWL_EXSTYLE,
                     GetWindowLongPtr((HWND)winId(), GWL_EXSTYLE) | WS_EX_TRANSPARENT);
#elif defined(Q_OS_LINUX)
    qDebug() << "检测到Linux平台，Qt平台:" << QGuiApplication::platformName();

    if (QGuiApplication::platformName().contains("xcb")) {
        qDebug() << "✓ 确认为X11环境";

        // **尝试使用XOpenDisplay（传统方式）**
        qDebug() << "尝试使用XOpenDisplay打开显示连接...";
        Display *display = XOpenDisplay(nullptr);

        if (display) {
            qDebug() << "✓ XOpenDisplay成功";

            // **检查XShape扩展是否可用**
            int event_base, error_base;
            int has_shape = XShapeQueryExtension(display, &event_base, &error_base);
            qDebug() << "XShape扩展支持:" << (has_shape ? "可用" : "不可用");

            if (has_shape) {
                Window windowId = (Window)winId();
                qDebug() << "窗口ID:" << windowId;

                // **检查窗口是否已映射（显示在屏幕上）**
                XWindowAttributes attrs;
                if (XGetWindowAttributes(display, windowId, &attrs)) {
                    qDebug() << "窗口映射状态:" << (attrs.map_state == IsViewable ? "已映射" : "未映射");
                    qDebug() << "窗口尺寸:" << attrs.width << "x" << attrs.height;
                }

                // **执行鼠标穿透**
                qDebug() << "执行XShapeCombineRectangles...";
                XShapeCombineRectangles(display, windowId, ShapeInput,
                                        0, 0, nullptr, 0, ShapeSet, YXBanded);
                XFlush(display);
                qDebug() << "✓ X11鼠标穿透设置完成";

                // **注意：这里不能关闭display，需要在disableMousePassthrough中处理**
            } else {
                qWarning() << "✗ X服务器不支持XShape扩展，无法实现高级鼠标穿透";
            }

            // **同时也尝试Qt原生接口（作为备选）**
            qDebug() << "尝试Qt原生接口...";
            if (auto *x11App = qApp->nativeInterface<QNativeInterface::QX11Application>()) {
                Display *qtDisplay = x11App->display();
                qDebug() << "Qt原生接口获取的Display:" << (qtDisplay ? "有效" : "无效");
            }

        } else {
            qCritical() << "✗ XOpenDisplay失败!";
            qDebug() << "可能原因:";
            qDebug() << "  1. 不在图形环境中运行";
            qDebug() << "  2. 没有正确的DISPLAY设置";
        }
    } else {
        qDebug() << "非X11环境，仅使用Qt标准穿透属性";
    }
#endif

    // **验证Qt属性是否生效**
    bool isTransparent = testAttribute(Qt::WA_TransparentForMouseEvents);
    qDebug() << "Qt属性WA_TransparentForMouseEvents:"
             << (isTransparent ? "已设置" : "未设置");

    mousePassthrough = true;
    qDebug() << "=== 启用鼠标穿透函数结束 ===";
}

// 在 #ifdef Q_OS_LINUX 块之外，添加一个通用的实现（适用于 Windows 或其他平台）
void ImageWidget::disableMousePassthrough()
{
    qDebug() << "禁用鼠标穿透 - 开始";

    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    qDebug() << "  Qt 穿透属性已清除";

    clearX11Shape();   // 彻底清除 X11 形状

    mousePassthrough = false;
    qDebug() << "禁用鼠标穿透 - 完成";
}


bool ImageWidget::isCanvasModeEnabled()
{
    return canvasMode;
}

void ImageWidget::forceX11ShapeRefresh()
{
#ifdef Q_OS_LINUX
    if (!QGuiApplication::platformName().contains("xcb")) return;
    Display *display = XOpenDisplay(nullptr);
    if (!display) return;
    Window windowId = (Window)winId();
    if (!windowId) {
        XCloseDisplay(display);
        return;
    }

    // 发送一个 ConfigureNotify 事件，模拟窗口配置变化（但大小不变）
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ConfigureNotify;
    event.xconfigure.window = windowId;
    event.xconfigure.width = width();
    event.xconfigure.height = height();
    event.xconfigure.border_width = 0;
    event.xconfigure.above = None;
    event.xconfigure.override_redirect = False;
    XSendEvent(display, windowId, False, StructureNotifyMask, &event);
    XFlush(display);
    XCloseDisplay(display);

    qDebug() << "X11 刷新事件已发送";
#endif
}
