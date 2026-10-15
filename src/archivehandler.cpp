#include "archivehandler.h"
#include <QFileInfo>
#include <QDebug>
#include <QMutexLocker>

ArchiveHandler::ArchiveHandler() : archive(nullptr)
{
}

ArchiveHandler::~ArchiveHandler()
{
    // 析构时不需要锁，因为此时没有其他线程应该访问
    closeArchiveInternal();
}

bool ArchiveHandler::isSupportedArchive(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    return (suffix == "zip" || suffix == "rar" || suffix == "7z" ||
            suffix == "tar" || suffix == "gz" || suffix == "bz2");
}

bool ArchiveHandler::openArchive(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);

    closeArchiveInternal();   // ★ 不调用 closeArchive（避免死锁）

    archive = archive_read_new();
    archive_read_support_format_all(archive);
    archive_read_support_filter_all(archive);

    int r = archive_read_open_filename(
        archive, filePath.toLocal8Bit().constData(), 10240);
    if (r != ARCHIVE_OK) {
        archive_read_free(archive);
        archive = nullptr;
        return false;
    }

    archivePath = filePath;
    return true;
}

void ArchiveHandler::closeArchive()
{
    QMutexLocker locker(&m_mutex);
    closeArchiveInternal();
}

void ArchiveHandler::closeArchiveInternal()
{
    if (archive) {
        archive_read_close(archive);
        archive_read_free(archive);
        archive = nullptr;
    }
    archivePath.clear();
}

QString ArchiveHandler::getArchivePath() const
{
    QMutexLocker locker(&m_mutex);
    return archivePath;
}

bool ArchiveHandler::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return archive != nullptr;
}

QStringList ArchiveHandler::getImageFiles()
{
    QMutexLocker locker(&m_mutex);

    QStringList imageFiles;
    if (!archive) return imageFiles;

    // 拷一份 archivePath，防止中途被改
    const QString localPath = archivePath;

    // 重置到开始位置
    archive_read_free(archive);
    archive = archive_read_new();
    archive_read_support_format_all(archive);
    archive_read_support_filter_all(archive);
    archive_read_open_filename(archive, localPath.toLocal8Bit().constData(), 10240);

    struct archive_entry *entry;
    int entryCount = 0;
    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(entry);
        if (filename) {
            QString filePath = QString::fromUtf8(filename);
            entryCount++;
            if (isImageFile(filePath)) {
                imageFiles.append(filePath);
            }
        }
        archive_read_data_skip(archive);
    }

    // 重新打开以准备后续读取
    archive_read_free(archive);
    archive = archive_read_new();
    archive_read_support_format_all(archive);
    archive_read_support_filter_all(archive);
    archive_read_open_filename(archive, localPath.toLocal8Bit().constData(), 10240);

    return imageFiles;
}

QByteArray ArchiveHandler::extractFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);

    QByteArray data;
    if (!archive) return data;

    // 拷一份 archivePath
    const QString localPath = archivePath;

    // 使用新的 archive 实例
    struct archive *tempArchive = archive_read_new();
    archive_read_support_format_all(tempArchive);
    archive_read_support_filter_all(tempArchive);

    int r = archive_read_open_filename(tempArchive, localPath.toLocal8Bit().constData(), 10240);
    if (r != ARCHIVE_OK) {
        archive_read_free(tempArchive);
        return data;
    }

    struct archive_entry *entry;
    bool found = false;

    while (archive_read_next_header(tempArchive, &entry) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(entry);
        if (filename) {
            QString currentFile = QString::fromUtf8(filename);
            if (currentFile == filePath) {
                found = true;

                const void *buff;
                size_t size;
                la_int64_t offset;
                while (archive_read_data_block(tempArchive, &buff, &size, &offset) == ARCHIVE_OK) {
                    data.append(static_cast<const char *>(buff), size);
                }
                break;
            }
        }
        archive_read_data_skip(tempArchive);
    }

    archive_read_close(tempArchive);
    archive_read_free(tempArchive);

    return data;
}

bool ArchiveHandler::isImageFile(const QString &fileName)
{
    QString lowerName = fileName.toLower();
    return (lowerName.endsWith(".png") || lowerName.endsWith(".jpg") ||
            lowerName.endsWith(".jpeg") || lowerName.endsWith(".bmp") ||
            lowerName.endsWith(".webp") || lowerName.endsWith(".gif") ||
            lowerName.endsWith(".tiff") || lowerName.endsWith(".tif"));
}