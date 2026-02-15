#include "configmanager.h"
#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QDebug>

// Config 结构体的默认构造函数
// ConfigManager::Config::Config() :
//     windowPosition(QPoint(100, 100)),
//     windowSize(QSize(800, 600)),
//     windowMaximized(false),
//     transparentBackground(false),
//     titleBarVisible(true),
//     alwaysOnTop(false) {}

// ConfigManager 构造函数
ConfigManager::ConfigManager(const QString& filename)
{
    // 使用 Qt 的标准配置路径
    QString configDir = QApplication::applicationDirPath();
    configPath = configDir + "/" + filename;

    qDebug() << "Config file path:" << configPath;
}

// 保存配置到文件
bool ConfigManager::saveConfig(const Config& config)
{
    QSettings settings(configPath, QSettings::IniFormat);

    // 保存窗口状态
    settings.beginGroup("Window");
    settings.setValue("Position", config.windowPosition);
    settings.setValue("Size", config.windowSize);
    settings.setValue("Maximized", config.windowMaximized);
    settings.endGroup();

    // 保存透明背景状态
    settings.beginGroup("Appearance");
    settings.setValue("TransparentBackground", config.transparentBackground);
    settings.setValue("TitleBarVisible", config.titleBarVisible);
    //settings.setValue("AlwaysOnTop", config.alwaysOnTop);
    settings.endGroup();

    // 保存最近打开路径
    settings.beginGroup("Recent");
    settings.setValue("LastOpenPath", config.lastOpenPath);
    settings.endGroup();


    // 新增：保存状态信息
    settings.beginGroup("State");
    settings.setValue("LastViewMode", config.lastViewMode);
    settings.setValue("LastImageIndex", config.lastImageIndex);
    settings.setValue("LastImagePath", config.lastImagePath);
    settings.endGroup();


    settings.sync();
    return (settings.status() == QSettings::NoError);
}

// 从文件加载配置
ConfigManager::Config ConfigManager::loadConfig()
{
    Config config;
    QSettings settings(configPath, QSettings::IniFormat);

    // 如果配置文件不存在，返回默认配置
    if (!QFile::exists(configPath)) {
        qDebug() << "Config file not found, using default settings";
        return config;
    }

    // 加载窗口状态
    settings.beginGroup("Window");
    config.windowPosition = settings.value("Position", config.windowPosition).toPoint();
    config.windowSize = settings.value("Size", config.windowSize).toSize();
    config.windowMaximized = settings.value("Maximized", config.windowMaximized).toBool();
    settings.endGroup();

    // 加载外观状态
    settings.beginGroup("Appearance");
    config.transparentBackground =
        settings.value("TransparentBackground", config.transparentBackground)
                                       .toBool();
    config.titleBarVisible =
        settings.value("TitleBarVisible", config.titleBarVisible).toBool();
    // config.alwaysOnTop = settings.value("AlwaysOnTop",
    // config.alwaysOnTop).toBool();
    settings.endGroup();

    // 加载最近打开路径
    settings.beginGroup("Recent");
    config.lastOpenPath = settings.value("LastOpenPath", config.lastOpenPath).toString();
    settings.endGroup();

    // 新增：加载状态信息
    settings.beginGroup("State");
    config.lastViewMode = settings.value("LastViewMode", 0).toInt();
    config.lastImageIndex = settings.value("LastImageIndex", -1).toInt();
    config.lastImagePath = settings.value("LastImagePath", "").toString();
    settings.endGroup();

    qDebug() << "Config loaded from:" << configPath;
    return config;
}

// 获取配置文件路径
QString ConfigManager::getConfigPath() const
{
    return configPath;
}
