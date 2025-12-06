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
    QString hint = QString("ESC退出 | 拖动调整位置 | Ctrl+滚轮缩放 (%1%) | 透明度: %2%")
                       .arg(int(m_displayState.scaleFactor * 100))
                       .arg(int(m_windowOpacity * 100));
    painter.drawText(10, height() - 10, hint);

    qDebug() << "CanvasOverlay::paintEvent - 绘制完成";
}

// 修改 setDisplayState 函数以处理透明度
void CanvasOverlay::setDisplayState(const DisplayState& state)
{
    m_displayState = state;

    // 如果状态中包含透明度，则更新窗口透明度
    if (state.windowOpacity > 0) {
        setWindowOpacityValue(state.windowOpacity);
    }

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
