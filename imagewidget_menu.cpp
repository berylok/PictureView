// imagewidget_menu.cpp
#include "imagewidget.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>

void ImageWidget::showContextMenu(const QPoint &globalPos)
{
    QMenu contextMenu;

    // 设置菜单样式，确保在画布模式下正常显示
    contextMenu.setStyleSheet(
        "QMenu { "
        "   background-color: white; "
        "   color: black; "
        "   border: 1px solid #cccccc; "
        "}"
        "QMenu::item { "
        "   padding: 5px 20px 5px 20px; "
        "   background-color: transparent; "
        "}"
        "QMenu::item:selected { "
        "   background-color: #0066cc; "
        "   color: white; "
        "}"
        "QMenu::separator { "
        "   height: 1px; "
        "   background-color: #cccccc; "
        "   margin: 5px 0px 5px 0px; "
        "}"
        );

    // 如果在压缩包模式下，添加返回上级目录的选项
    if (isArchiveMode) {
        contextMenu.addAction(tr("返回上级目录 (ESC)"), this,
                              &ImageWidget::exitArchiveMode);
        contextMenu.addSeparator();
    }

    if (currentViewMode == SingleView) {
        contextMenu.addAction(tr("返回缩略图(Enter)"), this,
                              &ImageWidget::switchToThumbnailView);
        contextMenu.addSeparator();

        // 添加切换提示
        contextMenu.addAction(tr("上一张 (←)"), this,
                              &ImageWidget::loadPreviousImage);
        contextMenu.addAction(tr("下一张 (→)"), this,
                              &ImageWidget::loadNextImage);

        // 添加合适大小和实际大小的菜单项
        contextMenu.addAction(tr("合适大小 (↑)"), this,
                              &ImageWidget::fitToWindow);
        contextMenu.addAction(tr("实际大小 (↓)"), this, &ImageWidget::actualSize);
        contextMenu.addSeparator();

        // 编辑菜单
        QMenu *editMenu = contextMenu.addMenu(tr("编辑"));
        QMenu *rotateSubMenu = editMenu->addMenu(tr("旋转"));
        rotateSubMenu->addAction(tr("逆时针90° (PageUp)"), this,
                                 &ImageWidget::rotate90CCW);
        rotateSubMenu->addAction(tr("顺时针90° (PageDown)"), this,
                                 &ImageWidget::rotate90CW);
        rotateSubMenu->addAction(tr("180°"), this, &ImageWidget::rotate180);

        QMenu *mirrorSubMenu = editMenu->addMenu(tr("镜像"));
        mirrorSubMenu->addAction(tr("垂直镜像 (Ctrl+PageUp)"), this,
                                 &ImageWidget::mirrorVertical);
        mirrorSubMenu->addAction(tr("水平镜像 (Ctrl+PageDown)"), this,
                                 &ImageWidget::mirrorHorizontal);

        if (isTransformed()) {
            editMenu->addSeparator();
            editMenu->addAction(tr("重置变换 (Ctrl+0)"), this,
                                &ImageWidget::resetTransform);
        }

        contextMenu.addSeparator();

        // 添加删除菜单项
        contextMenu.addAction(tr("删除当前图片 (Del)"), this,
                              &ImageWidget::deleteCurrentImage);
        contextMenu.addSeparator();

    } else {
        // 缩略图模式下的菜单
        int selectedIndex = thumbnailWidget->getSelectedIndex();
        if(selectedIndex>=0 && selectedIndex < imageList.size()){

            contextMenu.addAction(tr("打开图片(Enter)"), this, &ImageWidget::openSelectedImage);
            contextMenu.addSeparator();

            contextMenu.addAction(tr("删除选中图片 (Del)"), this, &ImageWidget::deleteSelectedThumbnail);
            contextMenu.addSeparator();
        }
    }

    // 打开文件菜单
    QMenu *openfileshowMenu = contextMenu.addMenu(tr("文件..."));
    QAction *openfileshowAction =
        openfileshowMenu->addAction(tr("打开文件夹 (Ctrl+O)"));
    connect(openfileshowAction, &QAction::triggered, this,
            &ImageWidget::openFolder);

    QAction *openpicshowAction =
        openfileshowMenu->addAction(tr("打开图片 (Ctrl+Shift+O)"));
    connect(openpicshowAction, &QAction::triggered, this,
            &ImageWidget::openImage);

    if (currentViewMode == SingleView) {
        QAction *showAction1 =
            openfileshowMenu->addAction(tr("保存图片 (Ctrl+S)"));
        connect(showAction1, &QAction::triggered, this, &ImageWidget::saveImage);

        QAction *showAction2 =
            openfileshowMenu->addAction(tr("拷贝图片至剪切板 (Ctrl+C)"));
        connect(showAction2, &QAction::triggered, this,
                &ImageWidget::copyImageToClipboard);

        QAction *showAction3 =
            openfileshowMenu->addAction(tr("粘贴剪切板图片 (Ctrl+V)"));
        connect(showAction3, &QAction::triggered, this,
                &ImageWidget::pasteImageFromClipboard);
    }

    // 窗口控制菜单
    QMenu *windowshowMenu = contextMenu.addMenu(tr("窗口"));
    QAction *windowshowAction1 = windowshowMenu->addAction(
        hasTitleBar() ? tr("隐藏标题栏") : tr("显示标题栏"));
    connect(windowshowAction1, &QAction::triggered, this,
            &ImageWidget::toggleTitleBar);

    QAction *windowshowAction2 = windowshowMenu->addAction(
        isAlwaysOnTop() ? tr("取消置顶") : tr("窗口置顶"));
    connect(windowshowAction2, &QAction::triggered, this,
            &ImageWidget::toggleAlwaysOnTop);

    QAction *windowshowAction3 = windowshowMenu->addAction(
        hasTransparentBackground() ? tr("黑色背景")
                                   : tr("透明背景（需隐藏标题+设置后重开程序）"));
    connect(windowshowAction3, &QAction::triggered, this,
            &ImageWidget::toggleTransparentBackground);

    // 透明度子菜单
    QMenu *opacitySubMenu = windowshowMenu->addMenu(tr("透明度"));
    QAction *opacity100 = opacitySubMenu->addAction(tr("100% - 不透明"));
    QAction *opacity90 = opacitySubMenu->addAction("90%");
    QAction *opacity80 = opacitySubMenu->addAction("80%");
    QAction *opacity70 = opacitySubMenu->addAction("70%");
    QAction *opacity60 = opacitySubMenu->addAction("60%");
    QAction *opacity50 = opacitySubMenu->addAction("50%");
    QAction *opacity40 = opacitySubMenu->addAction("40%");
    QAction *opacity30 = opacitySubMenu->addAction("30%");
    QAction *opacity20 = opacitySubMenu->addAction("20%");
    QAction *opacity10 = opacitySubMenu->addAction("10%");

    // 设置可勾选状态
    opacity100->setCheckable(true);
    opacity90->setCheckable(true);
    opacity80->setCheckable(true);
    opacity70->setCheckable(true);
    opacity60->setCheckable(true);
    opacity50->setCheckable(true);
    opacity40->setCheckable(true);
    opacity30->setCheckable(true);
    opacity20->setCheckable(true);
    opacity10->setCheckable(true);

    // 根据当前透明度设置勾选状态
    opacity100->setChecked(qAbs(m_windowOpacity - 1.0) < 0.05);
    opacity90->setChecked(qAbs(m_windowOpacity - 0.9) < 0.05);
    opacity80->setChecked(qAbs(m_windowOpacity - 0.8) < 0.05);
    opacity70->setChecked(qAbs(m_windowOpacity - 0.7) < 0.05);
    opacity60->setChecked(qAbs(m_windowOpacity - 0.6) < 0.05);
    opacity50->setChecked(qAbs(m_windowOpacity - 0.5) < 0.05);
    opacity40->setChecked(qAbs(m_windowOpacity - 0.4) < 0.05);
    opacity30->setChecked(qAbs(m_windowOpacity - 0.3) < 0.05);
    opacity20->setChecked(qAbs(m_windowOpacity - 0.2) < 0.05);
    opacity10->setChecked(qAbs(m_windowOpacity - 0.1) < 0.05);

    // 连接透明度选项
    connect(opacity100, &QAction::triggered, [this]() { setWindowOpacityValue(1.0); });
    connect(opacity90, &QAction::triggered, [this]() { setWindowOpacityValue(0.9); });
    connect(opacity80, &QAction::triggered, [this]() { setWindowOpacityValue(0.8); });
    connect(opacity70, &QAction::triggered, [this]() { setWindowOpacityValue(0.7); });
    connect(opacity60, &QAction::triggered, [this]() { setWindowOpacityValue(0.6); });
    connect(opacity50, &QAction::triggered, [this]() { setWindowOpacityValue(0.5); });
    connect(opacity40, &QAction::triggered, [this]() { setWindowOpacityValue(0.4); });
    connect(opacity30, &QAction::triggered, [this]() { setWindowOpacityValue(0.3); });
    connect(opacity20, &QAction::triggered, [this]() { setWindowOpacityValue(0.2); });
    connect(opacity10, &QAction::triggered, [this]() { setWindowOpacityValue(0.1); });

    if (currentViewMode == SingleView) {
        // 在画布模式下显示不同的菜单项
        if (canvasMode) {
            QAction *exitCanvasAction =
                windowshowMenu->addAction(tr("退出画布模式 (ESC/Insert)"));
            connect(exitCanvasAction, &QAction::triggered, this,
                    &ImageWidget::disableCanvasMode);
            windowshowMenu->addSeparator();
            windowshowMenu->addAction(tr("增加透明度 (PageUp)"));
            windowshowMenu->addAction(tr("减少透明度 (PageDown)"));
        } else {
            QAction *canvasModeAction =
                windowshowMenu->addAction(tr("进入画布模式 (Insert)"));
            connect(canvasModeAction, &QAction::triggered, this,
                    &ImageWidget::toggleCanvasMode);
        }
    }

    // 幻灯片菜单
    QMenu *slideshowMenu = contextMenu.addMenu(tr("幻灯功能"));
    QAction *slideshowAction = slideshowMenu->addAction(
        isSlideshowActive ? tr("停止幻灯") : tr("开始幻灯"));
    connect(slideshowAction, &QAction::triggered, this,
            &ImageWidget::toggleSlideshow);

    slideshowMenu->addSeparator();

    QMenu *intervalMenu = slideshowMenu->addMenu(tr("切换间隔"));
    QAction *interval1s = intervalMenu->addAction(tr("1秒"));
    QAction *interval2s = intervalMenu->addAction(tr("2秒"));
    QAction *interval3s = intervalMenu->addAction(tr("3秒"));
    QAction *interval5s = intervalMenu->addAction(tr("5秒"));
    QAction *interval10s = intervalMenu->addAction(tr("10秒"));

    interval1s->setCheckable(true);
    interval2s->setCheckable(true);
    interval3s->setCheckable(true);
    interval5s->setCheckable(true);
    interval10s->setCheckable(true);

    interval1s->setChecked(slideshowInterval == 1000);
    interval2s->setChecked(slideshowInterval == 2000);
    interval3s->setChecked(slideshowInterval == 3000);
    interval5s->setChecked(slideshowInterval == 5000);
    interval10s->setChecked(slideshowInterval == 10000);

    connect(interval1s, &QAction::triggered, [this]() { setSlideshowInterval(1000); });
    connect(interval2s, &QAction::triggered, [this]() { setSlideshowInterval(2000); });
    connect(interval3s, &QAction::triggered, [this]() { setSlideshowInterval(3000); });
    connect(interval5s, &QAction::triggered, [this]() { setSlideshowInterval(5000); });
    connect(interval10s, &QAction::triggered, [this]() { setSlideshowInterval(10000); });

    // 帮助菜单
    QMenu *helpMenu = contextMenu.addMenu(tr("帮助"));
    QAction *aboutAction = helpMenu->addAction(tr("关于 (F1)"));
    connect(aboutAction, &QAction::triggered, this,
            &ImageWidget::showAboutDialog);

    QAction *shortcutHelpAction = helpMenu->addAction(tr("快捷键帮助"));
    connect(shortcutHelpAction, &QAction::triggered, this,
            &ImageWidget::showShortcutHelp);

    contextMenu.addSeparator();
    contextMenu.addAction(tr("退出（Esc）"), this, &QWidget::close);

    contextMenu.exec(globalPos);
}
void ImageWidget::openSelectedImage()
{
    int selectedIndex = thumbnailWidget->getSelectedIndex();
    if(selectedIndex >= 0 && selectedIndex < imageList.size()){
        // 触发与双击缩略图相同的行为
        thumbnailWidget->thumbnailClicked(selectedIndex);
    }
}
