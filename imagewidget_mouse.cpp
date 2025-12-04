// imagewidget_mouse.cpp
#include "imagewidget.h"
#include <QMouseEvent>
#include <QMenu>

#ifdef Q_OS_LINUX
#endif

void ImageWidget::mousePressEvent(QMouseEvent *event)
{
    // 在画布模式下，只响应右键点击显示菜单
    if (canvasMode) {
        if (event->button() == Qt::RightButton) {
            // 临时禁用鼠标穿透以显示菜单
            disableMousePassthrough();
            showContextMenu(event->globalPos());
            // 菜单关闭后重新启用鼠标穿透
            if (canvasMode) {
                enableMousePassthrough();
            }
        }
        return; // 忽略其他所有鼠标事件
    }

    if (currentViewMode == ThumbnailView) {
        // 在缩略图模式下，只处理右键点击（显示菜单）
        if (event->button() == Qt::RightButton) {
            showContextMenu(event->globalPos());
        }
        // 左键和中键事件交给 ThumbnailWidget 处理
        else {
            QWidget::mousePressEvent(event);
        }
    } else {
        // 单张模式下的处理
        if (event->button() == Qt::MiddleButton) {
            isDraggingWindow = true;
            dragStartPosition = event->globalPos() - frameGeometry().topLeft();
        } else if (event->button() == Qt::RightButton) {
            showContextMenu(event->globalPos());
        } else if (event->button() == Qt::LeftButton) {
            // 检查是否在图片区域内
            if (!pixmap.isNull()) {
                QSize scaledSize = pixmap.size() * scaleFactor;
                QPointF offset((width() - scaledSize.width()) / 2 + panOffset.x(),
                               (height() - scaledSize.height()) / 2 + panOffset.y());
                QRectF imageRect(offset, scaledSize);

                if (imageRect.contains(event->pos())) {
                    // 在图片区域内，检查是左四分之一还是右四分之一
                    QPointF relativePos = event->pos() - offset;
                    if (relativePos.x() < scaledSize.width() / 9) {
                        // 点击左四分之一，切换到上一张
                        loadPreviousImage();
                        // 标记为键盘操作，不显示导航提示
                        showNavigationHints = false;
                    } else if (relativePos.x() > scaledSize.width() * 8 / 9) {
                        // 点击右四分之一，切换到下一张
                        loadNextImage();
                        // 标记为键盘操作，不显示导航提示
                        showNavigationHints = false;
                    } else {
                        // 点击中间一半区域，进行拖拽
                        isPanningImage = true;
                        panStartPosition = event->pos();
                    }
                    return; // 处理完毕
                }
            }

            // 不在图片区域内或图片为空，进行拖拽
            isPanningImage = true;
            panStartPosition = event->pos();
        }
    }
}

void ImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isDraggingWindow && (event->buttons() & Qt::MiddleButton)) {
        QPoint newPosition = event->globalPos() - dragStartPosition;
        move(newPosition);
    } else if (isPanningImage && (event->buttons() & Qt::LeftButton)) {
        // 标记为手动调整
        currentViewStateType = ManualAdjustment;

        QPointF delta = event->pos() - panStartPosition;
        panOffset += delta;
        panStartPosition = event->pos();
        update();
    }
}

void ImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        isDraggingWindow = false;
    } else if (event->button() == Qt::LeftButton) {
        isPanningImage = false;
    }
}

// void ImageWidget::mouseDoubleClickEvent(QMouseEvent *event)
// {
//     if (event->button() == Qt::LeftButton) {
//         if (currentViewMode == ThumbnailView &&
//             thumbnailWidget->getSelectedIndex() >= 0) {
//             switchToSingleView(thumbnailWidget->getSelectedIndex());
//         } else if (currentViewMode == SingleView) {
//             // 检查是否在图片区域内，并且不在切换区域
//             if (!pixmap.isNull()) {
//                 QSize scaledSize = pixmap.size() * scaleFactor;
//                 QPointF offset((width() - scaledSize.width()) / 2 + panOffset.x(),
//                                (height() - scaledSize.height()) / 2 + panOffset.y());
//                 QRectF imageRect(offset, scaledSize);

//                 if (imageRect.contains(event->pos())) {
//                     QPointF relativePos = event->pos() - offset;
//                     // 只有在中间一半区域才响应双击
//                     if (relativePos.x() >= scaledSize.width() / 4 &&
//                         relativePos.x() <= scaledSize.width() * 3 / 4) {
//                         switchToThumbnailView();
//                     }
//                     // 在切换区域的双击不执行任何操作
//                 } else {
//                     // 不在图片区域内，双击返回缩略图
//                     switchToThumbnailView();
//                 }
//             } else {
//                 // 没有图片，双击返回缩略图
//                 switchToThumbnailView();
//             }
//         }
//     }
// }


// 在 mouseDoubleClickEvent 中确保正确处理画布模式
void ImageWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    // 在画布模式下，完全忽略所有鼠标事件
    if (canvasMode) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (currentViewMode == ThumbnailView &&
            thumbnailWidget->getSelectedIndex() >= 0) {
            switchToSingleView(thumbnailWidget->getSelectedIndex());
        } else if (currentViewMode == SingleView) {
            // 检查是否在图片区域内，并且不在切换区域
            if (!pixmap.isNull()) {
                QSize scaledSize = pixmap.size() * scaleFactor;
                QPointF offset((width() - scaledSize.width()) / 2 + panOffset.x(),
                               (height() - scaledSize.height()) / 2 + panOffset.y());
                QRectF imageRect(offset, scaledSize);

                if (imageRect.contains(event->pos())) {
                    QPointF relativePos = event->pos() - offset;
                    // 只有在中间一半区域才响应双击
                    if (relativePos.x() >= scaledSize.width() / 4 &&
                        relativePos.x() <= scaledSize.width() * 3 / 4) {
                        switchToThumbnailView();
                    }
                    // 在切换区域的双击不执行任何操作
                } else {
                    // 不在图片区域内，双击返回缩略图
                    switchToThumbnailView();
                }
            } else {
                // 没有图片，双击返回缩略图
                switchToThumbnailView();
            }
        }
    }
}
