#include "imagewidget.h"
#include "qapplication.h"
#include <QPainter>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QFileDialog>
#include <QClipboard>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyle>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QScreen>
#include <QGuiApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <windows.h>
#include <QPainterPath>
#include <QScrollBar>
#include <shellapi.h>

#include <QMainWindow>  // 必须添加这行
#include <QAction>
#include <QContextMenuEvent>

#include "canvascontrolpanel.h"  // 确保包含控制面板头文件

// imagewidget.cpp
ImageWidget::ImageWidget(QWidget *parent) : QWidget(parent),
    scaleFactor(1.0),
    panOffset(0, 0),
    isDraggingWindow(false),
    isPanningImage(false),
    currentImageIndex(-1),
    isSlideshowActive(false),
    slideshowInterval(3000),
    currentViewMode(ThumbnailView),
    thumbnailSize(150, 150),
    thumbnailSpacing(10),
    currentViewStateType(FitToWindow),
    mouseInImage(false),
    showNavigationHints(true),  // 默认显示导航提示
    m_windowOpacity(1.0),  // 默认完全不透明

    canvasMode(false),
    mousePassthrough(false),
    controlPanel(nullptr),
    openFolderAction(nullptr),
    openImageAction(nullptr),    // 新增：初始化保存动作为nullptr
    saveImageAction(nullptr),    // 新增：初始化拷贝动作为nullptr
    copyImageAction(nullptr),   // 新增：初始化粘贴动作为nullptr
    pasteImageAction(nullptr),     // 新增：初始化关于动作为nullptr
    aboutAction(nullptr),  // 确保在这里初始化 controlPanel

    rotationAngle(0),
    isHorizontallyFlipped(false),
    isVerticallyFlipped(false)

{
    // 创建缩略图部件 - 确保 this 作为父窗口传递
    thumbnailWidget = new ThumbnailWidget(this);  // 关键：传递 this 作为父窗口

    // 创建配置管理器
    configManager = new ConfigManager("image_viewer_config.ini");


    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);


    // 创建滚动区域
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);

    // 设置滚动条不接收焦点
    scrollArea->horizontalScrollBar()->setFocusPolicy(Qt::NoFocus);
    scrollArea->verticalScrollBar()->setFocusPolicy(Qt::NoFocus);


    // 创建缩略图部件
    thumbnailWidget = new ThumbnailWidget(this);
    scrollArea->setWidget(thumbnailWidget);

    // 测试父窗口关系
    qDebug() << "ThumbnailWidget 父窗口:" << thumbnailWidget->parentWidget();
    qDebug()
        << "父窗口类型:"
        << (thumbnailWidget->parentWidget()
            ? thumbnailWidget->parentWidget()->metaObject()->className()
                : "null");

    scrollArea->show();


    // 连接信号
    // 修改信号连接 - 确保双击能打开图片
    connect(thumbnailWidget, &ThumbnailWidget::thumbnailClicked, this,
            &ImageWidget::onThumbnailClicked);
    connect(thumbnailWidget, &ThumbnailWidget::ensureRectVisible, this,
            &ImageWidget::onEnsureRectVisible);

    mainLayout->addWidget(scrollArea);

    // 启用拖拽功能
    setAcceptDrops(true);

    // 加载配置
    loadConfiguration();


    // 创建快捷键动作
    createShortcutActions();


    // showMaximized();
    setFocusPolicy(Qt::StrongFocus);

    // 创建幻灯定时器
    slideshowTimer = new QTimer(this);
    connect(slideshowTimer, &QTimer::timeout, this, &ImageWidget::slideshowNext);

    setMouseTracking(true); // 启用鼠标跟踪


    // 初始重绘以确保无残影
    update();

}

ImageWidget::~ImageWidget()
{
    // 确保销毁控制面板
    destroyControlPanel();

    delete slideshowTimer;
    delete thumbnailWidget;
    delete scrollArea;
}


// 创建控制面板
void ImageWidget::createControlPanel()
{
    if (!controlPanel) {
        controlPanel = new CanvasControlPanel();
        connect(controlPanel, &CanvasControlPanel::exitCanvasMode, this,
                &ImageWidget::onExitCanvasMode);
        positionControlPanel();
        controlPanel->show();
        qDebug() << "控制面板已创建";
    }
}

// 销毁控制面板
void ImageWidget::destroyControlPanel()
{
    if (controlPanel) {
        controlPanel->hide();
        controlPanel->deleteLater();
        controlPanel = nullptr;
        qDebug() << "控制面板已销毁";
    }
}

// 定位控制面板到屏幕右上角
void ImageWidget::positionControlPanel()
{
    if (controlPanel) {
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect screenGeometry = screen->availableGeometry();

        // 放置在屏幕右上角，留出一些边距
        int x = screenGeometry.width() - controlPanel->width() - 10;
        int y = 10;

        controlPanel->move(x, y);
        qDebug() << "控制面板已定位到:" << x << "," << y;
    }
}

// 退出画布模式槽函数
void ImageWidget::onExitCanvasMode()
{
    qDebug() << "通过控制面板退出画布模式";
    disableCanvasMode();
}


// 简化的画布模式实现
void ImageWidget::toggleCanvasMode()
{
    if (canvasMode) {
        disableCanvasMode();
    } else {
        enableCanvasMode();
    }
}

void ImageWidget::enableCanvasMode()
{
    qDebug() << "启用画布模式";

    canvasMode = true;

    // 先隐藏窗口
    hide();

    // 保存当前窗口状态
    bool wasMaximized = isMaximized();
    QRect normalGeometry = geometry();

    // 方法2：不使用 WA_TranslucentBackground，而是通过设置背景色
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // 设置窗口背景为透明
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, false); // 明确禁用

    // 设置样式表实现透明
    setStyleSheet("background-color: rgba(0, 0, 0, 0);");

    // 设置初始半透明状态
    setWindowOpacity(0.7);

    // 恢复窗口显示
    if (wasMaximized) {
        showMaximized();
    } else {
        if (normalGeometry.isValid() && !normalGeometry.isEmpty()) {
            setGeometry(normalGeometry);
        } else {
            QScreen *screen = QGuiApplication::primaryScreen();
            QRect screenGeometry = screen->availableGeometry();
            setGeometry(screenGeometry);
        }
        showNormal();
    }

    // 启用鼠标穿透
    enableMousePassthrough();

    // 创建控制面板
    createControlPanel();

    // 确保窗口获得焦点
    activateWindow();
    raise();

}

void ImageWidget::disableCanvasMode()
{
    qDebug() << "禁用画布模式";

    canvasMode = false;

    // 禁用鼠标穿透
    disableMousePassthrough();

    setWindowOpacity(1.0);

    // 销毁控制面板
    destroyControlPanel();

    // 恢复正常的窗口属性
    loadConfiguration(); // 直接重新加载配置恢复原始状态

    update();
}


// imagewidget.cpp
// 简化的鼠标穿透实现
void ImageWidget::enableMousePassthrough()
{
#ifdef Q_OS_WIN
    // 使用 Windows API 设置鼠标穿透
    SetWindowLongPtr((HWND)winId(), GWL_EXSTYLE,
                     GetWindowLongPtr((HWND)winId(), GWL_EXSTYLE) | WS_EX_TRANSPARENT);
#endif

    mousePassthrough = true;
    //logMessage("鼠标穿透已启用");
}

