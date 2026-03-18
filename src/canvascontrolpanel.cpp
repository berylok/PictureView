// canvascontrolpanel.cpp
#include "canvascontrolpanel.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QCursor>

CanvasControlPanel::CanvasControlPanel(QWidget *parent)
    : QWidget(parent),
    mouseOver(false),
    buttonHover(false),
    isDragging(false)
{
    // 设置窗口属性
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(120, 40);

    // 启用鼠标跟踪
    setMouseTracking(true);
}

void CanvasControlPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 计算按钮区域
    buttonRect = QRect(10, 5, width() - 20, height() - 10);

    // 绘制背景
    if (mouseOver) {
        painter.setBrush(QColor(60, 60, 60, 220));
    } else {
        painter.setBrush(QColor(50, 50, 50, 180));
    }
    painter.setPen(QPen(QColor(100, 100, 100, 150), 1));
    painter.drawRoundedRect(rect(), 5, 5);

    // 绘制按钮
    if (buttonHover) {
        painter.setBrush(QColor(220, 80, 80, 255)); // 红色悬停
    } else {
        painter.setBrush(QColor(180, 60, 60, 255)); // 红色正常
    }
    painter.setPen(QPen(QColor(255, 255, 255, 150), 1));
    painter.drawRoundedRect(buttonRect, 3, 3);

    // 绘制按钮文字
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 9, QFont::Bold));
    painter.drawText(buttonRect, Qt::AlignCenter, tr("退出画布"));
}

void CanvasControlPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (buttonRect.contains(event->pos())) {
            // 点击按钮，退出画布模式
            emit exitCanvasMode();
        } else {
            // 点击非按钮区域，开始拖拽
            isDragging = true;
            dragStartPosition =
                event->globalPosition().toPoint() - frameGeometry().topLeft();
            setCursor(Qt::ClosedHandCursor);
        }
    }
}

void CanvasControlPanel::mouseMoveEvent(QMouseEvent *event)
{
    // 更新按钮悬停状态
    bool oldHover = buttonHover;
    buttonHover = buttonRect.contains(event->pos());

    if (oldHover != buttonHover) {
        update(); // 重绘以更新按钮颜色
    }

    // 处理拖拽
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        QPoint newPosition = event->globalPosition().toPoint() - dragStartPosition;
        move(newPosition);
    }
}

void CanvasControlPanel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void CanvasControlPanel::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    mouseOver = true;
    update(); // 重绘以更新背景颜色
}

void CanvasControlPanel::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    mouseOver = false;
    buttonHover = false;
    update(); // 重绘以更新背景和按钮颜色
}
