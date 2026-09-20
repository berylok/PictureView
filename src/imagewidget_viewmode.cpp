
// imagewidget_viewmode.cpp
#include "imagewidget.h"
#include <QTimer>
#include <QMessageBox>

void ImageWidget::switchToThumbnailView()
{
    // 关键：禁用所有鼠标穿透（包括 X11 形状）
    disableMousePassthrough();

    // 关闭透明背景属性
    setAttribute(Qt::WA_TranslucentBackground, false);

    scrollArea->show();
    scrollArea->raise();

    currentViewMode = ThumbnailView;

    QTimer::singleShot(50, this, [this]() {
        if (currentViewMode == ThumbnailView) {
            // 1. 再次清除鼠标穿透（获取最新窗口尺寸）
            disableMousePassthrough();

            // 2. 强制窗口重新映射（hide/show 会触发窗口管理器重置状态）
            bool wasVisible = isVisible();
            bool wasMaximized = isMaximized();

            // 在延迟回调中，去掉 hide/show，改为：
            Qt::WindowFlags flags = windowFlags();
            setWindowFlags(flags);   // 重新应用当前窗口标志
            show();                  // 确保窗口可见（实际上已可见，但为了保险）
            update();                // 触发重绘
            repaint();               // 立即重绘

            if (wasMaximized) showMaximized();

            // 3. 再次清除，确保万无一失
            disableMousePassthrough();
            clearMask();

            //qDebug() << "终极清除：窗口已重新映射，形状应已恢复";
        }
    });




    // 确保缩略图部件获得焦点
    thumbnailWidget->setFocus();

    // 设置当前选中的缩略图索引
    if (currentImageIndex >= 0 && currentImageIndex < imageList.size()) {
        //qDebug() << "切换到缩略图模式，设置选中索引:" << currentImageIndex;
        thumbnailWidget->setSelectedIndex(currentImageIndex);

        // 延迟确保滚动位置正确
        QTimer::singleShot(50, [this]() {
            thumbnailWidget->ensureVisible(currentImageIndex);
        });
    } else if (!imageList.isEmpty()) {
        currentImageIndex = 0;
        //qDebug() << "切换到缩略图模式，设置默认选中索引:" << currentImageIndex;
        thumbnailWidget->setSelectedIndex(0);

        // 延迟确保滚动位置正确
        QTimer::singleShot(50, [this]() {
            thumbnailWidget->ensureVisible(0);
        });
    } else {
        //qDebug() << "切换到缩略图模式，无图片可选中";
        currentImageIndex = -1;
    }

    updateWindowTitle();
    ensureWindowVisible();
    update();
}

void ImageWidget::switchToSingleView(int index)
{
    currentViewMode = SingleView;
    scrollArea->hide();

    // 如果有有效的索引，加载对应的图片
    if (index >= 0 && index < imageList.size()) {
        // 不再保存当前状态
        loadImageByIndex(index);
    }

    // 🚨 关键：恢复用户之前设置的透明背景状态
    // currentConfig 是从配置加载的当前设置，或者您也可以直接检查保存的状态
    if (currentConfig.transparentBackground) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        updateMask();   // 立即生成掩码
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
        clearMask();    // 确保掩码被清除
    }

    update();
}

void ImageWidget::onThumbnailClicked(int index)
{
    if (index < 0 || index >= imageList.size()) return;

    QString fileName = imageList.at(index);
    QString filePath = currentDir.absoluteFilePath(fileName);

    // 检查是否是压缩包文件
    if (isArchiveFile(fileName)) {
        // 打开压缩包
        if (openArchive(filePath)) {
            //qDebug() << "成功打开压缩包:" << filePath;
        } else {
            //qDebug() << "打开压缩包失败:" << filePath;
            QMessageBox::warning(this, tr("错误"),
                                 tr("无法打开压缩包文件: %1").arg(fileName));
        }
    } else if (isArchiveMode) {
        // 在压缩包模式下点击图片
        currentImageIndex = index;
        loadImageByIndex(index);
        switchToSingleView();
    } else {
        // 普通图片文件
        currentImageIndex = index;
        switchToSingleView(index);
    }
}

void ImageWidget::onEnsureRectVisible(const QRect &rect)
{
    // 使用 ensureVisible 确保矩形区域可见
    scrollArea->ensureVisible(rect.x(), rect.y(), rect.width(), rect.height());
}

void ImageWidget::toggleImmersiveMode(bool useTransparent)
{
    bool currentlyImmersive   = isMaximized() && !hasTitleBar();
    bool currentlyTransparent = hasTransparentBackground();

    // ---------- 情况 1：已在沉浸模式，且背景类型相同 → 退出 ----------
    if (currentlyImmersive && currentlyTransparent == useTransparent) {
        bool wasVisible = isVisible();
        QRect normalGeometry = geometry();
        if (wasVisible) hide();
        setUpdatesEnabled(false);

        Qt::WindowFlags flags = windowFlags();
        flags &= ~Qt::FramelessWindowHint;
        setWindowFlags(flags);

        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
        clearMask();

        currentConfig.titleBarVisible = true;
        currentConfig.transparentBackground = false;
        currentConfig.windowMaximized = false;

        if (wasVisible) {
            setGeometry(normalGeometry);
            showNormal();
        }
        setUpdatesEnabled(true);
        update();
        saveConfiguration();
        return;
    }

    // ---------- 情况 2：进入或切换到目标沉浸模式 ----------
    bool wasVisible = isVisible();
    if (wasVisible) hide();
    setUpdatesEnabled(false);

    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::FramelessWindowHint;
    setWindowFlags(flags);

    setAttribute(Qt::WA_TranslucentBackground, useTransparent);
    setAutoFillBackground(!useTransparent);

    currentConfig.titleBarVisible = false;
    currentConfig.transparentBackground = useTransparent;
    currentConfig.windowMaximized = true;

    show();   // 确保窗口可见
    // 延迟一帧让 WM 处理 flags 变化后再最大化
    QTimer::singleShot(0, this, [this, useTransparent]() {
        showMaximized();
        if (useTransparent && currentViewMode == SingleView && !pixmap.isNull()) {
            updateMask();
        } else {
            clearMask();
        }
        setUpdatesEnabled(true);
        update();
    });

    saveConfiguration();

    // 透明背景首次启用提示重启
    if (useTransparent && !m_transparentBackgroundReady) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                                  tr("APP需要重启"),
                                                                  tr("透明背景首次切换需要重启程序才能完全生效。\n\n"

                                                                     "如果不重启，可能会有残影。确定立即重启APP？"),
                                                                  QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            restartApplication();
        }
    }
}
