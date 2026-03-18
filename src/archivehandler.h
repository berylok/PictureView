#ifndef ARCHIVEHANDLER_H
#define ARCHIVEHANDLER_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <archive.h>
#include <archive_entry.h>

class ArchiveHandler
{
public:
    ArchiveHandler();
    ~ArchiveHandler();

    // 检查文件是否是支持的压缩格式
    static bool isSupportedArchive(const QString &filePath);

    // 打开压缩包
    bool openArchive(const QString &filePath);

    // 关闭压缩包
    void closeArchive();

    // 获取压缩包中的图片文件列表
    QStringList getImageFiles();

    // 从压缩包中提取文件到内存
    QByteArray extractFile(const QString &filePath);

    // 获取压缩包基本信息
    QString getArchivePath() const { return archivePath; }
    bool isOpen() const { return archive != nullptr; }

private:
    struct archive *archive;
    QString archivePath;

    // 检查文件是否是图片
    bool isImageFile(const QString &fileName);
};

#endif // ARCHIVEHANDLER_H
