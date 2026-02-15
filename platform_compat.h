#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#include <QtGlobal>
#include <QString>
#include <QThread>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QTextStream>
#include <QMessageBox>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#else
// Linux 类型定义
typedef unsigned short WCHAR;
typedef int BOOL;
typedef void* HMODULE;
typedef unsigned long DWORD;
typedef const char* LPCSTR;
typedef char* LPSTR;

#define MAX_PATH 260
#define SW_SHOW 5
#define SW_SHOWNORMAL 1
#define FO_DELETE 0x0003
#define FOF_ALLOWUNDO 0x0040
#define FOF_NOERRORUI 0x0400
#define FOF_SILENT 0x0004
#define FOF_NOCONFIRMATION 0x0010

// Windows 结构体模拟
typedef struct _SHFILEOPSTRUCTA {
    void *hwnd;
    unsigned int wFunc;
    const char *pFrom;
    const char *pTo;
    unsigned short fFlags;
    int fAnyOperationsAborted;
    void *hNameMappings;
    const char *lpszProgressTitle;
} SHFILEOPSTRUCTA, *LPSHFILEOPSTRUCTA;

#define SHFILEOPSTRUCT SHFILEOPSTRUCTA
#define LPSHFILEOPSTRUCT LPSHFILEOPSTRUCTA
#endif

class PlatformCompat {
public:
    // 睡眠函数
    static void sleep(unsigned long ms) {
#ifdef _WIN32
        Sleep(ms);
#else
        QThread::msleep(ms);
#endif
    }

    // 打开文件或URL
    static bool openFile(const QString& filePath) {
#ifdef _WIN32
        return (intptr_t)ShellExecuteA(NULL, "open", filePath.toLocal8Bit().constData(), NULL, NULL, SW_SHOW) > 32;
#else
        return QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
#endif
    }

    // 在资源管理器中显示文件
    static bool showInFolder(const QString& filePath) {
#ifdef _WIN32
        QString param = "/select," + QDir::toNativeSeparators(filePath);
        return (intptr_t)ShellExecuteA(0, "open", "explorer", param.toLocal8Bit().constData(), NULL, SW_SHOWNORMAL) > 32;
#else
        QFileInfo fileInfo(filePath);
        QString folder = fileInfo.path();
        QString fileName = fileInfo.fileName();

        // 定义支持的文件管理器及其参数
        struct {
            const char* name;
            QStringList args;
        } managers[] = {
            {"nautilus", {"--select", filePath}},   // GNOME
            {"dolphin",  {"--select", filePath}},   // KDE
            {"nemo",     {"--select", filePath}},   // Cinnamon
            {"thunar",   {folder}},                 // XFCE（不支持选中，仅打开文件夹）
            {"pcmanfm",  {folder}},                 // LXDE（同上）
        };

        for (const auto& mgr : managers) {
            if (QProcess::execute("which", {mgr.name}) == 0) {
                return QProcess::startDetached(mgr.name, mgr.args);
            }
        }

        // 终极回退：用默认应用打开文件夹
        return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
    }

    // 移动到回收站
    static bool moveToRecycleBin(const QString& filePath) {
#ifdef _WIN32
        WCHAR wcPath[MAX_PATH];
        memset(wcPath, 0, sizeof(wcPath));
        filePath.toWCharArray(wcPath);
        wcPath[filePath.length()] = '\0';

        SHFILEOPSTRUCT fileOp;
        memset(&fileOp, 0, sizeof(fileOp));
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = wcPath;
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION;

        return SHFileOperation(&fileOp) == 0;
#else \
    // Linux 实现
        if (QProcess::execute("which", {"trash-put"}) == 0) {
            return QProcess::execute("trash-put", {filePath}) == 0;
        }
        if (QProcess::execute("which", {"gio"}) == 0) {
            return QProcess::execute("gio", {"trash", filePath}) == 0;
        }

        // 手动实现回收站
        return moveToTrashManual(filePath);
#endif
    }

private:
#ifdef __linux__
    static bool moveToTrashManual(const QString& filePath) {
        QString homeDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        QString trashFiles = homeDir + "/.local/share/Trash/files/";
        QString trashInfo = homeDir + "/.local/share/Trash/info/";

        // 创建回收站目录
        QDir().mkpath(trashFiles);
        QDir().mkpath(trashInfo);

        QFileInfo srcInfo(filePath);
        QString fileName = srcInfo.fileName();
        QString destFile = trashFiles + fileName;

        // 处理重名文件
        int counter = 1;
        while (QFile::exists(destFile)) {
            QString baseName = srcInfo.completeBaseName();
            QString suffix = srcInfo.suffix();
            fileName = QString("%1 %2").arg(baseName).arg(counter);
            if (!suffix.isEmpty()) {
                fileName += "." + suffix;
            }
            destFile = trashFiles + fileName;
            counter++;
        }

        // 创建 .trashinfo 文件
        QString infoFile = trashInfo + fileName + ".trashinfo";
        QFile info(infoFile);
        if (info.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&info);
            out << "[Trash Info]\n";
            out << "Path=" << filePath << "\n";
            out << "DeletionDate=" << QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss") << "\n";
            info.close();
        }

        // 移动文件
        return QFile::rename(filePath, destFile);
    }
#endif
};

// Windows API 函数模拟
#ifndef _WIN32
inline int SHFileOperation(LPSHFILEOPSTRUCT lpFileOp) {
    // Linux 下的简单实现 - 直接删除
    QFile file(lpFileOp->pFrom);
    return file.remove() ? 0 : 1;
}
#endif

#endif // PLATFORM_COMPAT_H