void ImageWidget::disableMousePassthrough()
{
#ifdef Q_OS_WIN
    // 移除 Windows 鼠标穿透
    SetWindowLongPtr((HWND)winId(), GWL_EXSTYLE,
                     GetWindowLongPtr((HWND)winId(), GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
#endif

    mousePassthrough = false;
    //logMessage("鼠标穿透已禁用");
}


// 关闭事件处理
void ImageWidget::closeEvent(QCloseEvent *event)
{
    // 销毁控制面板
    destroyControlPanel();

    saveConfiguration();
    event->accept();
}

// 修改配置相关方法，只处理窗口状态和标题栏状态
void ImageWidget::loadConfiguration()
{
    ConfigManager::Config config = configManager->loadConfig();

    // 保存最后路径到当前配置
    currentConfig.lastOpenPath = config.lastOpenPath;
    applyConfiguration(config);
}

void ImageWidget::saveConfiguration()
{
    ConfigManager::Config config;

    // 保存窗口状态
    config.windowPosition = this->pos();
    config.windowSize = this->size();
    config.windowMaximized = this->isMaximized();


    // 保存透明背景状态
    config.transparentBackground = this->testAttribute(Qt::WA_TranslucentBackground);

    // 保存标题栏状态
    config.titleBarVisible = !(this->windowFlags() & Qt::FramelessWindowHint);

    // 保存最后打开路径
    config.lastOpenPath = currentConfig.lastOpenPath;

    configManager->saveConfig(config);
    //logMessage("配置已保存");
}

// 修改 applyConfiguration 方法中的透明背景处理部分
void ImageWidget::applyConfiguration(const ConfigManager::Config &config)
{
    // 保存当前窗口状态
    bool wasMaximized = isMaximized();
    QRect normalGeometry;
    if (!wasMaximized) {
        normalGeometry = geometry();
    }

    // 第一步：设置透明背景（不影响窗口标志）
    if (config.transparentBackground) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
    }

    // 第二步：设置标题栏状态（独立于透明背景）
    Qt::WindowFlags flags = windowFlags();

    // 先移除无边框标志
    flags &= ~Qt::FramelessWindowHint;

    // 然后根据配置添加或移除
    if (!config.titleBarVisible) {
        flags |= Qt::FramelessWindowHint;
    }

    // 设置置顶状态
    if (config.alwaysOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }

    // 应用窗口标志
    if (flags != windowFlags()) {
        setWindowFlags(flags);
    }

    // === 修复：修改窗口位置和大小的应用逻辑 ===

    // 设置窗口位置（只有在不是最大化状态且有有效位置时才设置）
    if (!config.windowPosition.isNull() && !config.windowMaximized) {
        move(config.windowPosition);
    }

    // 设置窗口大小（只有在不是最大化状态且有有效大小时才设置）
    if (!config.windowSize.isEmpty() && !config.windowMaximized) {
        resize(config.windowSize);
    }

    // 设置窗口最大化状态
    if (config.windowMaximized) {
        showMaximized();
    } else {
        showNormal();

        // 如果配置中有位置和大小，但当前normalGeometry无效，使用配置的值
        if (normalGeometry.isNull() && !config.windowPosition.isNull() &&
            !config.windowSize.isEmpty()) {
            move(config.windowPosition);
            resize(config.windowSize);
        }
    }
    update();
}

// 修改 toggleTitleBar 方法
void ImageWidget::toggleTitleBar()
{
    // 保存当前窗口状态
    bool wasMaximized = isMaximized();
    QRect normalGeometry;
    if (!wasMaximized) {
        normalGeometry = geometry();
    }

    Qt::WindowFlags flags = windowFlags();

    if (flags & Qt::FramelessWindowHint) {
        // 显示标题栏
        flags &= ~Qt::FramelessWindowHint;
    } else {
        // 隐藏标题栏
        flags |= Qt::FramelessWindowHint;
    }

    setWindowFlags(flags);

    // 恢复窗口状态
    if (wasMaximized) {
        showMaximized();
    } else if (!normalGeometry.isNull()) {
        setGeometry(normalGeometry);
        showNormal();
    } else {
        showNormal();
    }

    // 保存配置
    saveConfiguration();

}

// imagewidget.cpp
void ImageWidget::setCurrentDir(const QDir &dir)
{
    currentDir = dir;
}

// imagewidget.cpp
void ImageWidget::fitToWindow()
{
    if (pixmap.isNull()) return;

    QSize windowSize = this->size();
    if (windowSize.isEmpty() || windowSize.width() <= 0 || windowSize.height() <= 0) {
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect desktopRect = screen->availableGeometry();
        windowSize = desktopRect.size();
    }

    QSize imageSize = pixmap.size();
    double widthRatio = static_cast<double>(windowSize.width()) / imageSize.width();
    double heightRatio = static_cast<double>(windowSize.height()) / imageSize.height();

    scaleFactor = qMin(widthRatio, heightRatio);
    panOffset = QPointF(0, 0);
    currentViewStateType = FitToWindow; // 设置为合适大小模式

    update();

}

void ImageWidget::actualSize()
{
    if (pixmap.isNull()) return;

    scaleFactor = 1.0;
    panOffset = QPointF(0, 0);
    currentViewStateType = ActualSize; // 设置为实际大小模式

    update();

}

// 添加压缩包相关方法
// 修改 openArchive 函数，保存之前的状态
bool ImageWidget::openArchive(const QString &filePath)
{
    if (!archiveHandler.openArchive(filePath)) {
        qDebug() << "无法打开压缩包:" << filePath;
        return false;
    }

    // 保存当前状态（压缩包外的状态）
    previousDir = currentDir;
    previousImageList = imageList;
    previousImageIndex = currentImageIndex;
    previousViewMode = currentViewMode;

    isArchiveMode = true;
    currentArchivePath = filePath;
    archiveImageCache.clear(); // 清空缓存

    // 加载压缩包中的图片列表
    loadArchiveImageList();

    // 切换到缩略图模式
    switchToThumbnailView();

    updateWindowTitle();
    qDebug() << "成功打开压缩包，包含" << imageList.size() << "个文件";
    return true;
}

// 添加退出压缩包模式的函数
void ImageWidget::exitArchiveMode()
{
    if (!isArchiveMode) return;

    qDebug() << "退出压缩包模式";

    // 关闭压缩包
    closeArchive();

    // 恢复之前的状态
    currentDir = previousDir;
    imageList = previousImageList;
    currentImageIndex = previousImageIndex;

    // 重新加载图片列表
    thumbnailWidget->setImageList(imageList, currentDir);

    // 恢复之前的视图模式
    if (previousViewMode == ThumbnailView) {
        switchToThumbnailView();
    } else {
        // 如果之前是单张模式，尝试加载对应的图片
        if (currentImageIndex >= 0 && currentImageIndex < imageList.size()) {
            loadImageByIndex(currentImageIndex);
        }
        switchToSingleView();
    }

    updateWindowTitle();
    qDebug() << "已返回到目录:" << currentDir.absolutePath();
}

void ImageWidget::closeArchive()
{
    if (isArchiveMode) {
        archiveHandler.closeArchive();
        isArchiveMode = false;
        currentArchivePath.clear();
        imageList.clear();
    }
}

void ImageWidget::loadArchiveImageList()
{
    if (!isArchiveMode) return;

    QStringList archiveImageList = archiveHandler.getImageFiles();
    archiveImageList.sort();

    // 保存原始文件名列表
    imageList = archiveImageList;

    // 构建用于缩略图显示的完整路径列表
    QStringList thumbnailPaths;
    for (const QString &fileName : std::as_const(archiveImageList)) {
        thumbnailPaths.append(currentArchivePath + "|" + fileName);
    }

    // 传递给缩略图部件
    thumbnailWidget->setImageList(thumbnailPaths, QDir());

    qDebug() << "从压缩包中找到图片文件:" << imageList.size() << "个";
    qDebug() << "缩略图路径列表:" << thumbnailPaths;
}

bool ImageWidget::loadImageFromArchive(const QString &filePath)
{
    if (!isArchiveMode) return false;

    QByteArray imageData = archiveHandler.extractFile(filePath);
    if (imageData.isEmpty()) {
        return false;
    }

    QPixmap loadedPixmap;
    if (!loadedPixmap.loadFromData(imageData)) {
        return false;
    }

    // 保存原始图片并重置变换状态
    originalPixmap = loadedPixmap;
    pixmap = loadedPixmap;

    // 重置变换状态
    rotationAngle = 0;
    isHorizontallyFlipped = false;
    isVerticallyFlipped = false;

    // 设置视图状态
    switch (currentViewStateType) {
    case FitToWindow:
        fitToWindow();
        break;
    case ActualSize:
        actualSize();
        break;
    case ManualAdjustment:
        // 保持当前的缩放和偏移
        break;
    }

    currentImagePath = currentArchivePath + "|" + filePath;

    // 确保当前图片索引正确设置
    currentImageIndex = imageList.indexOf(filePath);

    update();
    updateWindowTitle();

    return true;
}

// 载入图片
bool ImageWidget::loadImage(const QString &filePath, bool fromCache)
{
    qDebug() << "=== loadImage 开始 ===";
    qDebug() << "文件路径:" << filePath;
    qDebug() << "fromCache:" << fromCache;
    qDebug() << "isArchiveMode:" << isArchiveMode;

    // 检查是否是压缩包
    if (ArchiveHandler::isSupportedArchive(filePath)) {
        qDebug() << "检测为压缩包文件";
        return openArchive(filePath);
    }

    // 如果是压缩包模式，从压缩包加载
    if (isArchiveMode) {
        qDebug() << "压缩包模式，从压缩包加载";
        return loadImageFromArchive(filePath);
    }

    QFileInfo fileInfo(filePath);
    qDebug() << "文件是否存在:" << fileInfo.exists();
    qDebug() << "文件大小:" << fileInfo.size();
    qDebug() << "文件权限:" << fileInfo.permissions();

    if (!fileInfo.exists()) {
        qDebug() << "错误: 文件不存在";
        return false;
    }

    // 直接加载，绕过缓存进行测试
    QPixmap loadedPixmap;
    qDebug() << "开始加载图片...";

    if (!loadedPixmap.load(filePath)) {
        qDebug() << "错误: 直接加载失败";

        // 尝试使用 QImage 加载
        QImage image;
        if (image.load(filePath)) {
            qDebug() << "使用 QImage 加载成功";
            loadedPixmap = QPixmap::fromImage(image);
        } else {
            qDebug() << "错误: QImage 加载也失败";
            return false;
        }
    } else {
        qDebug() << "直接加载成功，图片尺寸:" << loadedPixmap.size();
    }

    if (loadedPixmap.isNull()) {
        qDebug() << "错误: 加载后的 pixmap 为空";
        return false;
    }

    // 继续原有逻辑...
    originalPixmap = loadedPixmap;
    pixmap = loadedPixmap;
    qDebug() << "图片设置完成";

    // 重置变换状态
    rotationAngle = 0;
    isHorizontallyFlipped = false;
    isVerticallyFlipped = false;

    // 设置视图状态
    switch (currentViewStateType) {
    case FitToWindow:
        fitToWindow();
        break;
    case ActualSize:
        actualSize();
        break;
    case ManualAdjustment:
        // 保持当前的缩放和偏移
        break;
    }

    currentImagePath = filePath;
    qDebug() << "当前图片路径设置为:" << currentImagePath;

    // 检查目录是否改变
    bool dirChanged = (currentDir != fileInfo.absoluteDir());
    if (dirChanged) {
        currentDir = fileInfo.absoluteDir();
        loadImageList();
    }

    // 确保当前图片索引正确设置
    currentImageIndex = imageList.indexOf(fileInfo.fileName());
    qDebug() << "当前图片索引:" << currentImageIndex;

    update();
    updateWindowTitle();
    qDebug() << "=== loadImage 完成 ===";

    return true;
}

// 载入图片列表
void ImageWidget::loadImageList()
{
    QStringList newImageList;

    QFileInfoList fileList = currentDir.entryInfoList(QDir::Files);
    QStringList imageFilters = {"*.png",  "*.jpg", "*.bmp",  "*.jpeg",
                                "*.webp", "*.gif", "*.tiff", "*.tif"};
    QStringList archiveFilters = {"*.zip", "*.rar", "*.7z", "*.tar",
                                  "*.gz",  "*.bz2"}; // 添加压缩包过滤器

    foreach (const QFileInfo &fileInfo, fileList) {
        bool isImage = false;
        foreach (const QString &filter, imageFilters) {
            if (fileInfo.fileName().endsWith(filter.mid(1), Qt::CaseInsensitive)) {
                newImageList.append(fileInfo.fileName());
                isImage = true;
                break;
            }
        }

        // 如果不是图片，检查是否是压缩包
        if (!isImage) {
            foreach (const QString &filter, archiveFilters) {
                if (fileInfo.fileName().endsWith(filter.mid(1),
                                                 Qt::CaseInsensitive)) {
                    newImageList.append(fileInfo.fileName());
                    break;
                }
            }
        }
    }

    newImageList.sort();

    // 只有当文件列表实际发生变化时才更新和输出日志
    if (newImageList != imageList) {
        imageList = newImageList;
        thumbnailWidget->setImageList(imageList, currentDir);
        qDebug() << "找到文件:" << imageList.size() << "个（包含图片和压缩包）";
    }
}

// 载入图片指定
bool ImageWidget::loadImageByIndex(int index, bool fromCache)
{
    if (imageList.isEmpty() || index < 0 || index >= imageList.size()) {
        return false;
    }

    bool result = false;

    if (isArchiveMode) {
        // 压缩包模式：使用内部文件名
        QString imagePath = imageList.at(index);
        result = loadImageFromArchive(imagePath);

        // 更新当前图片路径为压缩包路径 + 内部文件路径
        if (result) {
            currentImagePath = currentArchivePath + "|" + imagePath;
        }
    } else {
        // 普通文件模式：构建完整文件路径
        QString imagePath = currentDir.absoluteFilePath(imageList.at(index));
        result = loadImage(imagePath, fromCache);
    }

    // 更新当前索引
    if (result) {
        currentImageIndex = index;

        // 如果当前是缩略图模式，更新选中项
        if (currentViewMode == ThumbnailView) {
            thumbnailWidget->setSelectedIndex(currentImageIndex);
        }

        // 预加载下一张图片（用于幻灯片）
        if (isSlideshowActive) {
            int nextIndex = (currentImageIndex + 1) % imageList.size();

            if (isArchiveMode) {
                // 压缩包模式预加载
                QString nextPath = imageList.at(nextIndex);
                if (!archiveImageCache.contains(nextPath)) {
                    QtConcurrent::run([this, nextIndex]() {
                        QString nextPath = imageList.at(nextIndex);
                        QByteArray imageData = archiveHandler.extractFile(nextPath);
                        if (!imageData.isEmpty()) {
                            QPixmap tempPixmap;
                            if (tempPixmap.loadFromData(imageData)) {
                                QMutexLocker locker(&cacheMutex);
                                archiveImageCache.insert(nextPath, tempPixmap);
                                qDebug() << "预加载压缩包图片:" << nextPath;
                            }
                        }
                    });
                }
            } else {
                // 普通文件模式预加载
                QString nextPath =
                    currentDir.absoluteFilePath(imageList.at(nextIndex));
                if (!imageCache.contains(nextPath)) {
                    QtConcurrent::run([this, nextIndex]() {
                        QString nextPath =
                            currentDir.absoluteFilePath(imageList.at(nextIndex));
                        QPixmap tempPixmap;
                        if (tempPixmap.load(nextPath)) {
                            QMutexLocker locker(&cacheMutex);
                            imageCache.insert(nextPath, tempPixmap);
                        }
                    });
                }
            }
        }
    }

    return result;
}

// 载入下一张
void ImageWidget::loadNextImage()
{
    qDebug() << "=== loadNextImage 开始 ===";
    qDebug() << "当前模式:" << (currentViewMode == SingleView ? "单张" : "缩略图");
    qDebug() << "当前索引:" << currentImageIndex << "，图片总数:" << imageList.size();

    if (imageList.isEmpty()) {
        qDebug() << "图片列表为空，返回";
        return;
    }

    int nextIndex = (currentImageIndex + 1) % imageList.size();
    qDebug() << "计算出的下一个索引:" << nextIndex;

    if (currentViewMode == SingleView) {
        qDebug() << "单张模式，加载图片";
        loadImageByIndex(nextIndex, true);
    } else {
        // 缩略图模式下，只更新索引和选中状态
        qDebug() << "缩略图模式，更新选中状态";
        currentImageIndex = nextIndex;
        thumbnailWidget->setSelectedIndex(currentImageIndex);
        thumbnailWidget->ensureVisible(currentImageIndex);
        updateWindowTitle();

        qDebug() << "更新后的当前索引:" << currentImageIndex;
    }

    qDebug() << "=== loadNextImage 结束 ===";
}

//载入前一张
void ImageWidget::loadPreviousImage()
{
    qDebug() << "=== loadPreviousImage 开始 ===";
    qDebug() << "当前模式:" << (currentViewMode == SingleView ? "单张" : "缩略图");
    qDebug() << "当前索引:" << currentImageIndex << "，图片总数:" << imageList.size();

    if (imageList.isEmpty()) {
        qDebug() << "图片列表为空，返回";
        return;
    }

    int prevIndex = (currentImageIndex - 1 + imageList.size()) % imageList.size();
    qDebug() << "计算出的上一个索引:" << prevIndex;

    if (currentViewMode == SingleView) {
        qDebug() << "单张模式，加载图片";
        loadImageByIndex(prevIndex, true);
    } else {
        // 缩略图模式下，只更新索引和选中状态
        qDebug() << "缩略图模式，更新选中状态";
        currentImageIndex = prevIndex;
        thumbnailWidget->setSelectedIndex(currentImageIndex);
        thumbnailWidget->ensureVisible(currentImageIndex);
        updateWindowTitle();

        qDebug() << "更新后的当前索引:" << currentImageIndex;
    }

    qDebug() << "=== loadPreviousImage 结束 ===";
}

void ImageWidget::preloadAllImages()
{
    imageCache.clear();

    int loadedCount = 0;
    for (const QString &fileName : imageList) {
        QString filePath = currentDir.absoluteFilePath(fileName);
        QPixmap tempPixmap;
        if (tempPixmap.load(filePath)) {
            imageCache.insert(filePath, tempPixmap);
            loadedCount++;
            // 移除单条日志消息，减少干扰
        }
    }

    updateWindowTitle();
}

void ImageWidget::clearImageCache()
{
    int cacheSize = imageCache.size();
    imageCache.clear();
    updateWindowTitle();
}

void ImageWidget::startSlideshow()
{
    if (imageList.size() > 1) {
        // 不再预加载所有图片，改为按需加载
        isSlideshowActive = true;
        slideshowTimer->start(slideshowInterval);
        updateWindowTitle();
        // 移除日志消息，避免干扰
    }
}

void ImageWidget::stopSlideshow()
{
    isSlideshowActive = false;
    slideshowTimer->stop();
    updateWindowTitle();
}

void ImageWidget::toggleSlideshow()
{
    if (isSlideshowActive) {
        stopSlideshow();
    } else {
        startSlideshow();
    }
}

// imagewidget.cpp
void ImageWidget::setSlideshowInterval(int interval)
{
    slideshowInterval = interval;
    if (isSlideshowActive) {
        slideshowTimer->setInterval(slideshowInterval);
    }
}

void ImageWidget::slideshowNext()
{
    if (imageList.isEmpty()) {
        stopSlideshow();
        return;
    }

    int nextIndex = (currentImageIndex + 1) % imageList.size();

    // 预加载下一张图片（如果不在缓存中）
    if (!imageCache.contains(currentDir.absoluteFilePath(imageList.at(nextIndex)))) {
        QtConcurrent::run([this, nextIndex]() {
            QString nextPath = currentDir.absoluteFilePath(imageList.at(nextIndex));
            QPixmap tempPixmap;
            if (tempPixmap.load(nextPath)) {
                QMutexLocker locker(&cacheMutex);
                imageCache.insert(nextPath, tempPixmap);
            }
        });
    }

    // 加载当前图片
    loadImageByIndex(nextIndex, true);
}

void ImageWidget::updateWindowTitle()
{
    QString title;

    if (canvasMode) {
        // 画布模式下的标题
        title = tr("画布模式 - 按 Insert 或 ESC 退出");
        if (!currentImagePath.isEmpty()) {
            QFileInfo fileInfo(currentImagePath);
            title += QString(" - %1").arg(fileInfo.fileName());
        }
    } else if (!currentImagePath.isEmpty()) {
        QFileInfo fileInfo(currentImagePath);
        title = QString(tr("%1 (%2/%3) - %4模式"))
                    .arg(fileInfo.fileName())
                    .arg(currentImageIndex + 1)
                    .arg(imageList.size())
                    .arg(currentViewMode == SingleView ? tr("单张") : tr("缩略图"));
    } else if (!pixmap.isNull()) {
        title = tr("剪贴板图片");
    } else {
        title = tr("图片查看器 - 缩略图模式");
    }

    if (isSlideshowActive) {
        title += " [幻灯中]";
    }

    setWindowTitle(title);
}

QString ImageWidget::getShortPathName(const QString &longPath)
{
    wchar_t shortPath[MAX_PATH];
    if (GetShortPathName(longPath.toStdWString().c_str(), shortPath, MAX_PATH) > 0) {
        return QString::fromWCharArray(shortPath);
    }
    return longPath;
}

void ImageWidget::registerFileAssociation(const QString &fileExtension,
                                          const QString &fileTypeName,
                                          const QString &openCommand) {
    QString keyName = QString(".%1").arg(fileExtension);
    HKEY hKey;
    if (RegCreateKeyEx(
            HKEY_CLASSES_ROOT, reinterpret_cast<const wchar_t *>(keyName.utf16()),
            0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                      reinterpret_cast<const BYTE *>(fileTypeName.utf16()),
                      (fileTypeName.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    keyName = QString("%1").arg(fileTypeName);
    if (RegCreateKeyEx(
            HKEY_CLASSES_ROOT, reinterpret_cast<const wchar_t *>(keyName.utf16()),
            0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                      reinterpret_cast<const BYTE *>(fileTypeName.utf16()),
                      (fileTypeName.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    keyName = QString("%1\\shell\\open\\command").arg(fileTypeName);
    if (RegCreateKeyEx(
            HKEY_CLASSES_ROOT, reinterpret_cast<const wchar_t *>(keyName.utf16()),
            0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                      reinterpret_cast<const BYTE *>(openCommand.utf16()),
                      (openCommand.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
}

void ImageWidget::switchToThumbnailView()
{
    currentViewMode = ThumbnailView;
    scrollArea->show();
    scrollArea->raise();

    // 确保缩略图部件获得焦点
    thumbnailWidget->setFocus();

    // 设置当前选中的缩略图索引
    if (currentImageIndex >= 0 && currentImageIndex < imageList.size()) {
        qDebug() << "切换到缩略图模式，设置选中索引:" << currentImageIndex;
        thumbnailWidget->setSelectedIndex(currentImageIndex);

        // 延迟确保滚动位置正确
        QTimer::singleShot(50, [this]() {
            thumbnailWidget->ensureVisible(currentImageIndex);
        });
    } else if (!imageList.isEmpty()) {
        currentImageIndex = 0;
        qDebug() << "切换到缩略图模式，设置默认选中索引:" << currentImageIndex;
        thumbnailWidget->setSelectedIndex(0);

        // 延迟确保滚动位置正确
        QTimer::singleShot(50, [this]() {
            thumbnailWidget->ensureVisible(0);
        });
    } else {
        qDebug() << "切换到缩略图模式，无图片可选中";
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

    update();
}

void ImageWidget::openFolder()
{
    QString initialPath = currentConfig.lastOpenPath;
    if (initialPath.isEmpty() || !QDir(initialPath).exists()) {
        initialPath =
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    QString folderPath = QFileDialog::getExistingDirectory(
        this, tr("选择图片文件夹"), initialPath);

    if (!folderPath.isEmpty()) {
        // 更新最后打开路径
        currentConfig.lastOpenPath = folderPath;
        saveConfiguration(); // 立即保存配置

        // 重置为合适大小模式
        currentViewStateType = FitToWindow;

        currentDir = QDir(folderPath);
        loadImageList();

        // 确保切换到缩略图模式
        switchToThumbnailView(); // 使用统一的切换函数

        currentImageIndex = -1;

        if (!imageList.isEmpty()) {
            currentImageIndex = 0;
            thumbnailWidget->setSelectedIndex(0);
            qDebug() << "设置选中索引为 0，图片列表大小:" << imageList.size();
        } else {
            currentImageIndex = -1;
            qDebug() << "图片列表为空，选中索引保持为 -1";
        }

        // 确保窗口正常显示
        ensureWindowVisible();

        update();
    }
}

// 添加新的辅助函数来确保窗口可见
void ImageWidget::ensureWindowVisible()
{
    // 如果窗口最小化，恢复显示
    if (isMinimized()) {
        showNormal();
    }

    // 确保窗口可见
    show();
    raise();
    activateWindow();

    // 设置焦点到缩略图部件
    thumbnailWidget->setFocus();
}


void ImageWidget::openImage()
{
    QString initialPath = currentConfig.lastOpenPath;
    if (initialPath.isEmpty() || !QDir(initialPath).exists()) {
        initialPath =
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    QString fileName = QFileDialog::getOpenFileName(
        this, tr("打开图片"), initialPath,
        "Images (*.png *.jpg *.bmp *.jpeg *.webp *.gif *.tiff *.tif)");

    if (!fileName.isEmpty()) {

        // 更新最后打开路径（使用图片所在文件夹）
        QFileInfo fileInfo(fileName);
        currentConfig.lastOpenPath = fileInfo.absolutePath();
        saveConfiguration(); // 立即保存配置

        loadImage(fileName);
        switchToSingleView();
    }
}

// imagewidget.cpp - 修改paintEvent函数
void ImageWidget::paintEvent(QPaintEvent *event)
{
    if (currentViewMode == SingleView) {
        Q_UNUSED(event);
        QPainter painter(this);

        // 如果启用了透明背景，使用透明颜色填充
        if (testAttribute(Qt::WA_TranslucentBackground)) {
            painter.fillRect(rect(), QColor(0, 0, 0, 0));
        } else {
            // 非透明背景时填充黑色
            painter.fillRect(rect(), Qt::black);
        }

        // 安全检查：确保pixmap有效
        if (pixmap.isNull()) {
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, tr("没有图片或图片加载失败"));
            return;
        }

        // 安全检查：确保scaleFactor有效
        if (scaleFactor <= 0) {
            scaleFactor = 1.0;
        }

        QSize scaledSize = pixmap.size() * scaleFactor;

        // 安全检查：确保缩放后的尺寸有效
        if (scaledSize.width() <= 0 || scaledSize.height() <= 0) {
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, tr("图片尺寸无效"));
            return;
        }

        qDebug() << "绘制图片 - 原始尺寸:" << pixmap.size() << "缩放后:" << scaledSize;

        QPixmap scaledPixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 安全检查：确保缩放后的pixmap有效
        if (scaledPixmap.isNull()) {
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, tr("图片缩放失败"));
            return;
        }

        QPointF offset((width() - scaledPixmap.width()) / 2 + panOffset.x(),
                       (height() - scaledPixmap.height()) / 2 + panOffset.y());

        painter.drawPixmap(offset, scaledPixmap);

        // 添加变换状态提示
        if (isTransformed()) {
            painter.setPen(Qt::yellow);
            painter.setFont(QFont("Arial", 10));

            QString transformInfo;
            if (rotationAngle != 0) {
                transformInfo += QString("旋转: %1° ").arg(rotationAngle);
            }
            if (isHorizontallyFlipped) {
                transformInfo += "水平镜像 ";
            }
            if (isVerticallyFlipped) {
                transformInfo += "垂直镜像 ";
            }

            if (!transformInfo.isEmpty()) {
                painter.drawText(10, 30, transformInfo.trimmed());
            }
        }

        // 添加左右箭头提示（半透明，只在图片较大时显示）
        if (scaledSize.width() > 600 && scaledSize.height() > 600) {
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setOpacity(0.5); // 半透明

            // 左箭头
            QPainterPath leftArrow;
            leftArrow.moveTo(offset.x() + 20, offset.y() + scaledSize.height() / 2);
            leftArrow.lineTo(offset.x() + 40, offset.y() + scaledSize.height() / 2 - 15);
            leftArrow.lineTo(offset.x() + 40, offset.y() + scaledSize.height() / 2 + 15);
            leftArrow.closeSubpath();
            painter.fillPath(leftArrow, Qt::white);

            // 右箭头
            QPainterPath rightArrow;
            rightArrow.moveTo(offset.x() + scaledSize.width() - 20, offset.y() + scaledSize.height() / 2);
            rightArrow.lineTo(offset.x() + scaledSize.width() - 40, offset.y() + scaledSize.height() / 2 - 15);
            rightArrow.lineTo(offset.x() + scaledSize.width() - 40, offset.y() + scaledSize.height() / 2 + 15);
            rightArrow.closeSubpath();
            painter.fillPath(rightArrow, Qt::white);

            painter.setOpacity(1.0); // 恢复不透明
        }

        qDebug() << "图片绘制完成";
    }
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
            qDebug() << "成功打开压缩包:" << filePath;
        } else {
            qDebug() << "打开压缩包失败:" << filePath;
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

QPixmap ImageWidget::getArchiveThumbnail(const QString &archivePath)
{
    qDebug() << "获取压缩包缩略图:" << archivePath;

    // 解析路径格式：压缩包路径|内部文件路径
    if (!archivePath.contains("|")) {
        qDebug() << "路径格式错误，不包含 | 分隔符";
        return QPixmap();
    }

    QStringList parts = archivePath.split("|");
    if (parts.size() != 2) {
        qDebug() << "路径格式错误，分割后不是2部分";
        return QPixmap();
    }

    QString archiveFile = parts[0];
    QString internalFile = parts[1];

    qDebug() << "解析结果 - 压缩包:" << archiveFile << "内部文件:" << internalFile;

    // 如果已经在缓存中，直接返回
    if (archiveImageCache.contains(internalFile)) {
        qDebug() << "从缓存中获取缩略图:" << internalFile;
        return archiveImageCache.value(internalFile);
    }

    qDebug() << "从压缩包提取缩略图:" << internalFile;

    // 从压缩包提取图片
    if (archiveHandler.openArchive(archiveFile)) {
        QByteArray imageData = archiveHandler.extractFile(internalFile);

        // 不要立即关闭压缩包，因为可能还有其他操作
        // archiveHandler.closeArchive();

        if (!imageData.isEmpty()) {
            qDebug() << "成功提取图片数据，大小:" << imageData.size();

            QPixmap pixmap;
            if (pixmap.loadFromData(imageData)) {
                // 缩放到缩略图大小
                QPixmap thumbnail =
                    pixmap.scaled(thumbnailSize, Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation);

                QMutexLocker locker(&cacheMutex);
                archiveImageCache.insert(internalFile, thumbnail);

                qDebug() << "成功加载缩略图，尺寸:" << thumbnail.size();
                return thumbnail;
            } else {
                qDebug() << "从数据加载图片失败";
            }
        } else {
            qDebug() << "提取的图片数据为空";
        }
    } else {
        qDebug() << "无法打开压缩包:" << archiveFile;
    }

    return QPixmap();
}

// 添加辅助函数
bool ImageWidget::isArchiveFile(const QString &fileName) const
{
    QString lowerName = fileName.toLower();
    return (lowerName.endsWith(".zip") || lowerName.endsWith(".rar") ||
            lowerName.endsWith(".7z") || lowerName.endsWith(".tar") ||
            lowerName.endsWith(".gz") || lowerName.endsWith(".bz2"));
}

void ImageWidget::onEnsureRectVisible(const QRect &rect)
{
    // 使用 ensureVisible 确保矩形区域可见
    scrollArea->ensureVisible(rect.x(), rect.y(), rect.width(), rect.height());

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

    // 更新窗口标题显示当前选中位置
    updateWindowTitle();
}



void ImageWidget::toggleAlwaysOnTop()
{
    if (windowFlags() & Qt::WindowStaysOnTopHint) {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    }
    show();
}

// 修改 toggleTransparentBackground 方法，更彻底地处理透明背景
void ImageWidget::toggleTransparentBackground()
{
    bool currentState = testAttribute(Qt::WA_TranslucentBackground);

    if (currentState) {
        // 关闭透明背景
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);

        qDebug("透明背景 关闭");
    } else {
        // 启用透明背景
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);

        qDebug("透明背景 开启");
    }

    // 强制重绘
    update();

    // 保存配置
    saveConfiguration();

}

void ImageWidget::copyImageToClipboard()
{
    if (!pixmap.isNull()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setPixmap(pixmap);
    }
}

void ImageWidget::pasteImageFromClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    if (mimeData->hasImage()) {
        QImage image = clipboard->image();
        if (!image.isNull()) {
            pixmap = QPixmap::fromImage(image);
            scaleFactor = 1.0;
            panOffset = QPointF(0, 0);
            currentImagePath.clear();
            currentImageIndex = -1;
            imageList.clear();
            update();
            updateWindowTitle();
        }
    }
}

void ImageWidget::saveImage()
{
    if (pixmap.isNull()) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this, tr("保存图片"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "Images (*.png *.jpg *.bmp *.jpeg *.webp)");
    if (!fileName.isEmpty()) {
        if (pixmap.save(fileName)) {

        } else {
        }
    }
}



//右键菜单
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
        contextMenu.addAction(tr("返回缩略图模式"), this,
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

        // === 新增：编辑菜单 ===
        QMenu *editMenu = contextMenu.addMenu(tr("编辑"));

        // 旋转子菜单
        QMenu *rotateSubMenu = editMenu->addMenu(tr("旋转"));

        rotateSubMenu->addAction(tr("逆时针90° (PageUp)"), this,
                                 &ImageWidget::rotate90CCW);
        rotateSubMenu->addAction(tr("顺时针90° (PageDown)"), this,
                                 &ImageWidget::rotate90CW);
        rotateSubMenu->addAction(tr("180°"), this, &ImageWidget::rotate180);

        // 镜像子菜单
        QMenu *mirrorSubMenu = editMenu->addMenu(tr("镜像"));
        mirrorSubMenu->addAction(tr("垂直镜像 (Ctrl+PageUp)"), this,
                                 &ImageWidget::mirrorVertical);
        mirrorSubMenu->addAction(tr("水平镜像 (Ctrl+PageDown)"), this,
                                 &ImageWidget::mirrorHorizontal);

        // 重置变换
        if (isTransformed()) {
            editMenu->addSeparator();
            editMenu->addAction(tr("重置变换 (Ctrl+0)"), this,
                                &ImageWidget::resetTransform);
        }

        contextMenu.addSeparator();
        // === 编辑菜单结束 ===

        // 添加删除菜单项
        contextMenu.addAction(tr("删除当前图片 (Del)"), this,
                              &ImageWidget::deleteCurrentImage);
        // contextMenu.addAction(tr("永久删除"), this,
        // &ImageWidget::permanentlyDeleteCurrentImage);
        contextMenu.addSeparator();

    } else {
        // 缩略图模式下的菜单
        int selectedIndex = thumbnailWidget->getSelectedIndex();
        if (selectedIndex >= 0 && selectedIndex < imageList.size()) {
            contextMenu.addAction(tr("删除选中 (Del)"), this,
                                  &ImageWidget::deleteSelectedThumbnail);
            contextMenu.addSeparator();
        }
    }

    // 打开文件 整合
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

    // 窗口控制
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
                                   : tr("透明背景（需隐藏标题）"));
    connect(windowshowAction3, &QAction::triggered, this,
            &ImageWidget::toggleTransparentBackground);

    // 添加透明度子菜单
    QMenu *opacitySubMenu = windowshowMenu->addMenu(tr("透明度"));
    // 创建透明度选项
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

    // 连接透明度选项到对应的槽函数
    connect(opacity100, &QAction::triggered,
            [this]() { setWindowOpacityValue(1.0); });
    connect(opacity90, &QAction::triggered,
            [this]() { setWindowOpacityValue(0.9); });
    connect(opacity80, &QAction::triggered,
            [this]() { setWindowOpacityValue(0.8); });
    connect(opacity70, &QAction::triggered,
            [this]() { setWindowOpacityValue(0.7); });
    connect(opacity60, &QAction::triggered,
            [this]() { setWindowOpacityValue(0.6); });
    connect(opacity50, &QAction::triggered,
            [this]() { setWindowOpacityValue(0.5); });
    connect(opacity40, &QAction::triggered,
            [this]() { setWindowOpacityValue(0.4); });
    connect(opacity30, &QAction::triggered,
            [this]() { setWindowOpacityValue(0.3); });
    connect(opacity20, &QAction::triggered,
            [this]() { setWindowOpacityValue(0.2); });
    connect(opacity10, &QAction::triggered,
            [this]() { setWindowOpacityValue(0.1); });

    if (currentViewMode == SingleView) {
        // 在画布模式下显示不同的菜单项
        if (canvasMode) {
            QAction *exitCanvasAction =
                windowshowMenu->addAction(tr("退出画布模式 (ESC/Insert)"));
            connect(exitCanvasAction, &QAction::triggered, this,
                    &ImageWidget::disableCanvasMode);
            // 添加透明度调节提示
            windowshowMenu->addSeparator();
            windowshowMenu->addAction(tr("增加透明度 (PageUp)"));
            windowshowMenu->addAction(tr("减少透明度 (PageDown)"));
        } else {
            // 画布模式进入
            QAction *canvasModeAction =
                windowshowMenu->addAction(tr("进入画布模式 (Insert)"));
            connect(canvasModeAction, &QAction::triggered, this,
                    &ImageWidget::toggleCanvasMode);
        }
        // contextMenu.addSeparator();
    }

    // 幻灯
    QMenu *slideshowMenu = contextMenu.addMenu(tr("幻灯功能"));
    // 幻灯 子菜单1
    QAction *slideshowAction = slideshowMenu->addAction(
        isSlideshowActive ? tr("停止幻灯") : tr("开始幻灯"));
    connect(slideshowAction, &QAction::triggered, this,
            &ImageWidget::toggleSlideshow);
    // 分隔符
    slideshowMenu->addSeparator();
    //幻灯 子菜单2
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

    connect(interval1s, &QAction::triggered,
            [this]() { setSlideshowInterval(1000); });
    connect(interval2s, &QAction::triggered,
            [this]() { setSlideshowInterval(2000); });
    connect(interval3s, &QAction::triggered,
            [this]() { setSlideshowInterval(3000); });
    connect(interval5s, &QAction::triggered,
            [this]() { setSlideshowInterval(5000); });
    connect(interval10s, &QAction::triggered,
            [this]() { setSlideshowInterval(10000); });

    // 帮助菜单
    QMenu *helpMenu = contextMenu.addMenu(tr("帮助"));
    QAction *aboutAction = helpMenu->addAction(tr("关于 (F1)"));
    connect(aboutAction, &QAction::triggered, this,
            &ImageWidget::showAboutDialog);

    QAction *shortcutHelpAction = helpMenu->addAction(tr("快捷键帮助"));
    connect(shortcutHelpAction, &QAction::triggered, this,
            &ImageWidget::showShortcutHelp);

    contextMenu.addSeparator();
    contextMenu.addAction(tr("退出"), this, &QWidget::close);

    contextMenu.exec(globalPos);
}

// 设置窗口透明度
void ImageWidget::setWindowOpacityValue(double opacity)
{
    // 限制透明度范围在 0.1 到 1.0 之间
    opacity = qBound(0.1, opacity, 1.0);
    m_windowOpacity = opacity;

    // 设置窗口透明度
    this->setWindowOpacity(opacity);

}

bool ImageWidget::isAlwaysOnTop() const
{
    return windowFlags() & Qt::WindowStaysOnTopHint;
}

bool ImageWidget::hasTitleBar() const
{
    // 根据你的实现返回标题栏状态
    return !(windowFlags() & Qt::FramelessWindowHint);
}

bool ImageWidget::hasTransparentBackground() const
{
    // 根据你的实现返回透明背景状态
    // 实际检查透明背景状态
    return this->testAttribute(Qt::WA_TranslucentBackground);
}

// imagewidget.cpp
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

// imagewidget.cpp
void ImageWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
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

// imagewidget.cpp
void ImageWidget::wheelEvent(QWheelEvent *event)
{
    if (currentViewMode == SingleView) {
        // 标记为手动调整
        currentViewStateType = ManualAdjustment;

        double zoomFactor = 1.15;
        double oldScaleFactor = scaleFactor;

        if (event->angleDelta().y() > 0) {
            scaleFactor *= zoomFactor;
        } else {
            scaleFactor /= zoomFactor;
        }

        scaleFactor = qBound(0.03, scaleFactor, 8.0);

        QPointF mousePos = event->position();
        QPointF imagePos =
            (mousePos - QPointF(width() / 2, height() / 2) - panOffset) /
                           oldScaleFactor;
        panOffset = mousePos - QPointF(width() / 2, height() / 2) -
                    imagePos * scaleFactor;

        update();
    }
}

void ImageWidget::keyPressEvent(QKeyEvent *event)
{
    qDebug()<<"key:"<<event->key();

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
    // 在画布模式下，处理特定按键
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
        case Qt::Key_PageUp:
            // PageUp：增加透明度（变得更不透明）
            {
                double currentOpacity = windowOpacity(); // 正确调用
                double newOpacity = qMin(1.0, currentOpacity + 0.1);
                setWindowOpacity(newOpacity);
                event->accept();
            }
            break;
        case Qt::Key_PageDown:
            // PageDown：减少透明度（变得更透明）
            {
                double currentOpacity = windowOpacity(); // 正确调用
                double newOpacity = qMax(0.1, currentOpacity - 0.1);
                setWindowOpacity(newOpacity);
                event->accept();
            }
            break;
        default:
            // 忽略其他所有按键
            event->ignore();
            break;
        }
        return;
    }

    if (currentViewMode == SingleView) {//单张图片模式
        switch (event->key()) {
            qDebug()<<"s_key:"<<event->key();
        case Qt::Key_Left:
            loadPreviousImage();
            break;
        case Qt::Key_Right:
            loadNextImage();
            break;
        case Qt::Key_Up: // 上箭头 - 合适大小
            fitToWindow();
            break;
        case Qt::Key_Down: // 下箭头 - 实际大小
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

                // Ctrl+PageUp：水平镜像
                mirrorVertical();
            } else {
                // PageUp：顺时针旋转90度
                rotate90CCW();
            }
            event->accept();
            break;
        case Qt::Key_PageDown:
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+PageDown：垂直镜像
                mirrorHorizontal();
            } else {

                // PageDown：逆时针旋转90度
                rotate90CW();
            }
            event->accept();
            break;

        case Qt::Key_Escape:
            switchToThumbnailView();
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
            // Insert键：进入画布模式
            enableCanvasMode();
            break;

        // 添加回车键切换模式功能
        case Qt::Key_Enter:
        case Qt::Key_Return:
            switchToThumbnailView();
            break;
        case Qt::Key_Menu: //菜单键
            showContextMenu(this->mapToGlobal(QPoint(width()/2, height()/2)));
            break;

        case Qt::Key_Delete:
            if (currentViewMode == SingleView) {
                deleteCurrentImage(); // 移动到回收站
            } else {
                deleteSelectedThumbnail(); // 移动到回收站
            }
            break;

        default:
            QWidget::keyPressEvent(event);
        }
    } else { //缩略图模式
        // 缩略图模式下的按键处理
        switch (event->key()) {
        case Qt::Key_Up:
            // 上键：向上滚动
            {
                QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
                int newValue = vScrollBar->value() - vScrollBar->singleStep();
                vScrollBar->setValue(newValue);
                event->accept();
            }
            break;

        case Qt::Key_Down:
            // 下键：向下滚动
            {
                QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
                int newValue = vScrollBar->value() + vScrollBar->singleStep();
                vScrollBar->setValue(newValue);
                event->accept();
            }
            break;

        case Qt::Key_PageUp:
            // PageUp：向上翻页
            {
                QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
                int newValue = vScrollBar->value() - vScrollBar->pageStep();
                vScrollBar->setValue(newValue);
                event->accept();
            }
            break;

        case Qt::Key_PageDown:
            // PageDown：向下翻页
            {
                QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
                int newValue = vScrollBar->value() + vScrollBar->pageStep();
                vScrollBar->setValue(newValue);
                event->accept();
            }
            break;

        case Qt::Key_Home:
            // Home：滚动到顶部并选中第一个缩略图
            if (!imageList.isEmpty()) {
                scrollArea->verticalScrollBar()->setValue(0);
                currentImageIndex = 0;
                thumbnailWidget->setSelectedIndex(0);
                updateWindowTitle();
            }
            event->accept();
            break;

        case Qt::Key_End:
            // End：滚动到底部并选中最后一个缩略图
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
            // 回车：打开选中的图片
            if (currentImageIndex >= 0) {
                switchToSingleView(currentImageIndex);
            }
            event->accept();
            break;

        case Qt::Key_Delete:
            // 删除选中的缩略图
            deleteSelectedThumbnail();
            event->accept();
            break;

        case Qt::Key_Escape:
            // ESC：关闭程序
            close();
            event->accept();
            break;

        case Qt::Key_Menu:
            // 菜单键：显示上下文菜单
            showContextMenu(this->mapToGlobal(QPoint(width()/2, height()/2)));
            event->accept();
            break;

        case Qt::Key_Left:
        case Qt::Key_Right:
            // 左右方向键：由 ThumbnailWidget 处理，这里不做处理
            event->ignore();
            break;

        default:
            QWidget::keyPressEvent(event);
        }
    }
}

void ImageWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void ImageWidget::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (urlList.isEmpty()) {
            return;
        }

        // 只处理第一个拖拽项
        QString filePath = urlList.first().toLocalFile();
        //logMessage("拖拽消息:" + filePath);

        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {

            return;
        }

        if (fileInfo.isDir()) {

            // 更新最后打开路径（使用图片所在文件夹）
            currentConfig.lastOpenPath = fileInfo.absolutePath();
            saveConfiguration();

            // 处理文件夹拖拽 - 切换到缩略图模式
            currentDir = QDir(filePath);
            loadImageList();

            // 无论当前是什么模式，都切换到缩略图模式
            currentViewMode = ThumbnailView;
            scrollArea->show();
            currentImageIndex = -1;
            update();
            //logMessage("打开文件夹:" + filePath);
        } else if (fileInfo.isFile()) {
            // 处理文件拖拽
            if (loadImage(filePath)) {

                // 更新最后打开路径（使用图片所在文件夹）
                currentConfig.lastOpenPath = fileInfo.absolutePath();
                saveConfiguration();

                // 如果是单张模式，保持单张模式；如果是缩略图模式，切换到单张模式
                if (currentViewMode == ThumbnailView) {
                    switchToSingleView();
                } else {
                    update();
                }
                updateWindowTitle();
                //logMessage("打开文件:" + filePath);
            } else {
                ////logMessage("打开文件失败:" + filePath);
            }
        }

        event->acceptProposedAction();
    }
}

void ImageWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (currentViewMode == ThumbnailView) {
        thumbnailWidget->update();
    }
}

// imagewidget.cpp
void ImageWidget::testKeyboard()
{
    qDebug() << "=== 键盘测试开始 ===";
    qDebug() << "当前焦点部件:"
             << (QApplication::focusWidget()
                     ? QApplication::focusWidget()->objectName()
                     : "无");
    qDebug() << "缩略图部件焦点状态:" << thumbnailWidget->hasFocus();
    qDebug() << "当前模式:"
             << (currentViewMode == SingleView ? "单张" : "缩略图");
    qDebug() << "图片列表大小:" << imageList.size();
    qDebug() << "当前选中索引:" << currentImageIndex;
    qDebug() << "=== 键盘测试结束 ===";
}


void ImageWidget::deleteCurrentImage()
{
    if (currentImagePath.isEmpty() || !QFile::exists(currentImagePath)) {
        QMessageBox::warning(this, tr("警告"), tr("没有可删除的图片"));
        return;
    }

    // 确认对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("确认删除"),
                                  tr("确定要将图片 '") +
                                      QFileInfo(currentImagePath).fileName() +
                                      tr("' 移动到回收站吗？"),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString imageToDelete = currentImagePath;
    int indexToDelete = currentImageIndex;

    // 移动到回收站而不是直接删除
    if (moveFileToRecycleBin(imageToDelete)) {
        //logMessage("已将图片移动到回收站: " + imageToDelete);

        // 从缓存中移除
        imageCache.remove(imageToDelete);

        // 从缩略图缓存中移除
        ThumbnailWidget::clearThumbnailCacheForImage(imageToDelete);

        // 从图片列表中移除
        if (indexToDelete >= 0 && indexToDelete < imageList.size()) {
            imageList.removeAt(indexToDelete);

            // 更新缩略图
            thumbnailWidget->setImageList(imageList, currentDir);

            // 确定下一张要显示的图片
            if (imageList.isEmpty()) {
                // 如果没有图片了
                pixmap = QPixmap();
                currentImagePath.clear();
                currentImageIndex = -1;
                if (currentViewMode == SingleView) {
                    switchToThumbnailView();
                }
            } else {
                // 计算新的索引
                int newIndex = indexToDelete;
                if (newIndex >= imageList.size()) {
                    newIndex = imageList.size() - 1;
                }

                if (currentViewMode == SingleView) {
                    // 单张视图模式下加载新图片
                    loadImageByIndex(newIndex);
                } else {
                    // 缩略图模式下更新选中项
                    currentImageIndex = newIndex;
                    thumbnailWidget->setSelectedIndex(newIndex);
                }
            }

            update();
            updateWindowTitle();
        }
    } else {
        QMessageBox::critical(this, tr("错误"), tr("移动图片到回收站失败"));
    }
}

