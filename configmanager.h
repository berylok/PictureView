#ifndef configmanager_H
#define configmanager_H

#include <QString>
#include <QPoint>
#include <QSize>

class configmanager
{
public:
    // 配置结构体
    struct Config {
        // 窗口状态
        QPoint windowPosition;
        QSize windowSize;
        bool windowMaximized;

        // 透明背景状态
        bool transparentBackground;

        // 标题栏状态
        bool titleBarVisible;

        // 置顶状态
        bool alwaysOnTop;

        // 最近打开文件路径
        QString lastOpenPath;

        // 默认构造函数
        Config();
    };

    configmanager(const QString& filename = "viewer_config.ini");

    // 保存配置到文件
    bool saveConfig(const Config& config);

    // 从文件加载配置
    Config loadConfig();

    // 获取配置文件路径
    QString getConfigPath() const;

private:
    QString configPath;
};

#endif // configmanager_H
