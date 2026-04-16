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
    // QString locale = "en_US";  //硬性加载英文


    // 检查系统语言是否在我们支持的列表中
    QStringList supportedLocales = {
        "zh_CN",    // 中文（简体）        Chinese(China)
        "en_US",    // 英文（美国）        English(United States)
        "ja_JP",    // 日文                Japanese(Japan)
        "ko_KR",    // 韩文                Korean(South Korea)
        "fr_FR",    // 法文                French(France)
        "de_AT",    // 德文（奥地利）       German(Germany)
        "es_AR",    // 西班牙文（阿根廷） Spanish(Argentina)
        "ru_RU",    //俄罗斯           Russian(Russia)
        "pt_BR",    //巴西葡语          Portuguese(Brazil)
        "pt_PT",    //欧洲葡语          Portuguese(Portugal)
        "zh_TW",    //中国台湾（繁体）      Chinese(Taiwan)
        "fr_CA",    //加拿大法语         French(Canada)
        "it_IT",    //意大利           Italian(Italy)
        "en_CA",    //加拿大英语         English(canada)
        "ar_SA",    //阿拉伯语          Arabic(Saudi Arabia)
        "hi_IN",    //印地语           Hindi(India)
        "id_ID",    //印尼语           Indonesian(Indonesia)
        "nl_NL",    //荷兰语           Dutch(Netherlands)
        "pl_PL",    //波兰语           Polish(Poland)
        "tr_TR",    //土耳其语          Turkish (Turkey)
        "th_TH",    //泰语（泰国）        Thai (Thailand)
        "vi_VN"     //越南语（越南）       Vietnamese (Vietnam)

    };

    // 获取系统 locale
    QString sysLocale = QLocale::system().name();  // 如 "zh_CN", "en_US"
    QString locale = "en_US";  // 默认

    // 先检查是否精确匹配支持列表
    if (supportedLocales.contains(sysLocale)) {
        locale = sysLocale;
    } else {
        // 否则尝试匹配语言代码（前两位）
        QString sysLang = sysLocale.left(2);  // 如 "zh", "de"
        for (const QString &sup : supportedLocales) {
            if (sup.left(2) == sysLang) {
                locale = sup;
                break;
            }
        }
    }


    // 命令行参数覆盖
    QCommandLineParser parser;
    QCommandLineOption langOption("lang", "Override system language (e.g., zh_CN, ru_RU)", "language");
    parser.addOption(langOption);
    parser.process(app);
    if (parser.isSet(langOption)) {
        locale = parser.value(langOption);
    }

    qDebug() << "Selected locale:" << locale;

    // ========== 加载翻译文件 ==========
    QTranslator appTranslator;
    QTranslator qtTranslator;

    // 1. 加载 Qt 自带翻译（如文件对话框的按钮）
    QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qtTranslator.load("qt_" + locale, qtTranslationsPath)) {
        app.installTranslator(&qtTranslator);
        qDebug() << "✅ Loaded Qt translation for" << locale;
    } else {
        qDebug() << "⚠️ Failed to load Qt translation for" << locale;
    }

    // 2. 加载应用程序翻译（支持多路径搜索）
    QStringList searchPaths;
    searchPaths << QApplication::applicationDirPath() + "/translations"                 // 开发环境
                << QApplication::applicationDirPath() + "/../share/PictureView/translations" // AppImage 内部
                << ":/translations";                                                     // 资源文件

    QString appTranslationsPath;
    for (const QString &path : searchPaths) {
        if (QDir(path).exists()) {
            appTranslationsPath = path;
            break;
        }
    }
    qDebug() << "Using translation path:" << appTranslationsPath;

    if (appTranslator.load("PictureView_" + locale, appTranslationsPath)) {
        app.installTranslator(&appTranslator);
        qDebug() << "✅ Loaded application translation for" << locale;
    } else {
        qDebug() << "❌ Failed to load application translation for" << locale
                 << "from" << appTranslationsPath;
    }


    // 注册文件关联选项
    QCommandLineOption registerOption(
        "register",
        ("main", "Register file associations"));
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

        qDebug() << ("main", "File associations registered");
        return 0;
    }

    ImageWidget window;

    // 加载配置（必须在处理命令行参数之前）
    window.loadConfiguration();

    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        qDebug() << ("main", "Opening file:") << filePath;

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