void ImageWidget::deleteSelectedThumbnail()
{
    if (currentViewMode == ThumbnailView) {
        int selectedIndex = thumbnailWidget->getSelectedIndex();
        if (selectedIndex >= 0 && selectedIndex < imageList.size()) {
            // 临时切换到要删除的图片，然后调用删除函数
            QString imagePath =
                currentDir.absoluteFilePath(imageList.at(selectedIndex));
            loadImage(imagePath);
            currentImageIndex = selectedIndex;
            deleteCurrentImage();
        } else {
            QMessageBox::warning(this, tr("警告"), tr("请先选择要删除的图片"));
        }
    }
}


bool ImageWidget::moveFileToRecycleBin(const QString &filePath)
{
    // Windows API 方式移动到回收站
    WCHAR wcPath[MAX_PATH];
    memset(wcPath, 0, sizeof(wcPath));
    filePath.toWCharArray(wcPath);
    wcPath[filePath.length()] = '\0'; // 确保以 null 结尾

    SHFILEOPSTRUCT fileOp;
    ZeroMemory(&fileOp, sizeof(fileOp));
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = wcPath;
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION;

    int result = SHFileOperation(&fileOp);
    return (result == 0);
}

void ImageWidget::permanentlyDeleteSelectedThumbnail()
{
    if (currentViewMode == ThumbnailView) {
        int selectedIndex = thumbnailWidget->getSelectedIndex();
        if (selectedIndex >= 0 && selectedIndex < imageList.size()) {
            QString imagePath =
                currentDir.absoluteFilePath(imageList.at(selectedIndex));
            loadImage(imagePath);
            currentImageIndex = selectedIndex;
            permanentlyDeleteCurrentImage();
        } else {
            QMessageBox::warning(this, tr("警告"), tr("请先选择要删除的图片"));
        }
    }
}

