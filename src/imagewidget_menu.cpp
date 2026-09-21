
// imagewidget_menu.cpp
#include "imagewidget.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QProcess>
#include "platform_compat.h"
#include <QColorDialog>
#include <QIcon>
#include <QPixmap>

void ImageWidget::showContextMenu(const QPoint &globalPos)
{
    QMenu contextMenu;

    // 设置菜单样式
    // ★ 从配置读高亮色
    QColor hl(currentConfig.highlightColor);
    if (!hl.isValid()) hl = QColor("#00A0E9");

    // 文字色：亮底用黑字，暗底用白字
    QString hlTextColor = (hl.lightness() > 160) ? "black" : "white";

    QString menuStyle = QString(
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
                            "   background-color: %1; "
                            "   color: %2; "
                            "}"
                            "QMenu::separator { "
                            "   height: 1px; "
                            "   background-color: #cccccc; "
                            "   margin: 5px 0px 5px 0px; "
                            "}"
                            ).arg(hl.name(), hlTextColor);

    contextMenu.setStyleSheet(menuStyle);

    // 如果在压缩包模式下，添加返回上级目录的选项
    if (isArchiveMode) {
        contextMenu.addAction(tr("返回上级目录 (ESC)"), this,&ImageWidget::exitArchiveMode);
        contextMenu.addSeparator();
    }

    if (currentViewMode == SingleView) {
        contextMenu.addAction(tr("窗口最小化 (Ctrl+M)"), this,&ImageWidget::showMinimized);
        contextMenu.addSeparator();
        contextMenu.addAction(tr("返回缩略图(Enter)"), this,&ImageWidget::switchToThumbnailView);
        contextMenu.addSeparator();

        // 添加切换提示
        contextMenu.addAction(tr("上一张 (←)"), this,&ImageWidget::loadPreviousImage);
        contextMenu.addAction(tr("下一张 (→)"), this,&ImageWidget::loadNextImage);

        // 添加合适大小和实际大小的菜单项
        contextMenu.addAction(tr("合适大小 (↑)"), this,&ImageWidget::fitToWindow);
        contextMenu.addAction(tr("实际大小 (↓)"), this, &ImageWidget::actualSize);
        contextMenu.addSeparator();

        // 编辑菜单
        QMenu *editMenu = contextMenu.addMenu(tr("编辑"));
        QMenu *rotateSubMenu = editMenu->addMenu(tr("旋转"));
        rotateSubMenu->addAction(tr("逆时针90° (PageUp)"), this,&ImageWidget::rotate90CCW);
        rotateSubMenu->addAction(tr("顺时针90° (PageDown)"), this,&ImageWidget::rotate90CW);
        rotateSubMenu->addAction(tr("180°"), this, &ImageWidget::rotate180);

        QMenu *mirrorSubMenu = editMenu->addMenu(tr("镜像"));
        mirrorSubMenu->addAction(tr("垂直镜像 (Ctrl+PageUp)"), this,&ImageWidget::mirrorVertical);
        mirrorSubMenu->addAction(tr("水平镜像 (Ctrl+PageDown)"), this,&ImageWidget::mirrorHorizontal);

        if (isTransformed()) {
            editMenu->addSeparator();
            editMenu->addAction(tr("重置变换 (Ctrl+0)"), this,&ImageWidget::resetTransform);
        }
        // 在编辑菜单中，旋转和镜像选项之后添加
        editMenu->addSeparator();

        QAction *lockTransformAction = editMenu->addAction(
            transformLocked ? tr("解锁变换") : tr("锁定变换"));
        lockTransformAction->setCheckable(true);
        lockTransformAction->setChecked(transformLocked);
        connect(lockTransformAction, &QAction::triggered, this, &ImageWidget::toggleTransformLock);

        contextMenu.addSeparator();

        // 添加删除菜单项
        contextMenu.addAction(tr("删除当前图片 (Del)"), this,
                              &ImageWidget::deleteCurrentImage);
        contextMenu.addSeparator();

    } else {
        // 缩略图模式下的菜单
        int selectedIndex = thumbnailWidget->getSelectedIndex();
        if(selectedIndex>=0 && selectedIndex < imageList.size()){

            // 最小化
            contextMenu.addAction(tr("窗口最小化(Ctrl+M)"), this,&ImageWidget::showMinimized);
            contextMenu.addSeparator();


            contextMenu.addAction(tr("打开图片(Enter)"), this, &ImageWidget::openSelectedImage);
            contextMenu.addAction(tr("删除选中图片 (Del)"), this, &ImageWidget::deleteSelectedThumbnail);
            contextMenu.addSeparator();
        }
    }


    // 打开文件菜单
    QMenu *openfileshowMenu = contextMenu.addMenu(tr("文件..."));


    // 获取当前有效图片路径的辅助 lambda
    auto getCurrentImagePath = [this]() -> QString {
        if (currentViewMode == SingleView) {
            return currentImagePath;
        } else {
            int selected = thumbnailWidget->getSelectedIndex();
            if (selected >= 0 && selected < imageList.size()) {
                if (isArchiveMode) return QString();
                return currentDir.absoluteFilePath(imageList.at(selected));
            }
            return QString();
        }
    };








    QAction *openfileshowAction =
        openfileshowMenu->addAction(tr("打开文件夹 (Ctrl+O)"));
    connect(openfileshowAction, &QAction::triggered, this,
            &ImageWidget::openFolder);

    QAction *openpicshowAction =
        openfileshowMenu->addAction(tr("打开图片 (Ctrl+Shift+O)"));
    connect(openpicshowAction, &QAction::triggered, this,
            &ImageWidget::openImage);

      openfileshowMenu->addSeparator();

    // 1. 在新窗口打开图片
    QAction *openInNewWindowAction = new QAction(tr("在新窗口打开图片(Ctrl+N)"), this);
    QString imgPath = getCurrentImagePath();  // 用于初始启用状态
    bool hasValidImage = !imgPath.isEmpty() && QFile::exists(imgPath) && !isArchiveMode;
    openInNewWindowAction->setVisible(hasValidImage);

    connect(openInNewWindowAction, &QAction::triggered, this,
            [this, getCurrentImagePath]() {  // 按值捕获 getCurrentImagePath
                QString path = getCurrentImagePath();
                if (!path.isEmpty() && QFile::exists(path)) {
                    QString program = QCoreApplication::applicationFilePath();
                    QStringList arguments;
                    arguments << path;
                    QProcess::startDetached(program, arguments);
                } else {
                    QMessageBox::warning(this, tr("警告"), tr("没有可用的图片文件"));
                }
            });
    openfileshowMenu->addAction(openInNewWindowAction);

    // 2. 打开图片所在文件夹
    QAction *showInFolderAction = new QAction(tr("打开图片所在文件夹"), this);
    showInFolderAction->setVisible(hasValidImage);

    connect(showInFolderAction, &QAction::triggered, this,
            [this, getCurrentImagePath]() {  // 同样捕获
                QString path = getCurrentImagePath();
                if (!path.isEmpty() && QFile::exists(path)) {
                    PlatformCompat::showInFolder(path);
                } else {
                    QMessageBox::warning(this, tr("警告"), tr("没有可用的图片文件"));
                }
            });
    openfileshowMenu->addAction(showInFolderAction);

    openfileshowMenu->addSeparator();



    if (currentViewMode == SingleView) {
        QAction *showAction1 =
            openfileshowMenu->addAction(tr("保存图片 (Ctrl+S)"));
        connect(showAction1, &QAction::triggered, this, &ImageWidget::saveImage);

        openfileshowMenu->addSeparator();

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

    // 沉浸模式选项（放在最前面，突出显示）
    // ✅ 两个独立的菜单项
    bool isImmersive = isMaximized() && !hasTitleBar();
    bool isTransparentImm = isImmersive && hasTransparentBackground();

    QAction *immersiveBlackAction = windowshowMenu->addAction(
        (isImmersive && !isTransparentImm) ? tr("退出黑色沉浸 (Ctrl+F)")
                                           : tr("黑色沉浸模式 (Ctrl+F)"));
    connect(immersiveBlackAction, &QAction::triggered, this, [this]() {
        toggleImmersiveMode(false);
    });

    QAction *immersiveTransparentAction = windowshowMenu->addAction(
        isTransparentImm ? tr("退出透明沉浸 (Ctrl+Shift+F)")
                         : tr("透明沉浸模式 (Ctrl+Shift+F)"));
    connect(immersiveTransparentAction, &QAction::triggered, this, [this]() {
        toggleImmersiveMode(true);
    });

    windowshowMenu->addSeparator();
    QMenu *scalingMenu = windowshowMenu->addMenu(tr("图片缩放方式"));

    QAction *smoothAct = scalingMenu->addAction(tr("平滑插值（推荐）"));
    QAction *pixelAct  = scalingMenu->addAction(tr("像素（最近邻）"));

    smoothAct->setCheckable(true);
    pixelAct->setCheckable(true);
    smoothAct->setChecked(currentConfig.smoothScaling);
    pixelAct->setChecked(!currentConfig.smoothScaling);

    connect(smoothAct, &QAction::triggered, this, [this]() {
        currentConfig.smoothScaling = true;
        saveConfiguration();
        update();
    });

    connect(pixelAct, &QAction::triggered, this, [this]() {
        currentConfig.smoothScaling = false;
        saveConfiguration();
        update();
    });


    windowshowMenu->addSeparator();

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
                                   : tr("透明背景（最大化+隐藏标题+透明背景+设置后重开程序）"));
    connect(windowshowAction3, &QAction::triggered, this,
            &ImageWidget::toggleTransparentBackground);

    // ==================== 高亮颜色 ====================
    windowshowMenu->addSeparator();
    QMenu *highlightMenu = windowshowMenu->addMenu(tr("高亮颜色"));

    struct HighlightPreset {
        const char *name;
        const char *color;
    };
    static const HighlightPreset presets[] = {
                                              { QT_TR_NOOP("蓝色（默认）"), "#00A0E9" },
                                              { QT_TR_NOOP("琥珀色"),       "#FFA500" },
                                              { QT_TR_NOOP("橙红色"),       "#FF7830" },
                                              { QT_TR_NOOP("暖金色"),       "#E6B450" },
                                              { QT_TR_NOOP("蜜桃色"),       "#FFBE82" },
                                              { QT_TR_NOOP("珊瑚色"),       "#FF645A" },
                                              { QT_TR_NOOP("青绿色"),       "#20B2AA" },
                                              { QT_TR_NOOP("紫色"),         "#9B59B6" },
                                              { QT_TR_NOOP("粉色"),         "#FF69B4" },
                                              { QT_TR_NOOP("灰白色"),       "#CCCCCC" },
                                              };

    QString current = currentConfig.highlightColor.toUpper();

    for (const auto &p : presets) {
        QAction *act = highlightMenu->addAction(tr(p.name));

        // 前面画个小色块
        QPixmap swatch(16, 16);
        swatch.fill(QColor(p.color));
        act->setIcon(QIcon(swatch));

        act->setCheckable(true);
        act->setChecked(current == QString(p.color).toUpper());

        QString colorStr = p.color;
        connect(act, &QAction::triggered, this, [this, colorStr]() {
            currentConfig.highlightColor = colorStr;
            saveConfiguration();
            if (thumbnailWidget) thumbnailWidget->update();
            qDebug() << "高亮颜色设为:" << colorStr;
        });
    }

    // 自定义选项
    highlightMenu->addSeparator();
    QAction *customAct = highlightMenu->addAction(tr("自定义..."));
    connect(customAct, &QAction::triggered, this, [this]() {
        QColor c = QColorDialog::getColor(
            QColor(currentConfig.highlightColor),
            this, tr("选择高亮颜色"));
        if (c.isValid()) {
            currentConfig.highlightColor = c.name();   // #RRGGBB
            saveConfiguration();
            if (thumbnailWidget) thumbnailWidget->update();
        }
    });


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
        windowshowMenu->addSeparator();  // 与其他选项分隔
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

    // ==================== 缩放方式 ====================
    windowshowMenu->addSeparator();

    QMenu *scaleModeMenu = windowshowMenu->addMenu(tr("缩放方式（Ctrl+B）"));

    QAction *scaleFitAction = scaleModeMenu->addAction(tr("完整适应窗口"));
    QAction *scaleBoxAction = scaleModeMenu->addAction(tr("统一包围盒"));

    scaleFitAction->setCheckable(true);
    scaleBoxAction->setCheckable(true);

    scaleFitAction->setChecked(slideScaleMode == SlideFitWindow);
    scaleBoxAction->setChecked(slideScaleMode == SlideBoundingBox);

    connect(scaleFitAction, &QAction::triggered, this, [this]() {
        slideScaleMode = SlideFitWindow;
        boxZoom = 1.0;
        if (currentViewMode == SingleView && !pixmap.isNull()) {
            fitToWindow();
        }
    });

    connect(scaleBoxAction, &QAction::triggered, this, [this]() {
        slideScaleMode = SlideBoundingBox;
        boxZoom = 1.0;
        if (currentViewMode == SingleView && !pixmap.isNull()) {
            fitToWindow();
        }
    });

    contextMenu.addSeparator();

    // ==================== 图片解码尺寸 ====================
    QMenu *decodeMenu = contextMenu.addMenu(tr("图片解码尺寸"));

    QAction *decode1k        = decodeMenu->addAction(tr("1000 像素 (极速)"));
    QAction *decode2k        = decodeMenu->addAction(tr("2000 像素 (高速)"));
    QAction *decode4k        = decodeMenu->addAction(tr("4000 像素 (推荐)"));
    QAction *decode8k        = decodeMenu->addAction(tr("8000 像素 (高清晰)"));
    QAction *decodeUnlimited = decodeMenu->addAction(tr("不限制 (可能很慢)"));

    decode1k->setCheckable(true);
    decode2k->setCheckable(true);
    decode4k->setCheckable(true);
    decode8k->setCheckable(true);
    decodeUnlimited->setCheckable(true);

    int cur = currentConfig.maxDecodeSize;
    decode1k->setChecked(cur == 1000);
    decode2k->setChecked(cur == 2000);
    decode4k->setChecked(cur == 4000);
    decode8k->setChecked(cur == 8000);
    decodeUnlimited->setChecked(cur == 0);

    auto setDecodeSize = [this](int size) {
        // ① 保存配置
        currentConfig.maxDecodeSize = size;
        saveConfiguration();

        // ★ 代际 +1，作废所有在途预加载
        m_cacheGeneration.fetch_add(1);

        qDebug() << "解码尺寸上限设为:"
                 << (size == 0 ? QStringLiteral("不限制") : QString::number(size));

        // ② 清空主图 / 压缩包缓存
        {
            QMutexLocker locker(&cacheMutex);
            imageCache.clear();
            archiveImageCache.clear();
        }

        // ③ 清空缩略图缓存 + 触发重新加载
        if (thumbnailWidget) {
            thumbnailWidget->clearThumbnailCache();

            // ★ 关键：重置图片列表，让 ThumbnailWidget 重新走批量加载
            if (!imageList.isEmpty()) {
                if (isArchiveMode) {
                    // 压缩包模式：传带 "|" 的路径
                    QStringList thumbnailPaths;
                    for (const QString &f : imageList) {
                        thumbnailPaths.append(currentArchivePath + "|" + f);
                    }
                    thumbnailWidget->setImageList(thumbnailPaths, QDir());
                } else {
                    thumbnailWidget->setImageList(imageList, currentDir);
                }
            } else {
                thumbnailWidget->update();   // 空列表就刷一下重绘
            }
        }

        // ④ 重载当前单张图
        if (!currentImagePath.isEmpty() && currentViewMode == SingleView) {
            loadImage(currentImagePath, false);
        }
    };

    connect(decode1k,        &QAction::triggered, this, [setDecodeSize]() { setDecodeSize(1000); });
    connect(decode2k,        &QAction::triggered, this, [setDecodeSize]() { setDecodeSize(2000); });
    connect(decode4k,        &QAction::triggered, this, [setDecodeSize]() { setDecodeSize(4000); });
    connect(decode8k,        &QAction::triggered, this, [setDecodeSize]() { setDecodeSize(8000); });
    connect(decodeUnlimited, &QAction::triggered, this, [setDecodeSize]() { setDecodeSize(0); });


    // ==================== 缓存条目数 ====================
    QMenu *cacheMenu = contextMenu.addMenu(tr("缓存图片数"));

    QAction *cache10  = cacheMenu->addAction(tr("10 张 (低内存)"));
    QAction *cache30  = cacheMenu->addAction(tr("30 张"));
    QAction *cache50  = cacheMenu->addAction(tr("50 张 (推荐)"));
    QAction *cache100 = cacheMenu->addAction(tr("100 张"));
    QAction *cache200 = cacheMenu->addAction(tr("200 张 (高内存)"));
    QAction *cacheOff = cacheMenu->addAction(tr("不缓存 (最省内存)"));

    QList<QPair<QAction*, int>> cacheItems = {
        {cache10,  10},
        {cache30,  30},
        {cache50,  50},
        {cache100, 100},
        {cache200, 200},
        {cacheOff, 0}
    };

    int curCache = currentConfig.pixmapCacheSize;
    for (auto &item : cacheItems) {
        item.first->setCheckable(true);
        item.first->setChecked(curCache == item.second);
    }

    auto setCacheSize = [this](int n) {
        currentConfig.pixmapCacheSize = n;
        saveConfiguration();

        if (n <= 0) {
            pixmapCache.clear();
            pixmapCache.setMaxCost(1);      // QCache 不接受 0，用 1 变相禁用
            //qDebug() << "缓存已禁用";
        } else {
            pixmapCache.setMaxCost(n);
            //qDebug() << "缓存上限:" << n << "张，当前条数:" << pixmapCache.size();
        }
    };

    for (auto &item : cacheItems) {
        int n = item.second;
        connect(item.first, &QAction::triggered, this, [setCacheSize, n]() {
            setCacheSize(n);
        });
    }


    QMenu *preloadMenu = contextMenu.addMenu(tr("预加载范围"));

    QAction *pl0  = preloadMenu->addAction(tr("0 张 (省内存)"));
    QAction *pl3  = preloadMenu->addAction(tr("前后各 3 张"));
    QAction *pl5  = preloadMenu->addAction(tr("前后各 5 张"));
    QAction *pl10 = preloadMenu->addAction(tr("前后各 10 张 (推荐)"));
    QAction *pl20 = preloadMenu->addAction(tr("前后各 20 张 (吃内存)"));

    QList<QPair<QAction*, int>> items = {
        {pl0, 0}, {pl3, 3}, {pl5, 5}, {pl10, 10}, {pl20, 20}
    };

    int cur2 = currentConfig.preloadRange;
    for (auto &it : items) {
        it.first->setCheckable(true);
        it.first->setChecked(cur2 == it.second);
    }

    auto setPreload = [this](int n) {
        currentConfig.preloadRange = n;
        saveConfiguration();
        qDebug() << "预加载范围:" << n;
    };

    for (auto &it : items) {
        int n = it.second;
        connect(it.first, &QAction::triggered, this, [setPreload, n]() {
            setPreload(n);
        });
    }



    // 显示当前模式的提示（可选）
    scaleModeMenu->addSeparator();
    QAction *currentModeInfo = scaleModeMenu->addAction(
        slideScaleMode == SlideBoundingBox ? tr("当前：包围盒") : tr("当前：完整适应"));
    currentModeInfo->setEnabled(false);

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

    contextMenu.addSeparator();
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

    if (canvasMode && canvasOverlay && canvasOverlay->isVisible()) {
        QTimer::singleShot(0, canvasOverlay, [this]() {
            canvasOverlay->forceStayOnTop();
            //qDebug() << "菜单关闭：强制覆盖层置顶";
        });
    }
}
void ImageWidget::openSelectedImage()
{
    int selectedIndex = thumbnailWidget->getSelectedIndex();
    if(selectedIndex >= 0 && selectedIndex < imageList.size()){
        // 触发与双击缩略图相同的行为
        thumbnailWidget->thumbnailClicked(selectedIndex);
    }
}
