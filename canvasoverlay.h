// canvasoverlay.h
#ifndef CANVASOVERLAY_H
#define CANVASOVERLAY_H

#include <QWidget>
#include <QImage>
#include <QPixmap>

class ImageWidget;

class CanvasOverlay : public QWidget
{
    Q_OBJECT

public:
    // 修改构造函数，添加透明度参数
    explicit CanvasOverlay(ImageWidget* parent, qreal opacity = 1.0);
    ~CanvasOverlay();

    struct DisplayState {
        QImage image;
        qreal scaleFactor = 1.0;
        QPointF panOffset;
        QRect imageRect;
        qreal windowOpacity = 1.0;  // 添加透明度成员
    };

    void setImage(const QPixmap& pixmap);
    void setDisplayState(const DisplayState& state);
    void setWindowOpacityValue(qreal opacity);  // 添加设置透明度的方法

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void applyX11MousePassthrough();

    ImageWidget* m_parentWidget;
    QPixmap m_displayPixmap;
    DisplayState m_displayState;
    qreal m_windowOpacity = 1.0;  // 私有成员变量
};

#endif // CANVASOVERLAY_H