void ImageWidget::permanentlyDeleteCurrentImage()
{
    if (currentImagePath.isEmpty() || !QFile::exists(currentImagePath)) {
        QMessageBox::warning(this, tr("警告"), tr("没有可删除的图片"));
        return;
    }

    // 警告对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::warning(
        this, tr("永久删除警告"),
        tr("确定要永久删除图片 '") + QFileInfo(currentImagePath).fileName() +
            "' 吗？\n"
                                                                              "此操作无法撤销，文件将无法恢复！",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString imageToDelete = currentImagePath;
    int indexToDelete = currentImageIndex;

    // 直接删除文件
    if (QFile::remove(imageToDelete)) {
        //logMessage("已永久删除图片: " + imageToDelete);

        // 其余代码与 deleteCurrentImage 相同
        imageCache.remove(imageToDelete);
        ThumbnailWidget::clearThumbnailCacheForImage(imageToDelete);

        if (indexToDelete >= 0 && indexToDelete < imageList.size()) {
            imageList.removeAt(indexToDelete);
            thumbnailWidget->setImageList(imageList, currentDir);

            if (imageList.isEmpty()) {
                pixmap = QPixmap();
                currentImagePath.clear();
                currentImageIndex = -1;
                if (currentViewMode == SingleView) {
                    switchToThumbnailView();
                }
            } else {
                int newIndex = indexToDelete;
                if (newIndex >= imageList.size()) {
                    newIndex = imageList.size() - 1;
                }

                if (currentViewMode == SingleView) {
                    loadImageByIndex(newIndex);
                } else {
                    currentImageIndex = newIndex;
                    thumbnailWidget->setSelectedIndex(newIndex);
                }
            }

            update();
            updateWindowTitle();
        }
    } else {
        QMessageBox::critical(this, tr("错误"), tr("删除图片失败，可能没有权限或文件被占用"));
    }
}

