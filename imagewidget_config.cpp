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
    config.transparentBackground = this->testAttribute(Qt::WA_TranslucentBackground);
    config.titleBarVisible = !(this->windowFlags() & Qt::FramelessWindowHint);
    config.lastOpenPath = currentConfig.lastOpenPath;

    configManager->saveConfig(config);
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
    if (windowFlags() & Qt::WindowStaysOnTopHint) {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    }
    show();
}

void ImageWidget::toggleTransparentBackground()
{
    bool currentState = testAttribute(Qt::WA_TranslucentBackground);

    if (currentState) {
        // 关闭透明背景
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
        setStyleSheet(""); // 清除透明样式
        qDebug("透明背景 关闭");
    } else {
        // 开启透明背景
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        setStyleSheet("background: transparent;"); // 设置透明样式
        qDebug("透明背景 开启");
    }

    // 强制重绘
    update();
    repaint();

    // 保存配置
    saveConfiguration();
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
