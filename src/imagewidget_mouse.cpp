
// imagewidget_mouse.cpp
#include "imagewidget.h"
#include <QMouseEvent>
#include <QMenu>

#ifdef Q_OS_LINUX
#endif

#include <QWindow>
#include <QApplication>
#include <QEventLoop>

void ImageWidget::mousePressEvent(QMouseEvent *event)
{
    // 在画布模式下，只响应右键点击显示菜单
    if (canvasMode) {
        if (event->button() == Qt::RightButton) {
            // 临时禁用鼠标穿透以显示菜单
            disableMousePassthrough();
            showContextMenu(event->globalPosition().toPoint());
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
            showContextMenu(event->globalPosition().toPoint());
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
                dragStartPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
                return;
            }
            if (event->button() == Qt::RightButton) {
                showContextMenu(event->globalPosition().toPoint());
                return;
            }

            if (event->button() == Qt::LeftButton) {
                isPanningImage = true;
                panStartPosition = event->pos();

                if (isWayland()) {
                    QWidget::clearMask();
                    // ★ 立即提交一次，确保 mask 真的清掉了
                    if (windowHandle()) {
                        windowHandle()->requestUpdate();
                    }
                }
            }
        }
    }
}

void ImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isDraggingWindow && (event->buttons() & Qt::MiddleButton)) {
        QPoint newPosition = event->globalPosition().toPoint() - dragStartPosition;
        move(newPosition);
    } else if (isPanningImage && (event->buttons() & Qt::LeftButton)) {
        currentViewStateType = ManualAdjustment;

        QPointF delta = event->pos() - panStartPosition;
        panOffset += delta;
        panStartPosition = event->pos();

        update();              // 全窗口重绘（mask 已清）
        m_maskDirty = true;    // 保留，松手时用
    }
}

void ImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        isDraggingWindow = false;
    } else if (event->button() == Qt::LeftButton) {
        isPanningImage = false;

        m_dragCache = QPixmap();
        m_dragCacheScale = -1.0;

        if (isWayland()) {
            // 第一次：立即设（99% 情况生效）
            updateMask();
            if (windowHandle()) {
                windowHandle()->requestUpdate();
            }
            update();

            // ★ 兜底：50ms 后再设一次（修复合成器延迟的 1%）
            QTimer::singleShot(50, this, [this]() {
                if (isPanningImage) return;
                if (currentViewMode != SingleView) return;
                if (!testAttribute(Qt::WA_TranslucentBackground)) return;
                if (pixmap.isNull()) return;

                updateMask();
                if (windowHandle()) {
                    windowHandle()->requestUpdate();
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                }
            });
        } else {
            update();
            if (testAttribute(Qt::WA_TranslucentBackground) && !pixmap.isNull()) {
                m_maskDirty = true;
            }
        }
    }

    if (m_maskDirty && !isWayland()) {
        updateMask();
    }
}

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