//添加快捷键
void ImageWidget::createShortcutActions()
{
    // 打开文件夹快捷键：Ctrl+O
    openFolderAction = new QAction(tr("打开文件夹"), this);
    openFolderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    openFolderAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(openFolderAction, &QAction::triggered, this, &ImageWidget::openFolder);
    this->addAction(openFolderAction);

    // 打开图片快捷键：Ctrl+Shift+O
    openImageAction = new QAction(tr("打开图片"), this);
    openImageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    openImageAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(openImageAction, &QAction::triggered, this, &ImageWidget::openImage);
    this->addAction(openImageAction);

    // 保存图片快捷键：Ctrl+S
    saveImageAction = new QAction(tr("保存图片"), this);
    saveImageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    saveImageAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(saveImageAction, &QAction::triggered, this, &ImageWidget::saveImage);
    this->addAction(saveImageAction);

    // 拷贝图片快捷键：Ctrl+C
    copyImageAction = new QAction(tr("拷贝图片"), this);
    copyImageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
    copyImageAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(copyImageAction, &QAction::triggered, this,
            &ImageWidget::copyImageToClipboard);
    this->addAction(copyImageAction);

    // 粘贴图片快捷键：Ctrl+V
    pasteImageAction = new QAction(tr("粘贴图片"), this);
    pasteImageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
    pasteImageAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(pasteImageAction, &QAction::triggered, this,
            &ImageWidget::pasteImageFromClipboard);
    this->addAction(pasteImageAction);

    // 关于窗口快捷键：F1
    aboutAction = new QAction(tr("关于"), this);
    aboutAction->setShortcut(QKeySequence(Qt::Key_F1));
    aboutAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(aboutAction, &QAction::triggered, this, &ImageWidget::showAboutDialog);
    this->addAction(aboutAction);

    qDebug() << "快捷键已创建: "
             << "Ctrl+O (打开文件夹), "
             << "Ctrl+Shift+O (打开图片), "
             << "Ctrl+S (保存图片), "
             << "Ctrl+C (拷贝图片), "
             << "Ctrl+V (粘贴图片), "
             << "F1 (关于)";

    // 新增编辑快捷键
    // 旋转快捷键：Ctrl+R 顺时针旋转90度
    QAction *rotateCwAction = new QAction(tr("顺时针旋转"), this);
    rotateCwAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    rotateCwAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(rotateCwAction, &QAction::triggered, this,
            &ImageWidget::rotate90CW);
    this->addAction(rotateCwAction);

    // 旋转快捷键：Ctrl+Shift+R 逆时针旋转90度
    QAction *rotateCcwAction = new QAction(tr("逆时针旋转"), this);
    rotateCcwAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    rotateCcwAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(rotateCcwAction, &QAction::triggered, this,
            &ImageWidget::rotate90CCW);
    this->addAction(rotateCcwAction);

    // 水平镜像快捷键：Ctrl+H
    QAction *mirrorHAction = new QAction(tr("水平镜像"), this);
    mirrorHAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    mirrorHAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(mirrorHAction, &QAction::triggered, this,
            &ImageWidget::mirrorHorizontal);
    this->addAction(mirrorHAction);

    // 垂直镜像快捷键：Ctrl+V (注意：Ctrl+V 已经用于粘贴，这里可以改为其他快捷键，比如 Ctrl+Shift+V)
    QAction *mirrorVAction = new QAction(tr("垂直镜像"), this);
    mirrorVAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    mirrorVAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(mirrorVAction, &QAction::triggered, this,
            &ImageWidget::mirrorVertical);
    this->addAction(mirrorVAction);

    // 重置变换快捷键：Ctrl+0
    QAction *resetTransformAction = new QAction(tr("重置变换"), this);
    resetTransformAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    resetTransformAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(resetTransformAction, &QAction::triggered, this,
            &ImageWidget::resetTransform);
    this->addAction(resetTransformAction);

    qDebug() << "编辑快捷键已创建: "
             << "Ctrl+R (顺时针旋转), "
             << "Ctrl+Shift+R (逆时针旋转), "
             << "Ctrl+H (水平镜像), "
             << "Ctrl+Shift+V (垂直镜像), "
             << "Ctrl+0 (重置变换)";

}


