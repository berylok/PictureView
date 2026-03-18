// imagewidget_transform.cpp
#include "imagewidget.h"
#include <QTransform>

void ImageWidget::mirrorHorizontal()
{
    if (pixmap.isNull()) return;

    isHorizontallyFlipped = !isHorizontallyFlipped;
    applyTransformations();
}

void ImageWidget::mirrorVertical()
{
    if (pixmap.isNull()) return;

    isVerticallyFlipped = !isVerticallyFlipped;
    applyTransformations();
}

void ImageWidget::rotate90CW()
{
    if (pixmap.isNull()) return;

    rotationAngle = (rotationAngle + 90) % 360;
    applyTransformations();
    fitToWindow();
}

void ImageWidget::rotate90CCW()
{
    if (pixmap.isNull()) return;

    rotationAngle = (rotationAngle - 90 + 360) % 360;
    applyTransformations();
    fitToWindow();
}

void ImageWidget::rotate180()
{
    if (pixmap.isNull()) return;

    rotationAngle = (rotationAngle + 180) % 360;
    applyTransformations();
}

void ImageWidget::applyTransformations()
{
    if (originalPixmap.isNull()) return;

    QTransform transform;

    // 应用镜像变换
    if (isHorizontallyFlipped || isVerticallyFlipped) {
        qreal scaleX = isHorizontallyFlipped ? -1.0 : 1.0;
        qreal scaleY = isVerticallyFlipped ? -1.0 : 1.0;
        transform.scale(scaleX, scaleY);

        // 调整位置，确保镜像后图片仍然可见
        if (isHorizontallyFlipped) {
            transform.translate(-originalPixmap.width(), 0);
        }
        if (isVerticallyFlipped) {
            transform.translate(0, -originalPixmap.height());
        }
    }

    // 应用旋转变换
    if (rotationAngle != 0) {
        // 移动到中心点旋转
        transform.translate(originalPixmap.width() / 2.0,
                            originalPixmap.height() / 2.0);
        transform.rotate(rotationAngle);
        transform.translate(-originalPixmap.width() / 2.0,
                            -originalPixmap.height() / 2.0);
    }

    // 应用变换
    pixmap = originalPixmap.transformed(transform, Qt::SmoothTransformation);

    // 重置平移偏移
    panOffset = QPointF(0, 0);

    update();
}

void ImageWidget::resetTransform()
{
    if (originalPixmap.isNull()) return;

    // 重置所有变换状态
    rotationAngle = 0;
    isHorizontallyFlipped = false;
    isVerticallyFlipped = false;

    // 恢复原始图片
    pixmap = originalPixmap;
    fitToWindow();
    update();
}

bool ImageWidget::isTransformed() const
{
    return rotationAngle != 0 || isHorizontallyFlipped || isVerticallyFlipped;
}
