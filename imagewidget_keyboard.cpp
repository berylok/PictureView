// imagewidget_keyboard.cpp
#include "imagewidget.h"
#include <QKeyEvent>
#include <QScrollBar>
#include <QDebug>

void ImageWidget::keyPressEvent(QKeyEvent *event)
{
    qDebug() << "key:" << event->key();

    // 在压缩包模式下，ESC 键退出压缩包
    if (isArchiveMode && event->key() == Qt::Key_Escape) {
        exitArchiveMode();
        event->accept();
        return;
    }

    // 检查快捷键组合
    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
        case Qt::Key_O:
            if (event->modifiers() & Qt::ShiftModifier) {
                // Ctrl+Shift+O: 打开图片
                openImage();
                event->accept();
                return;
            } else {
                // Ctrl+O: 打开文件夹
                openFolder();
                event->accept();
                return;
            }
            break;
        case Qt::Key_S:
            // Ctrl+S: 保存图片
            if (currentViewMode == SingleView && !pixmap.isNull()) {
                saveImage();
                event->accept();
                return;
            }
            break;
        case Qt::Key_C:
            // Ctrl+C: 拷贝图片到剪贴板
            if (currentViewMode == SingleView && !pixmap.isNull()) {
                copyImageToClipboard();
                event->accept();
                return;
            }
            break;
        case Qt::Key_V:
            // Ctrl+V: 从剪贴板粘贴图片
            pasteImageFromClipboard();
            event->accept();
            return;
            break;
        }
    }

    // 单独按键处理
    switch (event->key()) {
    case Qt::Key_F1:
        // F1: 显示关于窗口
        showAboutDialog();
        event->accept();
        return;
        break;
    }

    // 在画布模式下，只响应特定的退出键
    if (canvasMode) {
        switch (event->key()) {
        case Qt::Key_Escape:
            // ESC键退出画布模式
            disableCanvasMode();
            break;
        case Qt::Key_Menu:
            // 菜单键显示上下文菜单
            showContextMenu(this->mapToGlobal(QPoint(width()/2, height()/2)));
            break;
        default:
            // 忽略其他所有按键
            event->ignore();
            break;
        }
        return;
    }

    if (currentViewMode == SingleView) {
        // 单张图片模式按键处理
        switch (event->key()) {
        case Qt::Key_Left:
            loadPreviousImage();
            break;
        case Qt::Key_Right:
            loadNextImage();
            break;
        case Qt::Key_Up:
            fitToWindow();
            break;
        case Qt::Key_Down:
            actualSize();
            break;
        case Qt::Key_Home:
            if (!imageList.isEmpty()) {
                loadImageByIndex(0);
            }
            break;
        case Qt::Key_End:
            if (!imageList.isEmpty()) {
                loadImageByIndex(imageList.size() - 1);
            }
            break;
        case Qt::Key_PageUp:
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+PageUp: 逆时针旋转90度
                rotate90CCW();
                event->accept();
            } else if (event->modifiers() & Qt::ShiftModifier) {
                // Shift+PageUp: 垂直镜像
                mirrorVertical();
                event->accept();
            } else {
                // PageUp：增加透明度（变得更不透明）
                double currentOpacity = windowOpacity();
                double newOpacity = qMin(1.0, currentOpacity + 0.1);
                setWindowOpacity(newOpacity);
                event->accept();
            }
            break;
        case Qt::Key_PageDown:
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+PageDown: 顺时针旋转90度
                rotate90CW();
                event->accept();
            } else if (event->modifiers() & Qt::ShiftModifier) {
                // Shift+PageDown: 水平镜像
                mirrorHorizontal();
                event->accept();
            } else {
                // PageDown：减少透明度（变得更透明）
                double currentOpacity = windowOpacity();
                double newOpacity = qMax(0.1, currentOpacity - 0.1);
                setWindowOpacity(newOpacity);
                event->accept();
            }
            break;
        case Qt::Key_Escape:
            close();     //esc直接退出程序
            break;
        case Qt::Key_Space:
            toggleSlideshow();
            break;
        case Qt::Key_S:
            if (isSlideshowActive) {
                stopSlideshow();
            }
            break;
        case Qt::Key_Insert:
            enableCanvasMode();
            break;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            switchToThumbnailView();
            break;
        case Qt::Key_Menu:
            showContextMenu(this->mapToGlobal(QPoint(width()/2, height()/2)));
            break;
        case Qt::Key_Delete:
            deleteCurrentImage();
            break;
        default:
            QWidget::keyPressEvent(event);
        }
    } else {
        // 缩略图模式下的按键处理
        switch (event->key()) {
        case Qt::Key_Up:
        {
            QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
            int newValue = vScrollBar->value() - vScrollBar->singleStep();
            vScrollBar->setValue(newValue);
            event->accept();
        }
        break;
        case Qt::Key_Down:
        {
            QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
            int newValue = vScrollBar->value() + vScrollBar->singleStep();
            vScrollBar->setValue(newValue);
            event->accept();
        }
        break;
        case Qt::Key_PageUp:
        {
            QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
            int newValue = vScrollBar->value() - vScrollBar->pageStep();
            vScrollBar->setValue(newValue);
            event->accept();
        }
        break;
        case Qt::Key_PageDown:
        {
            QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
            int newValue = vScrollBar->value() + vScrollBar->pageStep();
            vScrollBar->setValue(newValue);
            event->accept();
        }
        break;
        case Qt::Key_Home:
            if (!imageList.isEmpty()) {
                scrollArea->verticalScrollBar()->setValue(0);
                currentImageIndex = 0;
                thumbnailWidget->setSelectedIndex(0);
                updateWindowTitle();
            }
            event->accept();
            break;
        case Qt::Key_End:
            if (!imageList.isEmpty()) {
                QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
                vScrollBar->setValue(vScrollBar->maximum());
                currentImageIndex = imageList.size() - 1;
                thumbnailWidget->setSelectedIndex(currentImageIndex);
                updateWindowTitle();
            }
            event->accept();
            break;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            if (currentImageIndex >= 0) {
                switchToSingleView(currentImageIndex);
            }
            event->accept();
            break;
        case Qt::Key_Delete:
            deleteSelectedThumbnail();
            event->accept();
            break;
        case Qt::Key_Escape:
            close();
            event->accept();
            break;
        case Qt::Key_Menu:
            showContextMenu(this->mapToGlobal(QPoint(width()/2, height()/2)));
            event->accept();
            break;
        case Qt::Key_Left:
        case Qt::Key_Right:
            event->ignore();
            break;
        default:
            QWidget::keyPressEvent(event);
        }
    }
}

void ImageWidget::navigateThumbnails(int key)
{
    if (imageList.isEmpty()) return;

    int columns = (width() - thumbnailSpacing) /
                  (thumbnailSize.width() + thumbnailSpacing);
    if (columns <= 0)
        columns = 1;

    int newIndex = currentImageIndex;
    if (newIndex < 0) newIndex = 0;

    switch (key) {
    case Qt::Key_Left:
        newIndex = (newIndex - 1 + imageList.size()) % imageList.size();
        break;
    case Qt::Key_Right:
        newIndex = (newIndex + 1) % imageList.size();
        break;
    case Qt::Key_Up:
        newIndex = (newIndex - columns + imageList.size()) % imageList.size();
        break;
    case Qt::Key_Down:
        newIndex = (newIndex + columns) % imageList.size();
        break;
    }

    currentImageIndex = newIndex;
    thumbnailWidget->setSelectedIndex(newIndex);
    updateWindowTitle();
}
