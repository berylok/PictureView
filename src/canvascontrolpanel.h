// canvascontrolpanel.h
#ifndef CANVASCONTROLPANEL_H
#define CANVASCONTROLPANEL_H

#include <QWidget>
#include <QPoint>

class CanvasControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CanvasControlPanel(QWidget *parent = nullptr);

signals:
    void exitCanvasMode();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool mouseOver;
    bool buttonHover;
    bool isDragging;
    QPoint dragStartPosition;
    QRect buttonRect;
};

#endif // CANVASCONTROLPANEL_H
