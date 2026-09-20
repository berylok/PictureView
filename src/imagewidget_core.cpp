
// imagewidget_core.cpp
#include "imagewidget.h"
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QScrollBar>  // 添加这行
#include <QFrame>      // 添加这行

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
    showNavigationHints(true),
    m_windowOpacity(1.0),
    canvasMode(false),
    mousePassthrough(false),
    controlPanel(nullptr),
    openFolderAction(nullptr),
    openImageAction(nullptr),
    saveImageAction(nullptr),
    copyImageAction(nullptr),
    pasteImageAction(nullptr),
    aboutAction(nullptr),
    rotationAngle(0),
    isHorizontallyFlipped(false),
    isVerticallyFlipped(false),
    isArchiveMode(false),
    m_transparentBackgroundReady(false)

{

    // 创建缩略图部件 - 使用统一的构造函数
    thumbnailWidget = new ThumbnailWidget(this, this);  // imageWidget, parent
    // // 创建缩略图部件
    // thumbnailWidget = new ThumbnailWidget(this);

    // thumbnailWidget = new ThumbnailWidget(nullptr, this);  // 如果构造函数需要两个参数    // 设置 ImageWidget 指针

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

    scrollArea->setWidget(thumbnailWidget);
    scrollArea->show();

    // 连接信号
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

    setFocusPolicy(Qt::StrongFocus);

    // 创建幻灯定时器
    slideshowTimer = new QTimer(this);
    connect(slideshowTimer, &QTimer::timeout, this, &ImageWidget::slideshowNext);

    setMouseTracking(true);

    connect(&m_wheelTimer, &QTimer::timeout, this, [this](){
        if (m_maskDirty)
            updateMask();
    });

    // 初始重绘以确保无残影
    update();


}

ImageWidget::~ImageWidget()
{
    // 1. 销毁控制面板（UI 层面的，先做）
    destroyControlPanel();

    // 2. 等所有 QtConcurrent 后台任务结束
    //    - 此时 thumbnailWidget、imageCache、archiveHandler 都还活着
    //    - 后台线程的 QPointer<ThumbnailWidget> guard 不为 null，能正常跑完
    //    - 最多等 2 秒，避免 UI 卡死太久
    if (!QThreadPool::globalInstance()->waitForDone(2000)) {
        qWarning() << "等待后台任务超时，可能有任务仍在运行";
    }

    // 3. 之后再删自己 new 的对象（thumbnailWidget/scrollArea 是子对象，Qt 自动删）
    delete configManager;
    configManager = nullptr;
}

void ImageWidget::setCurrentDir(const QDir &dir)
{
    currentDir = dir;
}

void ImageWidget::ensureWindowVisible()
{
    // 如果窗口最小化，恢复显示
    if (isMinimized()) {
        showNormal();
    }

    // 只有在窗口不可见时才调用 show()
    if (!isVisible()) {
        show();
        raise();
        activateWindow();
    } else {
        // 如果已经可见，只需要激活
        raise();
        activateWindow();
    }

    // 设置焦点到缩略图部件
    thumbnailWidget->setFocus();
}

void ImageWidget::ensureFocus()
{
    if (currentViewMode == ThumbnailView) {
        thumbnailWidget->setFocus();
    } else {
        setFocus();
    }
}

void ImageWidget::testKeyboard()
{
    //qDebug() << "=== 键盘测试开始 ===";
    //qDebug() << "当前焦点部件:"<< (QApplication::focusWidget() ? QApplication::focusWidget()->objectName() : "无");
    //qDebug() << "缩略图部件焦点状态:" << thumbnailWidget->hasFocus();
    //qDebug() << "当前模式:"<< (currentViewMode == SingleView ? "单张" : "缩略图");
    //qDebug() << "图片列表大小:" << imageList.size();
    //qDebug() << "当前选中索引:" << currentImageIndex;
    //qDebug() << "=== 键盘测试结束 ===";
}
