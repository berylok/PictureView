
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

#include <QStyleFactory>
#include <QPalette>

int main(int argc, char *argv[])
{
    // === 在创建QApplication之前先检测环境变量 ===
    //qDebug() << "=== 程序启动 - 环境检测 ===";
    //qDebug() << "命令行参数:";
    for (int i = 0; i < argc; ++i) {
        //qDebug() << "  argv[" << i << "]:" << argv[i];
    }

    // 创建QApplication
    QApplication app(argc, argv);

    // // 获取应用程序的调色板
    // QPalette palette = app.palette();

    // // 设置焦点高亮颜色为蓝色
    // palette.setColor(QPalette::Active, QPalette::Highlight, QColor(0, 120, 215));     // 选中项背景色
    // palette.setColor(QPalette::Active, QPalette::HighlightedText, Qt::white);         // 选中项文字色
    // palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(0, 120, 215));
    // palette.setColor(QPalette::Inactive, QPalette::HighlightedText, Qt::white);

    // app.setPalette(palette);

    // 设置更高的内存分配限制（512MB）
    QImageReader::setAllocationLimit(512);
    //qDebug() << "设置内存分配限制为 512MB";

    // === 在设置应用程序信息后检测Qt平台 ===
    // 设置应用程序信息
    app.setApplicationName("PictureView");
    app.setApplicationVersion("1.6.0");
    app.setOrganizationName("berylok");

    // 打印环境信息
    //qDebug() << "=== 环境检测结果 ===";
    //qDebug() << "Qt运行平台:" << QGuiApplication::platformName();
    //qDebug() << "QT_QPA_PLATFORM环境变量:" << qgetenv("QT_QPA_PLATFORM");
    //qDebug() << "XDG_SESSION_TYPE环境变量:" << qgetenv("XDG_SESSION_TYPE");
    //qDebug() << "DISPLAY环境变量:" << qgetenv("DISPLAY");
    //qDebug() << "==================";

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

    // ========== 加载翻译文件 ==========
    QTranslator appTranslator;
    QTranslator qtTranslator;

    // 1. 加载 Qt 自带翻译（如文件对话框的按钮）
    QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qtTranslator.load("qt_" + locale, qtTranslationsPath)) {
        app.installTranslator(&qtTranslator);
    } else {
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


    if (appTranslator.load("PictureView_" + locale, appTranslationsPath)) {
        app.installTranslator(&appTranslator);
    } else {

    }


    // 注册文件关联选项
    QCommandLineOption registerOption(
        "register",
        QCoreApplication::translate("main", "Register file associations"));
    parser.addOption(registerOption);

    parser.process(app);

    if (parser.isSet(registerOption)) {
        QString exePath = QCoreApplication::applicationFilePath();
        QString shortExePath = ImageWidget::getShortPathName(exePath);        // ✅ 直接调
        QString openCommand = QString("\"%1\" \"%2\"").arg(shortExePath).arg("%1");

        ImageWidget::registerFileAssociation("png", "pngfile", openCommand);   // ✅
        ImageWidget::registerFileAssociation("jpg", "jpgfile", openCommand);   // ✅
        ImageWidget::registerFileAssociation("bmp", "bmpfile", openCommand);
        ImageWidget::registerFileAssociation("jpeg", "jpegfile", openCommand);
        ImageWidget::registerFileAssociation("webp", "webpfile", openCommand);
        ImageWidget::registerFileAssociation("gif", "giffile", openCommand);
        ImageWidget::registerFileAssociation("tiff", "tifffile", openCommand);
        ImageWidget::registerFileAssociation("tif", "tiffile", openCommand);

        return 0;
    }

    ImageWidget window;

    // 加载配置（必须在处理命令行参数之前）
    window.loadConfiguration();

    QColor hl(window.currentConfig.highlightColor);
    if (hl.isValid()) {
        QPalette p = app.palette();
        p.setColor(QPalette::Active,   QPalette::Highlight, hl);
        p.setColor(QPalette::Inactive, QPalette::Highlight, hl);
        // 文字颜色：亮色底用黑字，暗色底用白字
        Qt::GlobalColor text = (hl.lightness() > 128) ? Qt::black : Qt::white;
        p.setColor(QPalette::Active,   QPalette::HighlightedText, text);
        p.setColor(QPalette::Inactive, QPalette::HighlightedText, text);
        app.setPalette(p);
    }

    bool fileHandled = false;
    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        if (QFile::exists(filePath)) {
            QFileInfo fileInfo(filePath);
            if (fileInfo.isDir()) {
                window.setCurrentDir(QDir(filePath));
                window.loadImageList();
            } else {
                window.loadImage(filePath);
                window.switchToSingleView();
            }
            fileHandled = true;
        }
    }

    // ★ 独立判断，无论是否有参数
    if (!fileHandled) {
        // ★ 先恢复上次打开的文件夹
        const QString &lastPath = window.currentConfig.lastOpenPath;
        if (!lastPath.isEmpty() && QDir(lastPath).exists()) {
            window.setCurrentDir(QDir(lastPath));
            window.loadImageList();       // 扫描目录，填 imageList
        }

        if (window.getLastViewMode() == 1 && window.getImageCount() > 0) {
            int idx = qBound(0, window.getLastImageIndex(), window.getImageCount() - 1);
            window.switchToSingleView(idx);
        } else {
            window.switchToThumbnailView();
        }
    }

    window.show();

    return app.exec();
}
