// imagewidget_config.cpp
#include "imagewidget.h"
#include <QCloseEvent>
#include <QGuiApplication>
#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#endif

#include <QProcess>
#include <QCoreApplication>
#include <QMessageBox>

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

    // 设置透明背景属性
    if (config.transparentBackground) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        // 标记透明背景机制已就绪（因为窗口已经以此属性创建/显示过）
        m_transparentBackgroundReady = true;
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
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

    // 恢复上次打开的图片路径（但不自动加载，避免覆盖当前状态）
    if (!config.lastImagePath.isEmpty() && QFile::exists(config.lastImagePath)) {
        currentImagePath = config.lastImagePath;
        // 可选：如果当前没有图片，则加载它
        if (pixmap.isNull() && currentViewMode == SingleView) {
            loadImage(currentImagePath);
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
    bool wasTranslucent = testAttribute(Qt::WA_TranslucentBackground);
    bool wasSingleView = (currentViewMode == SingleView);
    bool hadImage = !pixmap.isNull();

    Qt::WindowFlags flags = windowFlags();
    if (flags & Qt::WindowStaysOnTopHint) {
        flags &= ~Qt::WindowStaysOnTopHint;
    } else {
        flags |= Qt::WindowStaysOnTopHint;
    }
    setWindowFlags(flags);
    show();

    setAttribute(Qt::WA_TranslucentBackground, wasTranslucent);
    setAutoFillBackground(!wasTranslucent);

    if (wasSingleView && hadImage && wasTranslucent) {
        updateMask();  // 第一次设置形状
        qDebug() << "置顶切换后重新生成掩码";

        // 延迟再次设置形状，给窗口管理器时间
        QTimer::singleShot(200, this, [this]() {
            if (testAttribute(Qt::WA_TranslucentBackground) && !pixmap.isNull()) {
                updateMask();  // 第二次设置
                qDebug() << "置顶切换后延迟再次生成掩码";

#ifdef Q_OS_LINUX
                // 同步 X11 请求，确保处理完毕
                Display *display = XOpenDisplay(nullptr);
                if (display) {
                    XSync(display, False);
                    XCloseDisplay(display);
                }
#endif
            }
        });
    } else {
        clearX11Shape();
        clearMask();
    }

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


void ImageWidget::restartApplication()
{
    saveConfiguration();  // 确保最新设置已保存

    QString appPath = QCoreApplication::applicationFilePath();
    QStringList args;

    // 如果当前有打开的图片，将图片路径作为参数传递给新实例
    if (!currentImagePath.isEmpty() && QFile::exists(currentImagePath)) {
        args << currentImagePath;
    }

    // 也可以保留原始的命令行参数（如 --lang 等），但注意避免重复传递图片路径
    // 简单起见，只传递当前图片路径即可满足“在新窗口打开”的需求

    if (QProcess::startDetached(appPath, args)) {
        QCoreApplication::quit();
    } else {
        QMessageBox::warning(this, tr("重启失败"),
                             tr("无法启动新实例，请手动重启程序。"));
    }
}

void ImageWidget::toggleTransparentBackground()
{
    bool currentState = testAttribute(Qt::WA_TranslucentBackground);

    if (!currentState) {
        // 尝试开启透明背景
        if (m_transparentBackgroundReady) {
            // 窗口已支持透明背景，直接动态切换
            setAttribute(Qt::WA_TranslucentBackground, true);
            setAutoFillBackground(false);
            if (currentViewMode == SingleView && !pixmap.isNull()) {
                updateMask();
            }
            currentConfig.transparentBackground = true;
            saveConfiguration();
            update();
            qDebug() << "透明背景已动态开启";
        } else {
            // 首次开启，需要重启才能完全生效
            QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                                      tr("需要重启"),
                                                                      tr("开启透明背景需要重启程序才能完全生效。是否立即重启？"),
                                                                      QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                currentConfig.transparentBackground = true;
                saveConfiguration();
                restartApplication();  // 需要事先实现该函数
            }
            // 如果用户选择 No，则什么都不做（保持关闭状态）
        }
    } else {
        // 关闭透明背景：总是可以直接动态关闭
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAutoFillBackground(true);
        clearMask();
        currentConfig.transparentBackground = false;
        saveConfiguration();
        update();
        qDebug() << "透明背景已关闭";
    }
}
