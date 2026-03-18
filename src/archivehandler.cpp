#include "archivehandler.h"
#include <QFileInfo>
#include <QDebug>

ArchiveHandler::ArchiveHandler() : archive(nullptr)
{
}

ArchiveHandler::~ArchiveHandler()
{
    closeArchive();
}

bool ArchiveHandler::isSupportedArchive(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    // 支持的压缩格式
    return (suffix == "zip" || suffix == "rar" || suffix == "7z" ||
            suffix == "tar" || suffix == "gz" || suffix == "bz2");
}

bool ArchiveHandler::openArchive(const QString &filePath)
{
    closeArchive();

    archive = archive_read_new();
    archive_read_support_format_all(archive);
    archive_read_support_filter_all(archive);

    int r = archive_read_open_filename(
        archive, filePath.toLocal8Bit().constData(), 10240);
    if (r != ARCHIVE_OK) {
        qDebug() << "Failed to open archive:" << filePath
                 << archive_error_string(archive);
        archive_read_free(archive);
        archive = nullptr;
        return false;
    }

    archivePath = filePath;
    return true;
}

void ArchiveHandler::closeArchive()
{
    if (archive) {
        archive_read_close(archive);
        archive_read_free(archive);
        archive = nullptr;
    }
    archivePath.clear();
}

QStringList ArchiveHandler::getImageFiles()
{
    QStringList imageFiles;

    if (!archive) return imageFiles;

    qDebug() << "=== 获取压缩包内图片文件列表 ===";

    // 保存当前读取位置
    int currentPosition = archive_read_header_position(archive);

    // 重置到开始位置
    archive_read_free(archive);
    archive = archive_read_new();
    archive_read_support_format_all(archive);
    archive_read_support_filter_all(archive);
    archive_read_open_filename(archive, archivePath.toLocal8Bit().constData(), 10240);

    struct archive_entry *entry;
    int entryCount = 0;
    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(entry);
        if (filename) {
            QString filePath = QString::fromUtf8(filename);
            entryCount++;

            qDebug() << "找到文件" << entryCount << ":" << filePath;

            if (isImageFile(filePath)) {
                imageFiles.append(filePath);
                qDebug() << "  ✅ 识别为图片文件";
            } else {
                qDebug() << "  ❌ 不是图片文件";
            }
        }
        archive_read_data_skip(archive);
    }

    qDebug() << "总共找到" << entryCount << "个文件，其中图片文件:" << imageFiles.size();
    qDebug() << "图片文件列表:" << imageFiles;

    // 重新打开以准备后续读取（保持原有逻辑）
    archive_read_free(archive);
    archive = archive_read_new();
    archive_read_support_format_all(archive);
    archive_read_support_filter_all(archive);
    archive_read_open_filename(archive, archivePath.toLocal8Bit().constData(), 10240);

    return imageFiles;
}

QByteArray ArchiveHandler::extractFile(const QString &filePath)
{
    QByteArray data;

    if (!archive) {
        qDebug() << "❌ ArchiveHandler: archive 为 null";
        return data;
    }

    qDebug() << "=== ArchiveHandler 提取文件 ===";
    qDebug() << "目标文件:" << filePath;
    qDebug() << "当前archive路径:" << archivePath;

    // 使用新的archive实例来避免影响当前状态
    struct archive *tempArchive = archive_read_new();
    archive_read_support_format_all(tempArchive);
    archive_read_support_filter_all(tempArchive);

    int r = archive_read_open_filename(tempArchive, archivePath.toLocal8Bit().constData(), 10240);
    if (r != ARCHIVE_OK) {
        qDebug() << "❌ 无法打开临时archive:" << archive_error_string(tempArchive);
        archive_read_free(tempArchive);
        return data;
    }

    struct archive_entry *entry;
    bool found = false;
    int scannedFiles = 0;

    while (archive_read_next_header(tempArchive, &entry) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(entry);
        scannedFiles++;

        if (filename) {
            QString currentFile = QString::fromUtf8(filename);
            qDebug() << "扫描文件" << scannedFiles << ":" << currentFile;

            if (currentFile == filePath) {
                qDebug() << "✅ 找到目标文件，开始提取数据";
                found = true;

                // 读取数据
                const void *buff;
                size_t size;
                la_int64_t offset;
                int dataBlocks = 0;
                int totalSize = 0;

                while (archive_read_data_block(tempArchive, &buff, &size, &offset) == ARCHIVE_OK) {
                    data.append(static_cast<const char *>(buff), size);
                    dataBlocks++;
                    totalSize += size;
                    qDebug() << "  数据块" << dataBlocks << "大小:" << size << "偏移:" << offset;
                }
                qDebug() << "提取完成: 数据块:" << dataBlocks << "总大小:" << totalSize;
                break;
            }
        }
        archive_read_data_skip(tempArchive);
    }

    if (!found) {
        qDebug() << "❌ 未找到文件:" << filePath << "扫描了" << scannedFiles << "个文件";

        // 输出前几个文件用于调试
        qDebug() << "前5个文件:";
        // 重新扫描输出前5个文件
        archive_read_free(tempArchive);
        tempArchive = archive_read_new();
        archive_read_support_format_all(tempArchive);
        archive_read_support_filter_all(tempArchive);
        archive_read_open_filename(tempArchive, archivePath.toLocal8Bit().constData(), 10240);

        for (int i = 0; i < 5 && archive_read_next_header(tempArchive, &entry) == ARCHIVE_OK; i++) {
            const char *filename = archive_entry_pathname(entry);
            if (filename) {
                qDebug() << "  " << i+1 << ":" << QString::fromUtf8(filename);
            }
            archive_read_data_skip(tempArchive);
        }
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
