// imagewidget_config.cpp
#include "imagewidget.h"
#include <QCloseEvent>

void ImageWidget::closeEvent(QCloseEvent *event)
{
    // 销毁控制面板
    destroyControlPanel();
    saveConfiguration();
    event->accept();
}

void ImageWidget::loadConfiguration()
{
    configmanager::Config config = configmanager->loadConfig();
    currentConfig.lastOpenPath = config.lastOpenPath;
    applyConfiguration(config);
}

void ImageWidget::saveConfiguration()
{
    configmanager::Config config;

    // 保存窗口状态
    config.windowPosition = this->pos();
    config.windowSize = this->size();
    config.windowMaximized = this->isMaximized();
    config.transparentBackground = this->testAttribute(Qt::WA_TranslucentBackground);
    config.titleBarVisible = !(this->windowFlags() & Qt::FramelessWindowHint);
    config.lastOpenPath = currentConfig.lastOpenPath;

    configmanager->saveConfig(config);
}

void ImageWidget::applyConfiguration(const configmanager::Config &config)
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
    flags &= ~Qt::FramelessWindowHint;
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

    // 设置窗口位置和大小
    if (!config.windowPosition.isNull() && !config.windowMaximized) {
        move(config.windowPosition);
    }

    if (!config.windowSize.isEmpty() && !config.windowMaximized) {
        resize(config.windowSize);
    }

    // 设置窗口最大化状态
    if (config.windowMaximized) {
        showMaximized();
    } else {
        showNormal();
        if (normalGeometry.isNull() && !config.windowPosition.isNull() &&
            !config.windowSize.isEmpty()) {
            move(config.windowPosition);
            resize(config.windowSize);
        }
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
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
        qDebug("透明背景 关闭");
    } else {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        qDebug("透明背景 开启");
    }

    update();
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
