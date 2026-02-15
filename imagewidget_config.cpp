// imagewidget_config.cpp
#include "imagewidget.h"
#include <QCloseEvent>
#include <QGuiApplication>

void ImageWidget::closeEvent(QCloseEvent *event)
{
    // 销毁控制面板
    destroyControlPanel();
    saveConfiguration();
    event->accept();
}

void ImageWidget::loadConfiguration()
{
    ConfigManager::Config config = configManager->loadConfig();
    qDebug() << "加载配置：透明背景 =" << config.transparentBackground;

    // 同步所有字段到 currentConfig
    currentConfig.windowPosition = config.windowPosition;
    currentConfig.windowSize = config.windowSize;
    currentConfig.windowMaximized = config.windowMaximized;
    currentConfig.transparentBackground = config.transparentBackground;
    currentConfig.titleBarVisible = config.titleBarVisible;
    currentConfig.alwaysOnTop = config.alwaysOnTop;
    currentConfig.lastOpenPath = config.lastOpenPath;
    currentConfig.lastViewMode = config.lastViewMode;
    currentConfig.lastImageIndex = config.lastImageIndex;
    currentConfig.lastImagePath = config.lastImagePath;

    applyConfiguration(config);
}

void ImageWidget::saveConfiguration()
{
    ConfigManager::Config config;

    // 窗口状态
    config.windowPosition = pos();
    config.windowSize = size();
    config.windowMaximized = isMaximized();

    // 透明背景状态：使用用户偏好，而不是当前窗口属性
    config.transparentBackground = currentConfig.transparentBackground;

    // 标题栏状态
    config.titleBarVisible = !(windowFlags() & Qt::FramelessWindowHint);
    config.alwaysOnTop = (windowFlags() & Qt::WindowStaysOnTopHint) != 0;

    // 最近打开路径
    config.lastOpenPath = currentConfig.lastOpenPath;

    // 视图模式等
    config.lastViewMode = (currentViewMode == SingleView) ? 1 : 0;
    config.lastImageIndex = currentImageIndex;
    config.lastImagePath = currentImagePath;

    configManager->saveConfig(config);
    qDebug() << "保存配置：透明背景 =" << config.transparentBackground;
}

// imagewidget_config.cpp - 修改 applyConfiguration 方法
void ImageWidget::applyConfiguration(const ConfigManager::Config &config)
{
    // 保存当前窗口状态
    bool wasMaximized = isMaximized();
    QRect normalGeometry = geometry();

    // 检查保存的几何信息是否有效
    QRect savedGeometry = QRect(config.windowPosition, config.windowSize);
    QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();

    // 如果保存的位置不在屏幕内，使用默认位置
    if (!screenGeometry.contains(savedGeometry.topLeft())) {
        savedGeometry.moveTopLeft(QPoint(100, 100));
    }

    // 如果保存的大小不合理，使用默认大小
    if (savedGeometry.width() < 100 || savedGeometry.height() < 100) {
        savedGeometry.setSize(QSize(800, 600));
    }

    // 第一步：设置窗口属性（透明背景）
    if (config.transparentBackground) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
    }

    // 第二步：在设置窗口几何之前，先设置标题栏标志
    // 重要：先恢复为正常窗口状态再设置标志
    bool needRestoreMaximized = false;

    if (wasMaximized || config.windowMaximized) {
        // 如果当前或配置要求最大化，先显示为正常窗口
        //showNormal();
        needRestoreMaximized = true;
    }

    // 设置标题栏标志
    Qt::WindowFlags flags = windowFlags();
    flags &= ~Qt::FramelessWindowHint;
    if (!config.titleBarVisible) {
        flags |= Qt::FramelessWindowHint;
    }

    if (config.alwaysOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }

    // 只有在标志确实改变时才设置
    if (flags != windowFlags()) {
        setWindowFlags(flags);
        // 设置窗口几何
        move(savedGeometry.topLeft());
        resize(savedGeometry.size());
        show(); // 必须重新显示
    }

    // 第三步：应用窗口位置和大小
    if (!config.windowMaximized) {
        move(savedGeometry.topLeft());
        resize(savedGeometry.size());
        showNormal();
        qDebug() << "应用配置：窗口正常大小";
    } else {
        // 如果是最大化，先设置正常大小再最大化
        move(savedGeometry.topLeft());
        resize(savedGeometry.size());
        showMaximized();
        qDebug() << "应用配置：窗口最大化";
    }

    update();
}

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
        flags &= ~Qt::FramelessWindowHint;
    } else {
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

    saveConfiguration();
}

void ImageWidget::toggleAlwaysOnTop()
{
    // 保存当前透明背景状态和视图模式
    bool wasTranslucent = testAttribute(Qt::WA_TranslucentBackground);
    bool wasSingleView = (currentViewMode == SingleView);

    // 切换置顶标志
    if (windowFlags() & Qt::WindowStaysOnTopHint) {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    }

    // 重新显示窗口（setWindowFlags 会隐藏窗口，需要重新显示）
    show();

    // 恢复透明背景属性（如果之前开启）
    if (wasTranslucent) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
    }

    // 如果处于单图模式且有图片，重新应用掩码
    if (wasSingleView && !pixmap.isNull() && wasTranslucent) {
        updateMask();   // 重新生成掩码
    } else {
        clearMask();    // 确保掩码被清除（例如缩略图模式）
    }

    // 保存配置
    saveConfiguration();
}

void ImageWidget::toggleTransparentBackground()
{
    bool currentState = testAttribute(Qt::WA_TranslucentBackground);

    if (currentState) {
        // 关闭
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
        clearMask();
        currentConfig.transparentBackground = false;
        qDebug("透明背景 关闭");
    } else {
        // 开启
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        if (currentViewMode == SingleView && !pixmap.isNull()) {
            updateMask();
        }
        currentConfig.transparentBackground = true;
        qDebug("透明背景 开启");
    }
    saveConfiguration(); // 立即保存
    update();
}

void ImageWidget::setWindowOpacityValue(double opacity)
{
    opacity = qBound(0.1, opacity, 1.0);
    m_windowOpacity = opacity;
    this->setWindowOpacity(opacity);
}

bool ImageWidget::isAlwaysOnTop() const
{
    return windowFlags() & Qt::WindowStaysOnTopHint;
}

bool ImageWidget::hasTitleBar() const
{
    return !(windowFlags() & Qt::FramelessWindowHint);
}

bool ImageWidget::hasTransparentBackground() const
{
    return this->testAttribute(Qt::WA_TranslucentBackground);
}
