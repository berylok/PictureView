// imagewidget_help.cpp
#include "imagewidget.h"
#include <QMessageBox>
#include <QApplication>
#include <QFile>

#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QDialog>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

QString ImageWidget::getShortPathName(const QString &longPath)
{
#ifdef Q_OS_WIN
    wchar_t shortPath[MAX_PATH];
    if (GetShortPathName(longPath.toStdWString().c_str(), shortPath, MAX_PATH) > 0) {
        return QString::fromWCharArray(shortPath);
    }
#endif
    return longPath;
}

void ImageWidget::registerFileAssociation(const QString &fileExtension,
                                          const QString &fileTypeName,
                                          const QString &openCommand)
{
#ifdef Q_OS_WIN
    QString keyName = QString(".%1").arg(fileExtension);
    HKEY hKey;
    if (RegCreateKeyEx(
            HKEY_CLASSES_ROOT, reinterpret_cast<const wchar_t *>(keyName.utf16()),
            0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                      reinterpret_cast<const BYTE *>(fileTypeName.utf16()),
                      (fileTypeName.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    keyName = QString("%1").arg(fileTypeName);
    if (RegCreateKeyEx(
            HKEY_CLASSES_ROOT, reinterpret_cast<const wchar_t *>(keyName.utf16()),
            0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                      reinterpret_cast<const BYTE *>(fileTypeName.utf16()),
                      (fileTypeName.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    keyName = QString("%1\\shell\\open\\command").arg(fileTypeName);
    if (RegCreateKeyEx(
            HKEY_CLASSES_ROOT, reinterpret_cast<const wchar_t *>(keyName.utf16()),
            0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                      reinterpret_cast<const BYTE *>(openCommand.utf16()),
                      (openCommand.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
#endif
}

void ImageWidget::updateWindowTitle()
{
    QString title;

    if (canvasMode) {
        // 画布模式下的标题
        title = tr("画布模式 - 按 Insert 或 ESC 退出");
        if (!currentImagePath.isEmpty()) {
            QFileInfo fileInfo(currentImagePath);
            title += QString(" - %1").arg(fileInfo.fileName());
        }
    } else if (!currentImagePath.isEmpty()) {
        QFileInfo fileInfo(currentImagePath);
        title = QString(tr("%1 (%2/%3) - %4模式"))
                    .arg(fileInfo.fileName())
                    .arg(currentImageIndex + 1)
                    .arg(imageList.size())
                    .arg(currentViewMode == SingleView ? tr("单张") : tr("缩略图"));
    } else if (!pixmap.isNull()) {
        title = tr("剪贴板图片");
    } else {
        title = tr("图片查看器 - 缩略图模式");
    }

    if (isSlideshowActive) {
        title += " [幻灯中]";
    }

    setWindowTitle(title);
}

void ImageWidget::showAboutDialog()
{
    // 在画布模式下临时禁用鼠标穿透
    bool wasPassthrough = mousePassthrough;
    if (canvasMode) {
        disableMousePassthrough();
    }

    // 创建自定义对话框
    QDialog helpDialog(this);
    helpDialog.setWindowTitle(tr("图片查看器 - 帮助"));
    helpDialog.setMinimumSize(600, 800);  // 稍微增加尺寸以适应更多内容
    helpDialog.setMaximumSize(1100, 1100);

    // 设置对话框样式
    helpDialog.setStyleSheet(
        "QDialog { "
        "   background-color: white; "
        "   color: black; "
        "}"
        "QTabWidget::pane { "
        "   border: 1px solid #cccccc; "
        "   background-color: white; "
        "}"
        "QTabWidget::tab-bar { "
        "   alignment: center; "
        "}"
        "QTabBar::tab { "
        "   background-color: #f0f0f0; "
        "   border: 1px solid #cccccc; "
        "   padding: 8px 16px; "
        "   margin-right: 2px; "
        "   border-top-left-radius: 4px; "
        "   border-top-right-radius: 4px; "
        "}"
        "QTabBar::tab:selected { "
        "   background-color: #0078d4; "
        "   color: white; "
        "}"
        "QTabBar::tab:hover { "
        "   background-color: #e0e0e0; "
        "}"
        "QLabel { "
        "   color: black; "
        "   background-color: transparent; "
        "}"
        "QTextEdit { "
        "   background-color: #f8f8f8; "
        "   border: 1px solid #dddddd; "
        "   border-radius: 3px; "
        "   padding: 8px; "
        "   color: #333333; "
        "   font-size: 9pt; "
        "   selection-background-color: #0078d4; "
        "}"
        "QPushButton { "
        "   background-color: #0078d4; "
        "   color: white; "
        "   border: none; "
        "   padding: 8px 16px; "
        "   border-radius: 4px; "
        "   font-weight: bold; "
        "   min-width: 80px; "
        "}"
        "QPushButton:hover { "
        "   background-color: #106ebe; "
        "}"
        "QPushButton:pressed { "
        "   background-color: #005a9e; "
        "}"
        );

    QVBoxLayout *mainLayout = new QVBoxLayout(&helpDialog);

    // 创建标签页
    QTabWidget *tabWidget = new QTabWidget(&helpDialog);

    // === 关于标签页 ===
    QWidget *aboutTab = new QWidget();
    QVBoxLayout *aboutLayout = new QVBoxLayout(aboutTab);

    // 标题区域
    QLabel *titleLabel = new QLabel(tr("图片查看器"), aboutTab);
    titleLabel->setStyleSheet(
        "QLabel { "
        "   font-size: 28px; "
        "   font-weight: bold; "
        "   color: #0078d4; "
        "   padding: 15px; "
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                               stop:0 #0078d4, stop:1 #00bcf2); "
        "   color: white; "
        "   border-radius: 8px; "
        "   margin-bottom: 10px; "
        "}"
        );
    titleLabel->setAlignment(Qt::AlignCenter);

    // 版本信息
    QLabel *versionLabel = new QLabel(tr("版本 1.5.0"), aboutTab);
    versionLabel->setStyleSheet("font-size: 14px; color: #666666; font-weight: bold;");
    versionLabel->setAlignment(Qt::AlignCenter);

    QLabel *developerLabel = new QLabel(tr("技术顾问：DeepSeek 图标创意：DouBao 开发者: berylok(幸运人的珠宝) "), aboutTab);
    developerLabel->setStyleSheet("font-size: 12px; color: #888888;");
    developerLabel->setAlignment(Qt::AlignCenter);

    // 功能特性
    QTextEdit *featuresText = new QTextEdit(aboutTab);
    featuresText->setReadOnly(true);
    featuresText->setHtml(
        "<h3>" + tr("主要特性") + "</h3>"
        "<table width='100%' cellspacing='5' cellpadding='5'>"
        "<tr><td width='50%'><b>📁 " + tr("文件管理") + "</b></td><td width='50%'><b>🎨 " + tr("视图功能") + "</b></td></tr>"
        "<tr><td>• " + tr("支持多种图片格式") + "</td><td>• " + tr("缩略图浏览模式") + "</td></tr>"
        "<tr><td>• " + tr("压缩包直接查看") + "</td><td>• " + tr("单张图片模式") + "</td></tr>"
        "<tr><td>• " + tr("文件夹拖拽支持") + "</td><td>• " + tr("画布透明模式") + "</td></tr>"
        "<tr><td>• " + tr("回收站安全删除") + "</td><td>• " + tr("幻灯片播放") + "</td></tr>"
        "<tr><td colspan='2'><b>⚡ " + tr("编辑功能") + "</b></td></tr>"
        "<tr><td colspan='2'>• " + tr("图片旋转和镜像") + "</td></tr>"
        "<tr><td colspan='2'>• " + tr("剪贴板操作") + "</td></tr>"
        "<tr><td colspan='2'>• " + tr("图片保存和导出") + "</td></tr>"
        "</table>"

        "<h3>" + tr("支持格式") + "</h3>"
        "<p><b>" + tr("图片格式:") + "</b> PNG, JPG, JPEG, BMP, WEBP, GIF, TIFF, TIF</p>"
        "<p><b>" + tr("压缩包格式:") + "</b> ZIP, RAR, 7Z, TAR, GZ, BZ2</p>"

        "<h3>" + tr("系统要求") + "</h3>"
        "<p>• " + tr("操作系统: Windows 7 或更高版本 / Ubuntu 20.04 及衍生版（以及其他Linux发行版）") + "</p>"
        "<p>• " + tr("桌面环境: 支持X11的桌面环境（GNOME、KDE、XFCE等）") + "</p>"
        "<p>• " + tr("内存: 至少 512MB RAM") + "</p>"
        "<p>• " + tr("磁盘空间: 至少 50MB 可用空间") + "</p>"

        "<h3>" + tr("开源组件") + "</h3>"
        "<p>• <b>Qt 框架</b> - " + tr("跨平台应用程序开发框架") + "</p>"
        "<p>• <b>zlib</b> - " + tr("数据压缩库") + "</p>"
        "<p>• <b>libpng</b> - " + tr("PNG图像处理库") + "</p>"
        "<p>• <b>libjpeg-turbo</b> - " + tr("JPEG图像处理库") + "</p>"
        "<p>• <b>libarchive</b> - " + tr("读写压缩格式") + "</p>"
        );

    aboutLayout->addWidget(titleLabel);
    aboutLayout->addWidget(versionLabel);
    aboutLayout->addWidget(developerLabel);
    aboutLayout->addWidget(featuresText);

    // === 快捷键标签页 ===
    QWidget *shortcutsTab = new QWidget();
    QVBoxLayout *shortcutsLayout = new QVBoxLayout(shortcutsTab);

    QTextEdit *shortcutsText = new QTextEdit(shortcutsTab);
    shortcutsText->setReadOnly(true);
    shortcutsText->setHtml(
        "<h2 style='color: #0078d4;'>" + tr("快捷键参考") + "</h2>"

        "<h3>📁 " + tr("文件操作") + "</h3>"
        "<table border='0' cellspacing='5' cellpadding='5' width='100%'>"
        "<tr><td width='40%'><b>Ctrl + O</b></td><td width='60%'>" + tr("打开文件夹") + "</td></tr>"
        "<tr><td><b>Ctrl + Shift + O</b></td><td>" + tr("打开单个图片") + "</td></tr>"
        "<tr><td><b>Ctrl + S</b></td><td>" + tr("保存图片") + "</td></tr>"
        "<tr><td><b>Ctrl + C</b></td><td>" + tr("复制图片到剪贴板") + "</td></tr>"
        "<tr><td><b>Ctrl + V</b></td><td>" + tr("从剪贴板粘贴图片") + "</td></tr>"
        "<tr><td><b>Delete</b></td><td>" + tr("删除当前图片到回收站") + "</td></tr>"
        "</table>"

        "<h3>👁️ " + tr("视图导航") + "</h3>"
        "<table border='0' cellspacing='5' cellpadding='5' width='100%'>"
        "<tr><td width='40%'><b>← →</b></td><td width='60%'>" + tr("上一张/下一张图片") + "</td></tr>"
        "<tr><td><b>↑ ↓</b></td><td>" + tr("合适大小/实际大小") + "</td></tr>"
        "<tr><td><b>Home / End</b></td><td>" + tr("第一张/最后一张图片") + "</td></tr>"
        "<tr><td><b>ESC</b></td><td>" + tr("返回缩略图模式") + "</td></tr>"
        "<tr><td><b>Enter / Return</b></td><td>" + tr("进入单张视图") + "</td></tr>"
        "<tr><td><b>鼠标双击</b></td><td>" + tr("切换视图模式") + "</td></tr>"
        "</table>"

        "<h3>🎨 " + tr("图片编辑") + "</h3>"
        "<table border='0' cellspacing='5' cellpadding='5' width='100%'>"
        "<tr><td width='40%'><b>PageUp</b></td><td width='60%'>" + tr("逆时针旋转90°") + "</td></tr>"
        "<tr><td><b>PageDown</b></td><td>" + tr("顺时针旋转90°") + "</td></tr>"
        "<tr><td><b>Ctrl + PageUp</b></td><td>" + tr("垂直镜像") + "</td></tr>"
        "<tr><td><b>Ctrl + PageDown</b></td><td>" + tr("水平镜像") + "</td></tr>"
        "<tr><td><b>Ctrl + R</b></td><td>" + tr("顺时针旋转90°") + "</td></tr>"
        "<tr><td><b>Ctrl + Shift + R</b></td><td>" + tr("逆时针旋转90°") + "</td></tr>"
        "<tr><td><b>Ctrl + H</b></td><td>" + tr("水平镜像") + "</td></tr>"
        "<tr><td><b>Ctrl + Shift + V</b></td><td>" + tr("垂直镜像") + "</td></tr>"
        "<tr><td><b>Ctrl + 0</b></td><td>" + tr("重置所有变换") + "</td></tr>"
        "</table>"

        "<h3>🖼️ " + tr("画布模式") + "</h3>"
        "<table border='0' cellspacing='5' cellpadding='5' width='100%'>"
        "<tr><td width='40%'><b>Insert</b></td><td width='60%'>" + tr("进入/退出画布模式") + "</td></tr>"
        "<tr><td><b>ESC</b></td><td>" + tr("退出画布模式") + "</td></tr>"
        "<tr><td><b>PageUp</b></td><td>" + tr("增加透明度") + "</td></tr>"
        "<tr><td><b>PageDown</b></td><td>" + tr("减少透明度") + "</td></tr>"
        "<tr><td><b>菜单键</b></td><td>" + tr("显示上下文菜单") + "</td></tr>"
        "</table>"

        "<h3>🎬 " + tr("幻灯片") + "</h3>"
        "<table border='0' cellspacing='5' cellpadding='5' width='100%'>"
        "<tr><td width='40%'><b>空格键</b></td><td width='60%'>" + tr("开始/停止幻灯片") + "</td></tr>"
        "<tr><td><b>S 键</b></td><td>" + tr("停止幻灯片") + "</td></tr>"
        "</table>"

        "<h3>❓ " + tr("帮助") + "</h3>"
        "<table border='0' cellspacing='5' cellpadding='5' width='100%'>"
        "<tr><td width='40%'><b>F1</b></td><td width='60%'>" + tr("显示此帮助窗口") + "</td></tr>"
        "<tr><td><b>右键菜单</b></td><td>" + tr("快速访问常用功能") + "</td></tr>"
        "</table>"

        "<div style='background-color: #fff3cd; border: 1px solid #ffeaa7; border-radius: 4px; padding: 10px; margin-top: 15px;'>"
        "<b>💡 " + tr("使用提示:") + "</b><br>"
        "• " + tr("在画布模式下，大部分快捷键会被禁用") + "<br>"
        "• " + tr("支持鼠标拖拽文件和文件夹到窗口") + "<br>"
        "• " + tr("中键拖拽可以移动窗口位置") + "<br>"
        "• " + tr("滚轮可以缩放图片") + "<br>"
        "• " + tr("在图片左右边缘点击可以快速切换") +
        "</div>"
        );

    shortcutsLayout->addWidget(shortcutsText);

    // === 使用指南标签页 ===
    QWidget *guideTab = new QWidget();
    QVBoxLayout *guideLayout = new QVBoxLayout(guideTab);

    QTextEdit *guideText = new QTextEdit(guideTab);
    guideText->setReadOnly(true);
    guideText->setHtml(
        "<h2 style='color: #0078d4;'>" + tr("使用指南") + "</h2>"

        "<h3>🚀 " + tr("快速开始") + "</h3>"
        "<ol>"
        "<li><b>" + tr("打开图片:") + "</b> " + tr("使用 Ctrl+O 打开文件夹或 Ctrl+Shift+O 打开单个图片") + "</li>"
        "<li><b>" + tr("浏览图片:") + "</b> " + tr("在缩略图模式下使用方向键导航，在单张模式下使用左右键切换") + "</li>"
        "<li><b>" + tr("基本操作:") + "</b> " + tr("双击切换视图，滚轮缩放，右键显示菜单") + "</li>"
        "</ol>"

        "<h3>🎨 " + tr("画布模式") + "</h3>"
        "<p>" + tr("画布模式允许您将图片以半透明方式显示在屏幕上，方便参考或对比。") + "</p>"
        "<ul>"
        "<li>" + tr("进入: 在单张模式下按 Insert 键") + "</li>"
        "<li>" + tr("退出: 按 ESC 或 Insert 键") + "</li>"
        "<li>" + tr("调节透明度: 使用 PageUp/PageDown") + "</li>"
        "<li>" + tr("鼠标穿透: 在画布模式下鼠标事件会穿透到下层程序") + "</li>"
        "</ul>"

        "<h3>📦 " + tr("压缩包支持") + "</h3>"
        "<p>" + tr("可以直接打开 ZIP、RAR 等压缩包文件，无需解压即可浏览其中的图片。") + "</p>"
        "<ul>"
        "<li>" + tr("支持格式: ZIP, RAR, 7Z, TAR, GZ, BZ2") + "</li>"
        "<li>" + tr("使用方法: 直接双击压缩包文件或从文件夹中打开") + "</li>"
        "<li>" + tr("退出压缩包: 按 ESC 键或使用右键菜单") + "</li>"
        "</ul>"

        "<h3>⚙️ " + tr("窗口定制") + "</h3>"
        "<p>" + tr("通过右键菜单可以自定义窗口行为:") + "</p>"
        "<ul>"
        "<li>" + tr("隐藏/显示标题栏") + "</li>"
        "<li>" + tr("窗口置顶") + "</li>"
        "<li>" + tr("透明背景") + "</li>"
        "<li>" + tr("调节窗口透明度") + "</li>"
        "</ul>"

        "<h3>🔧 " + tr("故障排除") + "</h3>"
        "<ul>"
        "<li><b>" + tr("图片无法加载:") + "</b> " + tr("检查文件是否损坏或格式不受支持") + "</li>"
        "<li><b>" + tr("快捷键无效:") + "</b> " + tr("确保窗口获得焦点，或在画布模式下使用专用快捷键") + "</li>"
        "<li><b>" + tr("程序无响应:") + "</b> " + tr("尝试关闭重新打开，或检查图片文件是否过大") + "</li>"
        "</ul>"
        );

    guideLayout->addWidget(guideText);

    // === 许可证标签页 ===
    QWidget *licenseTab = new QWidget();
    QVBoxLayout *licenseLayout = new QVBoxLayout(licenseTab);

    QTextEdit *licenseText = new QTextEdit(licenseTab);
    licenseText->setReadOnly(true);
    licenseText->setHtml(
        "<h2 style='color: #0078d4;'>" + tr("软件许可证") + "</h2>"

        "<div style='background-color: #e8f4f8; border: 1px solid #b8d8e8; border-radius: 4px; padding: 15px; margin-bottom: 20px;'>"
        "<h3 style='margin-top: 0; color: #0078d4;'>" + tr("图片查看器许可证") + "</h3>"
        "<p><strong>" + tr("图片查看器") + "</strong> " + tr("基于 GNU 通用公共许可证第三版 (GPL v3) 发布。") + "</p>"
        "<p>" + tr("这是一个自由软件，您可以：") + "</p>"
        "<ul>"
        "<li>" + tr("自由地运行此程序，无论出于何种目的") + "</li>"
        "<li>" + tr("研究和修改此程序的源代码") + "</li>"
        "<li>" + tr("重新分发此程序的副本") + "</li>"
        "<li>" + tr("分发您修改后的版本") + "</li>"
        "</ul>"
        "</div>"

        "<h3>📜 " + tr("GPL v3 条款摘要") + "</h3>"
        "<p><strong>" + tr("重要提示：这不是完整的许可证文本，仅供参考。完整许可证请访问 ") +
        "<a href='https://www.gnu.org/licenses/gpl-3.0.html'>https://www.gnu.org/licenses/gpl-3.0.html</a></strong></p>"

        "<table border='0' cellspacing='5' cellpadding='5' width='100%' style='margin-bottom: 15px;'>"
        "<tr style='background-color: #f5f5f5;'>"
        "<td width='30%'><b>" + tr("您的自由") + "</b></td>"
        "<td width='70%'><b>" + tr("相应义务") + "</b></td>"
        "</tr>"

        "<tr>"
        "<td>" + tr("使用自由") + "</td>"
        "<td>" + tr("无限制 - 您可以出于任何目的运行此软件") + "</td>"
        "</tr>"

        "<tr>"
        "<td>" + tr("学习与修改自由") + "</td>"
        "<td>" + tr("必须保持许可证声明和版权信息") + "</td>"
        "</tr>"

        "<tr>"
        "<td>" + tr("重新分发自由") + "</td>"
        "<td>" + tr("必须提供完整源代码，包括所有修改") + "</td>"
        "</tr>"

        "<tr>"
        "<td>" + tr("分发修改版本") + "</td>"
        "<td>" + tr("必须同样采用GPL v3许可证") + "</td>"
        "</tr>"

        "</table>"

        "<hr style='margin: 25px 0; border: 1px solid #cccccc;'>"

        "<div style='background-color: #f0f8ff; border: 1px solid #a8d8ff; border-radius: 4px; padding: 15px; margin-bottom: 20px;'>"
        "<h3 style='margin-top: 0; color: #0078d4;'>" + tr("Qt 框架许可证") + "</h3>"
        "<p><strong>" + tr("本软件使用 Qt 框架开发。") + "</strong></p>"
        "<p>" + tr("Qt 在以下许可证下可用：") + "</p>"
        "<ul>"
        "<li><strong>" + tr("GNU LGPL v3:") + "</strong> " + tr("用于开源开发") + "</li>"
        "<li><strong>" + tr("商业许可证:") + "</strong> " + tr("用于商业和专有开发") + "</li>"
        "</ul>"

        "<p>" + tr("本软件基于 Qt 的开源版本构建，遵循 LGPL v3 许可证条款。") + "</p>"
        "<p>" + tr("Qt 是 The Qt Company Ltd. 的注册商标。") + "</p>"
        "</div>"

        "<h3>🔗 " + tr("许可证链接") + "</h3>"
        "<ul>"
        "<li>" + tr("GPL v3 完整文本: ") +
        "<a href='https://www.gnu.org/licenses/gpl-3.0.html'>https://www.gnu.org/licenses/gpl-3.0.html</a></li>"
        "<li>" + tr("LGPL v3 完整文本: ") +
        "<a href='https://www.gnu.org/licenses/lgpl-3.0.html'>https://www.gnu.org/licenses/lgpl-3.0.html</a></li>"
        "<li>" + tr("Qt 许可证信息: ") +
        "<a href='https://www.qt.io/licensing/'>https://www.qt.io/licensing/</a></li>"
        "<li>" + tr("自由软件基金会: ") +
        "<a href='https://www.fsf.org/'>https://www.fsf.org/</a></li>"
        "</ul>"

        "<h3>⚖️ " + tr("重要声明") + "</h3>"
        "<div style='background-color: #fff3cd; border: 1px solid #ffeaa7; border-radius: 4px; padding: 15px;'>"
        "<p><strong>" + tr("免责声明:") + "</strong> " +
        tr("本软件按原样提供，不提供任何明示或暗示的担保，包括但不限于对适销性和特定用途适用性的暗示担保。") + "</p>"
        "<p><strong>" + tr("版权声明:") + "</strong> " +
        tr("版权所有 © 2024 berylok(幸运人的珠宝)。保留所有权利。") + "</p>"
        "<p><strong>" + tr("Qt 声明:") + "</strong> " +
        tr("Qt 是 The Qt Company Ltd. 及其子公司和关联公司的注册商标。") + "</p>"
        "</div>"

        "<div style='background-color: #e8f5e9; border: 1px solid #c8e6c9; border-radius: 4px; padding: 15px; margin-top: 20px;'>"
        "<h4 style='margin-top: 0;'>" + tr("开源贡献") + "</h4>"
        "<p>" + tr("此软件是开源社区的一部分。我们鼓励：") + "</p>"
        "<ul>"
        "<li>" + tr("报告错误和改进建议") + "</li>"
        "<li>" + tr("贡献代码改进") + "</li>"
        "<li>" + tr("翻译和本地化") + "</li>"
        "<li>" + tr("分享使用体验") + "</li>"
        "</ul>"
        "<p>" + tr("感谢您支持开源软件！") + "</p>"
        "</div>"
        );

    licenseLayout->addWidget(licenseText);

    // 添加标签页
    tabWidget->addTab(aboutTab, tr("关于"));
    tabWidget->addTab(shortcutsTab, tr("快捷键"));
    tabWidget->addTab(guideTab, tr("使用指南"));
    tabWidget->addTab(licenseTab, tr("许可证"));

    // 按钮区域
    QDialogButtonBox *buttonBox = new QDialogButtonBox(&helpDialog);
    QPushButton *closeButton = new QPushButton(tr("关闭"), &helpDialog);
    buttonBox->addButton(closeButton, QDialogButtonBox::AcceptRole);

    connect(closeButton, &QPushButton::clicked, &helpDialog, &QDialog::accept);

    // 主布局
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttonBox);

    // 设置对话框属性
    helpDialog.setWindowFlags(helpDialog.windowFlags() & ~Qt::FramelessWindowHint);
    helpDialog.setAttribute(Qt::WA_NoSystemBackground, false);
    helpDialog.setAttribute(Qt::WA_TranslucentBackground, false);
    helpDialog.setAutoFillBackground(true);

    // 显示对话框
    helpDialog.exec();

    // 恢复鼠标穿透状态
    if (wasPassthrough && canvasMode) {
        enableMousePassthrough();
    }
}

void ImageWidget::showShortcutHelp()
{
    // 现在 F1 已经整合了所有帮助，这个方法可以重定向到 F1 帮助
    showAboutDialog();
}