void ImageWidget::showAboutDialog()
{
    // 在画布模式下临时禁用鼠标穿透
    bool wasPassthrough = mousePassthrough;
    if (canvasMode) {
        disableMousePassthrough();
    }

    QString aboutText =
        "<div style='text-align: center;'>"
        "<table width='100%' cellspacing='0' cellpadding='0'>"
        "<tr>"
        "</td>"
        "<td valign='top'>"
        "<h2 style='margin-top: 0;'>" +
                        tr("图片查看器") +
                        "</h2>"
                                             "<p><b>" +
                        tr("版本:") +
                        "</b> 1.38</p>"
                                        "<p><b>" +
                        tr("开发者:") +
                        "</b> 幸运人的珠宝berylok</p>"
                                          "<p><b>" +
                        tr("描述:") + "</b> " +
                        tr("一个功能丰富的图片查看器，支持多种浏览模式和画布功能") +
                        "</p>"
                                                                                                               "</td>"
                                                                                                               "</tr>"
                                                                                                               "</table>"
                                                                                                               "</div>"
                                                                                                               "<hr>"
                                                                                                               "<h3>" +
                        tr("主要功能:") +
                        "</h3>"
                                            "<ul>"
                                            "<li><b>" +
                        tr("两种浏览模式:") + "</b> " + tr("缩略图模式和单张图片模式") +
                        "</li>"
                                                                                           "<li><b>" +
                        tr("画布模式:") + "</b> " + tr("半透明显示图片，支持调节透明度") +
                        "</li>"
                                                                                             "<li><b>" +
                        tr("幻灯片播放:") + "</b> " + tr("自动轮播图片") +
                        "</li>"
                                                                             "<li><b>" +
                        tr("图片编辑:") + "</b> " + tr("复制、粘贴、保存图片") +
                        "</li>"
                                                                                   "<li><b>" +
                        tr("窗口定制:") + "</b> " + tr("透明背景、置顶、隐藏标题栏") +
                        "</li>"
                                                                                         "<li><b>" +
                        tr("快捷键支持:") + "</b> " + tr("丰富的键盘快捷键操作") +
                        "</li>"
                                                                                     "</ul>"
                                                                                     "<h3>" +
                        tr("基本使用方法:") +
                        "</h3>"
                                                "<ol>"
                                                "<li><b>" +
                        tr("打开图片/文件夹:") + "</b> " +
                        tr("使用 Ctrl+O 打开文件夹，Ctrl+Shift+O 打开单张图片") +
                        "</li>"
                                                                                                                       "<li><b>" +
                        tr("浏览图片:") + "</b> " +
                        tr("在缩略图模式下使用方向键导航，在单张模式下使用左右键切换") +
                        "</li>"
                                                                                                                       "<li><b>" +
                        tr("画布模式:") + "</b> " +
                        tr("在单张模式下按 Insert 键进入画布模式，ESC 键退出") +
                        "</li>"
                                                                                                               "<li><b>" +
                        tr("调节透明度:") + "</b> " +
                        tr("在画布模式下使用 PageUp/PageDown 调节透明度") +
                        "</li>"
                                                                                                            "<li><b>" +
                        tr("幻灯片:") + "</b> " + tr("按空格键开始/停止幻灯片播放") +
                        "</li>"
                                                                                        "<li><b>" +
                        tr("删除图片:") + "</b> " + tr("按 Delete 键删除当前图片") +
                        "</li>"
                                                                                       "</ol>"
                                                                                       "<p>" +
                        tr("按 F1 键可随时查看此帮助信息。") +
                        "</p>"
                                                                 "<hr>"
                                                                 "<p style='color: gray; font-size: small; text-align: center;'>© 2025 "

                                                                 "berylok. " +
                        tr("保留所有权利。") + "</p>";

    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(tr("关于图片查看器"));
    aboutBox.setText(aboutText);
    aboutBox.setTextFormat(Qt::RichText);

    // 设置更明确的样式，确保在画布模式下正常显示
    aboutBox.setStyleSheet(
        "QMessageBox { "
        "   background-color: white; "
        "   color: black; "
        "   border: 2px solid #cccccc; "
        "}"
        "QMessageBox QLabel { "
        "   color: black; "
        "   background-color: white; "
        "}"
        "QMessageBox QPushButton { "
        "   background-color: #f0f0f0; "
        "   border: 1px solid #cccccc; "
        "   padding: 5px 15px; "
        "}"
        "QMessageBox QPushButton:hover { "
        "   background-color: #e0e0e0; "
        "}"
        );

    // 设置对话框属性，确保正常显示
    aboutBox.setWindowFlags(aboutBox.windowFlags() & ~Qt::FramelessWindowHint);
    aboutBox.setAttribute(Qt::WA_NoSystemBackground, false);
    aboutBox.setAttribute(Qt::WA_TranslucentBackground, false);
    aboutBox.setAutoFillBackground(true);

    // 确保对话框有合适的尺寸
    aboutBox.setMinimumSize(600, 700);

    // 尝试加载并设置大图标
    QPixmap iconPixmap;
    if (iconPixmap.load(":/icons/PictureView.ico")) {
        // 设置100x100的大图标
        QPixmap scaledPixmap = iconPixmap.scaled(100, 100, Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation);
        aboutBox.setIconPixmap(scaledPixmap);
    } else {
        // 如果资源加载失败，尝试从文件系统加载
        QString appDir = QApplication::applicationDirPath();
        QString iconPath = appDir + "/icons/PictureView.png";
        if (QFile::exists(iconPath) && iconPixmap.load(iconPath)) {
            QPixmap scaledPixmap = iconPixmap.scaled(100, 100, Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation);
            aboutBox.setIconPixmap(scaledPixmap);
        } else {
            // 如果都失败，使用默认图标
            aboutBox.setIcon(QMessageBox::Information);
        }
    }

    aboutBox.exec();

    // 恢复鼠标穿透状态
    if (wasPassthrough && canvasMode) {
        enableMousePassthrough();
    }
}

