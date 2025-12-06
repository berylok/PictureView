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
}
#endif

// canvasoverlay.cpp - 修改构造函数，添加opacity参数
CanvasOverlay::CanvasOverlay(ImageWidget* parent, qreal opacity)
    : QWidget(nullptr), // 注意：父对象为nullptr，使其成为独立顶级窗口
    m_parentWidget(parent),
    m_displayPixmap(),
    m_windowOpacity(opacity)  // 使用传入的透明度值初始化成员变量
{
    // === 关键设置 1：窗口标志 ===
    // === 1. 窗口标志：确保置顶且无边框 ===
    setWindowFlags(Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint |
                   Qt::Tool |  // 重要：工具窗口有特殊置顶行为
                   Qt::WindowDoesNotAcceptFocus); // 不接受焦点，减少被抢焦点

    // === 2. 彻底透明背景 ===
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false); // 允许透明绘制
    setAutoFillBackground(false);

    // === 3. 样式：完全透明 ===
    setStyleSheet("background-color: transparent; border: none;");

    // === 4. 初始透明（窗口自身） ===
    setWindowOpacity(m_windowOpacity);  // 使用成员变量设置窗口透明度

    // === 5. 鼠标穿透相关 ===
    setAttribute(Qt::WA_TransparentForMouseEvents, false); // 稍后由X11处理
    setFocusPolicy(Qt::NoFocus);

// === 6. X11特定设置 ===
#ifdef Q_OS_LINUX
    // 这些属性有助于X11环境下的置顶
    setAttribute(Qt::WA_X11NetWmWindowTypeDesktop, false);
    setAttribute(Qt::WA_X11NetWmWindowTypeDock, false);
#endif

    // 记录窗口ID
    qDebug() << "CanvasOverlay 窗口ID:" << winId() << "透明度:" << m_windowOpacity;
}

CanvasOverlay::~CanvasOverlay()
{
    qDebug() << "CanvasOverlay 窗口销毁";
}

// 添加设置透明度的方法实现
void CanvasOverlay::setWindowOpacityValue(qreal opacity)
{
    m_windowOpacity = qBound(0.1, opacity, 1.0);  // 限制在0.1到1.0之间
    setWindowOpacity(m_windowOpacity);
    update();
    qDebug() << "CanvasOverlay 透明度设置为:" << m_windowOpacity;
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
    qDebug() << "  窗口透明度:" << m_windowOpacity;

    //  如果有图片，按照主窗口的显示方式绘制
    if (!m_displayState.image.isNull()) {
        qDebug() << "按照主窗口方式绘制图片...";

        // 方法1：直接使用主窗口的计算结果
        if (m_displayState.imageRect.isValid()) {
            // 计算在画布窗口中的对应位置
            // 由于画布窗口和主窗口大小位置相同，可以直接使用
            QRect targetRect = m_displayState.imageRect;

            qDebug() << "  使用主窗口显示区域:" << m_displayState.imageRect;
            qDebug() << "  画布中目标区域:" << targetRect;

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

    qDebug() << "CanvasOverlay::paintEvent - 绘制完成";
}

// 修改 setDisplayState 函数以处理透明度
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

    qDebug() << "CanvasOverlay 显示事件";
    qDebug() << "  窗口位置:" << geometry();
    qDebug() << "  窗口是否可见:" << isVisible();
    qDebug() << "  窗口是否最小化:" << isMinimized();
    qDebug() << "  窗口是否最大化:" << isMaximized();
    qDebug() << "  窗口透明度:" << m_windowOpacity;

    // 确保窗口获得焦点
    activateWindow();
    raise();

    // 确保窗口在前台
    QApplication::processEvents();

    // 延迟应用X11穿透，确保窗口稳定
    QTimer::singleShot(100, this, &CanvasOverlay::applyX11MousePassthrough);

    // 添加一个检查定时器，确保窗口持续显示
    QTimer::singleShot(500, this, [this]() {
        if (isVisible()) {
            qDebug() << "CanvasOverlay 窗口仍然可见";
        } else {
            qWarning() << "CanvasOverlay 窗口已隐藏！";
            // 尝试重新显示
            show();
            activateWindow();
            raise();
        }
    });
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
