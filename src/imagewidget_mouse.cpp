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
        // 单张模式
        if (currentViewMode == SingleView) {
            if (event->button() == Qt::MiddleButton) {
                isDraggingWindow = true;
                dragStartPosition = event->globalPos() - frameGeometry().topLeft();
                return;
            }
            if (event->button() == Qt::RightButton) {
                showContextMenu(event->globalPos());
                return;
            }
            if (event->button() == Qt::LeftButton) {
                // ---- 原有的左键处理（可保留或精简） ----
                if (!pixmap.isNull()) {
                    QSizeF scaledSize = QSizeF(pixmap.size()) * scaleFactor;
                    QPointF offset((width() - scaledSize.width()) / 2.0 + panOffset.x(),
                                   (height() - scaledSize.height()) / 2.0 + panOffset.y());
                    QRectF imageRect(offset, scaledSize);

                    if (imageRect.contains(event->pos())) {
                        // 如果仍想保留图片边缘的点击切换，可以保留下面的判断
                        // 但在箭头优先的情况下，这里的左右 1/9 可能永远不会被触发
                        // 建议移除或注释掉，避免混乱
                        QPointF relativePos = event->pos() - offset;
                        /*
                    if (relativePos.x() < scaledSize.width() / 9) {
                        loadPreviousImage();
                        showNavigationHints = false;
                        return;
                    } else if (relativePos.x() > scaledSize.width() * 8 / 9) {
                        loadNextImage();
                        showNavigationHints = false;
                        return;
                    }
                    */
                        // 否则进入拖拽平移
                        isPanningImage = true;
                        panStartPosition = event->pos();
                        return;
                    }
                }

                // 不在图片区域内 → 也允许拖拽平移（保留）
                isPanningImage = true;
                panStartPosition = event->pos();
            }
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
        updateMask();
    }
}

void ImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        isDraggingWindow = false;
    } else if (event->button() == Qt::LeftButton) {
        isPanningImage = false;
        // 拖拽结束，标记掩码需要更新（不再立即调用 updateMask）
        if (testAttribute(Qt::WA_TranslucentBackground) && !pixmap.isNull()) {
            m_maskDirty = true;
        }
    }

    // 任意鼠标释放后，统一执行一次掩码更新（包括滚轮停止后以及左键拖拽结束）
    if (m_maskDirty) {
        updateMask();
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

