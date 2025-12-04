#include "imagewidget.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("PictureView");
    app.setApplicationVersion("1.4.2.1");
    app.setOrganizationName("berylok");

    // 创建翻译器
    QTranslator appTranslator;
    QTranslator qtTranslator;

    // 设置默认语言（中文）
    QString locale = "zh_CN";

    // 检查命令行参数是否指定语言
    QCommandLineParser parser;
    QCommandLineOption langOption("lang", "Set language (zh_CN, en_US)", "language");
    parser.addOption(langOption);
    parser.process(app);

    if (parser.isSet(langOption)) {
        locale = parser.value(langOption);
    }

    // 加载Qt自带的标准对话框翻译
    QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qtTranslator.load("qt_" + locale, qtTranslationsPath)) {
        app.installTranslator(&qtTranslator);
    }

    // 加载应用程序翻译
    QString appTranslationsPath = QApplication::applicationDirPath() + "/translations";
    if (appTranslator.load("PictureView_" + locale, appTranslationsPath)) {
        app.installTranslator(&appTranslator);
        qDebug() << "Loaded translation for locale:" << locale;
    } else {
        qDebug() << "Failed to load translation for locale:" << locale;
        // 尝试从资源文件加载
        if (appTranslator.load(":/translations/PictureView_" + locale)) {
            app.installTranslator(&appTranslator);
            qDebug() << "Loaded translation from resources for locale:" << locale;
        }
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
            qWarning() << QCoreApplication::translate("main",
                                                      "File does not exist:")
                       << filePath;
            QMessageBox::warning(
                nullptr, QCoreApplication::translate("main", "Error"),
                QCoreApplication::translate("main", "File does not exist:\n%1")
                                                                           .arg(filePath));
        }
    }

    window.show();

    return app.exec();
}
