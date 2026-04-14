#include "imagewidget.h"
#include "qimagereader.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDir>
#include <QDebug>  // 添加QDebug头文件
#include <QGuiApplication>  // 添加QGuiApplication头文件用于平台检测

int main(int argc, char *argv[])
{
    // === 在创建QApplication之前先检测环境变量 ===
    qDebug() << "=== 程序启动 - 环境检测 ===";
    qDebug() << "命令行参数:";
    for (int i = 0; i < argc; ++i) {
        qDebug() << "  argv[" << i << "]:" << argv[i];
    }

    // 创建QApplication
    QApplication app(argc, argv);

    // // 加载英文翻译文件
    // QTranslator appTranslator;
    // QString appTranslationsPath = QApplication::applicationDirPath() + "/translations";

    // // 尝试加载几种可能的文件名
    // bool loaded = false;
    // QStringList tryNames = {
    //     "PictureView_en_US",

    // };

    // for (const QString &name : tryNames) {
    //     if (appTranslator.load(name, appTranslationsPath)) {
    //         qDebug() << "✅ Loaded translation:" << name << "from" << appTranslationsPath;
    //         loaded = true;
    //         break;
    //     }
    // }

    // if (!loaded) {
    //     // 尝试从资源文件加载
    //     for (const QString &name : tryNames) {
    //         if (appTranslator.load(":/translations/" + name)) {
    //             qDebug() << "✅ Loaded translation from resources:" << name;
    //             loaded = true;
    //             break;
    //         }
    //     }
    // }

    // if (loaded) {
    //     app.installTranslator(&appTranslator);
    // } else {
    //     qDebug() << "❌ Failed to load any English translation file!";
    //     qDebug() << "   Checked paths:" << appTranslationsPath << "and :/translations/";
    // }

    // 设置更高的内存分配限制（512MB）
    QImageReader::setAllocationLimit(512);
    qDebug() << "设置内存分配限制为 512MB";

    // === 在设置应用程序信息后检测Qt平台 ===
    // 设置应用程序信息
    app.setApplicationName("PictureView");
    app.setApplicationVersion("1.5.2");
    app.setOrganizationName("berylok");

    // 打印环境信息
    qDebug() << "=== 环境检测结果 ===";
    qDebug() << "Qt运行平台:" << QGuiApplication::platformName();
    qDebug() << "QT_QPA_PLATFORM环境变量:" << qgetenv("QT_QPA_PLATFORM");
    qDebug() << "XDG_SESSION_TYPE环境变量:" << qgetenv("XDG_SESSION_TYPE");
    qDebug() << "DISPLAY环境变量:" << qgetenv("DISPLAY");
    qDebug() << "==================";

    // ... 以下是你原有的代码，保持不变 ...


    // 设置默认语言
    // QString locale = "zh_CN";  // 已为中文



    // 设置默认语言：优先使用系统语言，若系统语言不支持则回退到中文
    QString locale = QLocale::system().name();  // 例如 "zh_CN", "en_US"

    // 检查系统语言是否在我们支持的列表中
    QStringList supportedLocales = {"zh_CN", "en_US"};
    if (!supportedLocales.contains(locale)) {
        // 如果系统语言不是 zh_CN 或 en_US，则进一步判断语种
        QString lang = QLocale::system().languageToString(QLocale::system().language());
        if (lang == "Chinese") {
            locale = "zh_CN";
        } else {
            locale = "en_US";  // 其他所有语言默认显示英文
        }
    }


    // 检查命令行参数（支持 --lang zh_CN 或 --lang en_US）
    QCommandLineParser parser;
    QCommandLineOption langOption("lang", "Set language (zh_CN, en_US)", "language");
    parser.addOption(langOption);
    parser.process(app);

    if (parser.isSet(langOption)) {
        locale = parser.value(langOption);
    }

    // 加载对应的翻译文件
    QTranslator appTranslator;
    QTranslator qtTranslator;

    QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    qtTranslator.load("qt_" + locale, qtTranslationsPath);
    app.installTranslator(&qtTranslator);

    QString appTranslationsPath = QApplication::applicationDirPath() + "/translations";
    if (appTranslator.load("PictureView_" + locale, appTranslationsPath)) {
        app.installTranslator(&appTranslator);
    }



    // 注册文件关联选项
    QCommandLineOption registerOption(
        "register",
        QCoreApplication::translate("main", "Register file associations"));
    parser.addOption(registerOption);

    parser.process(app);

    if (parser.isSet(registerOption)) {
        QString exePath = QCoreApplication::applicationFilePath();
        QString shortExePath = ImageWidget().getShortPathName(exePath);
        QString openCommand = QString("\"%1\" \"%2\"").arg(shortExePath).arg("%1");

        ImageWidget().registerFileAssociation("png", "pngfile", openCommand);
        ImageWidget().registerFileAssociation("jpg", "jpgfile", openCommand);
        ImageWidget().registerFileAssociation("bmp", "bmpfile", openCommand);
        ImageWidget().registerFileAssociation("jpeg", "jpegfile", openCommand);
        ImageWidget().registerFileAssociation("webp", "webpfile", openCommand);
        ImageWidget().registerFileAssociation("gif", "giffile", openCommand);
        ImageWidget().registerFileAssociation("tiff", "tifffile", openCommand);
        ImageWidget().registerFileAssociation("tif", "tiffile", openCommand);

        qDebug() << QCoreApplication::translate("main", "File associations registered");
        return 0;
    }

    ImageWidget window;

    // 加载配置（必须在处理命令行参数之前）
    window.loadConfiguration();

    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        qDebug() << QCoreApplication::translate("main", "Opening file:") << filePath;

        if (QFile::exists(filePath)) {
            QFileInfo fileInfo(filePath);
            if (fileInfo.isDir()) {
                window.setCurrentDir(QDir(filePath));
                window.loadImageList();
            } else {
                window.loadImage(filePath);
                window.switchToSingleView();
            }
        } else {
            // 无命令行参数：根据保存的视图模式恢复
            if (window.getLastViewMode() == 1) {  // 1 表示 SingleView
                window.switchToSingleView(window.getLastImageIndex());
            } else {
                window.switchToThumbnailView();
            }
        }
    }

    window.show();

    return app.exec();
}