void ImageWidget::showShortcutHelp()
{
    // 在画布模式下临时禁用鼠标穿透
    bool wasPassthrough = mousePassthrough;
    if (canvasMode) {
        disableMousePassthrough();
    }

    QString helpText =
        tr("<h3>快捷键帮助</h3>"
           "<table border='0' cellspacing='5' cellpadding='3'>"
           "<tr><td><b>Ctrl+O</b></td><td>打开文件夹</td></tr>"
           "<tr><td><b>Ctrl+Shift+O</b></td><td>打开图片</td></tr>"
           "<tr><td><b>Ctrl+S</b></td><td>保存图片</td></tr>"
           "<tr><td><b>Ctrl+C</b></td><td>拷贝图片到剪贴板</td></tr>"
           "<tr><td><b>Ctrl+V</b></td><td>从剪贴板粘贴图片</td></tr>"
           "<tr><td><b>Insert</b></td><td>进入/退出画布模式</td></tr>"
           "<tr><td><b>ESC</b></td><td>退出画布模式或返回缩略图</td></tr>"
           "<tr><td><b>← →</b></td><td>上一张/下一张图片</td></tr>"
           "<tr><td><b>↑ ↓</b></td><td>合适大小/实际大小</td></tr>"
           "<tr><td><b>Home/End</b></td><td>第一张/最后一张图片</td></tr>"
           "<tr><td><b>Del</b></td><td>删除当前图片</td></tr>"
                          "<tr><td><b>PageUp/PageDown</b></td><td>调节透明度（画布模式）</"
                          "td></tr>"
           "<tr><td><b>空格</b></td><td>开始/停止幻灯片</td></tr>"
           "<tr><td><b>F1</b></td><td>显示关于窗口</td></tr>"
           "</table>"
                          "<p style='margin-top: "
                          "15px;'><i>"
                          "提示：在画布模式下，大部分快捷键会被禁用，只能使用ESC、Insert、Page"
                          "Up/PageDown和菜单键。</i></p>");

    QMessageBox helpBox(this);
    helpBox.setWindowTitle(tr("快捷键帮助"));
    helpBox.setText(helpText);
    helpBox.setTextFormat(Qt::RichText);

    // 设置样式
    helpBox.setStyleSheet(
        "QMessageBox { "
        "   background-color: white; "
        "   color: black; "
        "   border: 2px solid #cccccc; "
        "}"
        "QMessageBox QLabel { "
        "   color: black; "
        "   background-color: white; "
        "}"
        "QMessageBox QPushButton { "
        "   background-color: #f0f0f0; "
        "   border: 1px solid #cccccc; "
        "   padding: 5px 15px; "
        "}"
        "QMessageBox QPushButton:hover { "
        "   background-color: #e0e0e0; "
        "}"
        );

    // 设置对话框属性
    helpBox.setWindowFlags(helpBox.windowFlags() & ~Qt::FramelessWindowHint);
    helpBox.setAttribute(Qt::WA_NoSystemBackground, false);
    helpBox.setAttribute(Qt::WA_TranslucentBackground, false);
    helpBox.setAutoFillBackground(true);

    helpBox.setMinimumSize(450, 500);
    helpBox.exec();

    // 恢复鼠标穿透状态
    if (wasPassthrough && canvasMode) {
        enableMousePassthrough();
    }
}

// 镜像和旋转功能实现
// 修正后的镜像和旋转功能实现
void ImageWidget::mirrorHorizontal()
{
    if (pixmap.isNull()) return;

    isHorizontallyFlipped = !isHorizontallyFlipped;
    applyTransformations();

    //logMessage("水平镜像已" + QString(isHorizontallyFlipped ? "启用" : "禁用"));
}

void ImageWidget::mirrorVertical()
{
    if (pixmap.isNull()) return;

    isVerticallyFlipped = !isVerticallyFlipped;
    applyTransformations();

    //logMessage("垂直镜像已" + QString(isVerticallyFlipped ? "启用" : "禁用"));
}

void ImageWidget::rotate90CW()
{
    if (pixmap.isNull()) return;

    rotationAngle = (rotationAngle + 90) % 360;
    applyTransformations();

    // 旋转后重置为合适大小
    fitToWindow();

    //logMessage("顺时针旋转90度");
}

void ImageWidget::rotate90CCW()
{
    if (pixmap.isNull()) return;

    rotationAngle = (rotationAngle - 90 + 360) % 360;
    applyTransformations();

    // 旋转后重置为合适大小
    fitToWindow();

    //logMessage("逆时针旋转90度");
}

void ImageWidget::rotate180()
{
    if (pixmap.isNull()) return;

    rotationAngle = (rotationAngle + 180) % 360;
    applyTransformations();

    //logMessage("旋转180度");
}

// 应用所有变换的核心函数
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

    // 重置为合适大小
    fitToWindow();

    update();

    //logMessage("变换已重置");
}

bool ImageWidget::isTransformed() const
{
    return rotationAngle != 0 || isHorizontallyFlipped || isVerticallyFlipped;
}
