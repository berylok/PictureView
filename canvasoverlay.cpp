// canvasoverlay.cpp
#include "canvasoverlay.h"
#include "imagewidget.h"
#include "qapplication.h"
#include <QPainter>
#include <QTimer>
#include <QDebug>

#ifdef Q_OS_LINUX
#include <QGuiApplication>
extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
}
#endif

// canvasoverlay.cpp - 修改构造函数
CanvasOverlay::CanvasOverlay(ImageWidget* parent)
    : QWidget(nullptr), m_parentWidget(parent)
{
    // 窗口标志：Tool + 无边框 + 置顶
    setWindowFlags(Qt::Tool |
                   Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint);

    // 关键属性：鼠标完全穿透、显示时不激活
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);

    setWindowOpacity(0.7);
    qDebug() << "CanvasOverlay 窗口已创建（Tool类型，更稳定置顶）";
}

CanvasOverlay::~CanvasOverlay()
{
    qDebug() << "CanvasOverlay 窗口销毁";
}

void CanvasOverlay::setImage(const QPixmap& pixmap)
{
    m_displayPixmap = pixmap;
    update();
}

// canvasoverlay.cpp - 修改paintEvent，复制主窗口的绘制逻辑
void CanvasOverlay::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    qDebug() << "CanvasOverlay::paintEvent - 开始绘制";
    qDebug() << "  窗口尺寸:" << size();
    qDebug() << "  图片是否为空:" << m_displayState.image.isNull();
    qDebug() << "  图片尺寸:" << m_displayState.image.size();
    qDebug() << "  缩放比例:" << m_displayState.scaleFactor;
    qDebug() << "  平移偏移:" << m_displayState.panOffset;

    // 1. 填充背景（比主窗口稍暗）
    painter.fillRect(rect(), QColor(25, 25, 35, 180));

    // 2. 绘制网格背景
    painter.setPen(QPen(QColor(60, 60, 80, 50), 1));
    for (int x = 0; x < width(); x += 50) {
        painter.drawLine(x, 0, x, height());
    }
    for (int y = 0; y < height(); y += 50) {
        painter.drawLine(0, y, width(), y);
    }

    // 3. 如果有图片，按照主窗口的显示方式绘制
    if (!m_displayState.image.isNull()) {
        qDebug() << "按照主窗口方式绘制图片...";

        // 方法1：直接使用主窗口的计算结果
        if (m_displayState.imageRect.isValid()) {
            // 计算在画布窗口中的对应位置
            // 由于画布窗口和主窗口大小位置相同，可以直接使用
            QRect targetRect = m_displayState.imageRect;
            targetRect.translate(geometry().topLeft() - m_parentWidget->geometry().topLeft());

            qDebug() << "  使用主窗口显示区域:" << m_displayState.imageRect;
            qDebug() << "  画布中目标区域:" << targetRect;

            // 绘制图片（使用与主窗口相同的缩放）
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

            // 如果需要缩放，先缩放图片
            QImage scaledImage = m_displayState.image;
            if (m_displayState.scaleFactor != 1.0) {
                scaledImage = m_displayState.image.scaled(
                    m_displayState.image.size() * m_displayState.scaleFactor,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                    );
            }

            painter.drawImage(targetRect, scaledImage);

            // 绘制图片边框
            painter.setPen(QPen(QColor(200, 200, 255, 150), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(targetRect);

            // 绘制信息
            painter.setPen(QColor(180, 200, 255, 180));
            painter.setFont(QFont("Arial", 10));
            painter.drawText(targetRect.bottomRight() - QPoint(150, 5),
                             QString("缩放: %1%").arg(int(m_displayState.scaleFactor * 100)));
        }
        // 方法2：重新计算（如果方法1不可用）
        else {
            // 复制主窗口的缩放逻辑
            QSize scaledSize = m_displayState.image.size() * m_displayState.scaleFactor;
            QRect targetRect = QRect(QPoint(0, 0), scaledSize);
            targetRect.moveCenter(rect().center() + m_displayState.panOffset.toPoint());

            qDebug() << "  重新计算目标区域:" << targetRect;

            // 绘制图片
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

            QImage scaledImage = m_displayState.image;
            if (m_displayState.scaleFactor != 1.0) {
                scaledImage = m_displayState.image.scaled(
                    scaledSize,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                    );
            }

            painter.drawImage(targetRect, scaledImage);

            // 绘制边框和信息
            painter.setPen(QPen(QColor(200, 200, 255, 150), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(targetRect);
        }
    } else if (!m_displayPixmap.isNull()) {
        // 备用：使用QPixmap绘制
        qDebug() << "使用QPixmap绘制图片...";

        QSize scaledSize = m_displayPixmap.size() * m_displayState.scaleFactor;
        QRect targetRect = QRect(QPoint(0, 0), scaledSize);
        targetRect.moveCenter(rect().center() + m_displayState.panOffset.toPoint());

        painter.drawPixmap(targetRect, m_displayPixmap.scaled(
                                           scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // 4. 绘制窗口装饰
    // 标题区域
    QRect titleRect(0, 0, width(), 35);
    QLinearGradient titleGradient(0, 0, 0, titleRect.height());
    titleGradient.setColorAt(0, QColor(50, 60, 90, 220));
    titleGradient.setColorAt(1, QColor(40, 50, 80, 220));
    painter.fillRect(titleRect, titleGradient);

    // 标题文字
    painter.setPen(QColor(220, 230, 255, 230));
    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.drawText(titleRect.adjusted(10, 0, -10, 0), Qt::AlignVCenter, "画布模式 - 图片置顶显示");

    // 窗口边框
    painter.setPen(QPen(QColor(80, 120, 200, 200), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(1, 1, -2, -2));

    // 操作提示
    painter.setPen(QColor(180, 190, 220, 180));
    painter.setFont(QFont("Arial", 9));
    QString hint = QString("ESC退出 | 拖动调整位置 | Ctrl+滚轮缩放 (%1%)")
                       .arg(int(m_displayState.scaleFactor * 100));
    painter.drawText(10, height() - 10, hint);

    qDebug() << "CanvasOverlay::paintEvent - 绘制完成";
}

// canvasoverlay.cpp - 修改 setDisplayState 函数
void CanvasOverlay::setDisplayState(const DisplayState& state)
{
    m_displayState = state;

    // 如果图片有效，创建对应的QPixmap
    if (!state.image.isNull()) {
        m_displayPixmap = QPixmap::fromImage(state.image);
    }

    update();
}

void CanvasOverlay::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);


}

// canvasoverlay.cpp - 修改 applyX11MousePassthrough 函数
void CanvasOverlay::applyX11MousePassthrough()
{
#ifdef Q_OS_LINUX
    if (QGuiApplication::platformName().contains("xcb")) {
        qDebug() << "应用X11鼠标穿透到CanvasOverlay（只穿透非图片区域）";

        Display* display = XOpenDisplay(nullptr);
        if (!display) {
            qWarning() << "无法打开X11显示连接";
            return;
        }

        Window windowId = (Window)winId();
        qDebug() << "CanvasOverlay X11窗口ID:" << windowId;

        // 获取图片显示区域
        QRect imageRect = QRect(0, 0, 0, 0);
        if (!m_displayPixmap.isNull()) {
            imageRect = m_displayPixmap.rect();
            imageRect.moveCenter(rect().center());
            qDebug() << "图片显示区域:" << imageRect;
        }

        if (!imageRect.isEmpty()) {
            // 创建一个矩形列表：窗口区域减去图片区域
            QVector<XRectangle> rects;

            // 顶部区域
            if (imageRect.top() > 0) {
                XRectangle rect;
                rect.x = 0;
                rect.y = 0;
                rect.width = width();
                rect.height = imageRect.top();
                rects.append(rect);
            }

            // 底部区域
            if (imageRect.bottom() < height() - 1) {
                XRectangle rect;
                rect.x = 0;
                rect.y = imageRect.bottom() + 1;
                rect.width = width();
                rect.height = height() - imageRect.bottom() - 1;
                rects.append(rect);
            }

            // 左侧区域
            if (imageRect.left() > 0) {
                XRectangle rect;
                rect.x = 0;
                rect.y = imageRect.top();
                rect.width = imageRect.left();
                rect.height = imageRect.height();
                rects.append(rect);
            }

            // 右侧区域
            if (imageRect.right() < width() - 1) {
                XRectangle rect;
                rect.x = imageRect.right() + 1;
                rect.y = imageRect.top();
                rect.width = width() - imageRect.right() - 1;
                rect.height = imageRect.height();
                rects.append(rect);
            }

            if (rects.isEmpty()) {
                // 如果图片填满整个窗口，则完全穿透
                XShapeCombineRectangles(display, windowId, ShapeInput,
                                        0, 0, nullptr, 0, ShapeSet, YXBanded);
                qDebug() << "图片填满窗口，完全穿透";
            } else {
                // 只穿透非图片区域
                XShapeCombineRectangles(display, windowId, ShapeInput,
                                        0, 0, rects.data(), rects.size(), ShapeSet, YXBanded);
                qDebug() << "已穿透非图片区域，矩形数量:" << rects.size();
            }
        } else {
            // 没有图片，完全穿透
            XShapeCombineRectangles(display, windowId, ShapeInput,
                                    0, 0, nullptr, 0, ShapeSet, YXBanded);
            qDebug() << "无图片，完全穿透";
        }

        XFlush(display);
        XCloseDisplay(display);

        qDebug() << "CanvasOverlay X11穿透已应用（智能穿透）";
    } else {
        qDebug() << "非X11环境，CanvasOverlay仅依赖Qt穿透属性";
    }
#endif
}

void CanvasOverlay::forceStayOnTop()
{
#ifdef Q_OS_LINUX
    if (!QGuiApplication::platformName().contains("xcb"))
        return;

    Display *display = XOpenDisplay(nullptr);
    if (!display) return;

    Window window = (Window)winId();
    if (!window) {
        XCloseDisplay(display);
        return;
    }

    // 1. 获取 _NET_WM_STATE 原子
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom net_wm_state_above = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);

    // 2. 发送 ClientMessage 请求添加 _NET_WM_STATE_ABOVE
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = net_wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1;          // _NET_WM_STATE_ADD
    event.xclient.data.l[1] = net_wm_state_above;
    event.xclient.data.l[2] = 0;          // 第二个原子，未使用
    event.xclient.data.l[3] = 1;          // 源指示：应用程序
    event.xclient.data.l[4] = 0;

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);

    // 3. 同时提升窗口（传统方式）
    XRaiseWindow(display, window);
    XFlush(display);
    XCloseDisplay(display);

    qDebug() << "CanvasOverlay: 强制置顶成功 (_NET_WM_STATE_ABOVE)";
#endif

    // 4. 保留 Qt 置顶标志作为后备
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    show();
    raise();
    activateWindow();
}


void CanvasOverlay::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    // 失去焦点时，可能被覆盖，延迟一帧强制置顶
    QTimer::singleShot(0, this, &CanvasOverlay::forceStayOnTop);
}
