#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QPoint>
#include <QSize>

class ConfigManager
{
public:
    // 配置结构体
    struct Config {
        // 窗口状态
        QPoint windowPosition;
        QSize windowSize;
        bool windowMaximized;

        bool transparentBackground; // 透明背景状态
        bool titleBarVisible;       // 标题栏状态
        bool alwaysOnTop;           // 置顶状态
        QString lastOpenPath;       // 最近打开文件路径

        int lastViewMode;        // 0=ThumbnailView, 1=SingleView
        int lastImageIndex;      // 最后查看的图片索引
        QString lastImagePath;   // 最后查看的图片路径（备用）


        //Config();       // 默认构造函数

        Config() :
            windowPosition(100, 100),
            windowSize(800, 600),
            windowMaximized(false),
            transparentBackground(false),
            titleBarVisible(true),
            alwaysOnTop(false),
            lastViewMode(0),          // 默认缩略图模式
            lastImageIndex(-1),       // 无选中图片
            lastImagePath("")
        {}
    };

    ConfigManager(const QString& filename = "viewer_config.ini");

    // 保存配置到文件
    bool saveConfig(const Config& config);

    // 从文件加载配置
    Config loadConfig();

    // 获取配置文件路径
    QString getConfigPath() const;

private:
    QString configPath;

};

#endif // CONFIGMANAGER_H
