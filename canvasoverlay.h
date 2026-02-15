// canvasoverlay.h（修改版本）
#ifndef CANVASOVERLAY_H
#define CANVASOVERLAY_H

#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QPointF>

// 前向声明
class ImageWidget;

class CanvasOverlay : public QWidget
{
    Q_OBJECT

public:
    // 定义 DisplayState 结构体
    struct DisplayState {
        QImage image;
        double scaleFactor = 1.0;
        QPointF panOffset;
        QRect imageRect;

        DisplayState() : scaleFactor(1.0) {}
    };

    explicit CanvasOverlay(ImageWidget* parent = nullptr);
    ~CanvasOverlay();

    void setImage(const QPixmap& pixmap);
    void setDisplayState(const DisplayState& state);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void applyX11MousePassthrough();
    QRect calculateImageRect() const;

    // 成员变量
    QPixmap m_displayPixmap;
    ImageWidget* m_parentWidget;
    DisplayState m_displayState;  // 使用自己的 DisplayState

    // 拖动相关
    bool m_isDragging;
    QPoint m_dragStartPos;
    QPoint m_imageOffset;
    double m_scaleFactor;
    void ensureOnTop();
public:
    void forceStayOnTop();  // 强制窗口置顶（X11原生）
protected:
    void focusOutEvent(QFocusEvent* event) override;

};

#endif // CANVASOVERLAY_H
