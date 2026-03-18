// imagewidget_archive.cpp
#include "imagewidget.h"
#include <QMessageBox>

#include <QPainter>
#include <QPainterPath>
#include <QFont>

#include <QImageReader>
#include <QBuffer>

bool ImageWidget::openArchive(const QString &filePath)
{
    if (!archiveHandler.openArchive(filePath)) {
        qDebug() << "无法打开压缩包:" << filePath;
        return false;
    }

    // 保存当前状态（压缩包外的状态）
    previousDir = currentDir;
    previousImageList = imageList;
    previousImageIndex = currentImageIndex;
    previousViewMode = currentViewMode;

    isArchiveMode = true;
    currentArchivePath = filePath;
    archiveImageCache.clear(); // 清空缓存

    // 加载压缩包中的图片列表
    loadArchiveImageList();

    // 切换到缩略图模式
    switchToThumbnailView();

    updateWindowTitle();
    qDebug() << "成功打开压缩包，包含" << imageList.size() << "个文件";
    return true;
}

void ImageWidget::exitArchiveMode()
{
    if (!isArchiveMode) return;

    qDebug() << "退出压缩包模式";

    // 关闭压缩包
    closeArchive();

    // 恢复之前的状态
    currentDir = previousDir;
    imageList = previousImageList;
    currentImageIndex = previousImageIndex;

    // 重新加载图片列表
    thumbnailWidget->setImageList(imageList, currentDir);

    // 恢复之前的视图模式
    if (previousViewMode == ThumbnailView) {
        switchToThumbnailView();
    } else {
        // 如果之前是单张模式，尝试加载对应的图片
        if (currentImageIndex >= 0 && currentImageIndex < imageList.size()) {
            loadImageByIndex(currentImageIndex);
        }
        switchToSingleView();
    }

    updateWindowTitle();
    qDebug() << "已返回到目录:" << currentDir.absolutePath();
}

void ImageWidget::closeArchive()
{
    if (isArchiveMode) {
        archiveHandler.closeArchive();
        isArchiveMode = false;
        currentArchivePath.clear();
        imageList.clear();
    }
}

void ImageWidget::loadArchiveImageList()
{
    if (!isArchiveMode) return;

    qDebug() << "=== 加载压缩包图片列表 ===";

    QStringList archiveImageList = archiveHandler.getImageFiles();
    archiveImageList.sort();

    qDebug() << "排序后的图片列表:";
    for (int i = 0; i < archiveImageList.size(); ++i) {
        qDebug() << "  " << i << ":" << archiveImageList[i];
    }

    // 保存原始文件名列表
    imageList = archiveImageList;

    // 构建用于缩略图显示的完整路径列表
    QStringList thumbnailPaths;
    for (const QString &fileName : std::as_const(archiveImageList)) {
        QString fullPath = currentArchivePath + "|" + fileName;
        thumbnailPaths.append(fullPath);
        qDebug() << "构建缩略图路径:" << fullPath;
    }

    // 传递给缩略图部件
    thumbnailWidget->setImageList(thumbnailPaths, QDir());

    qDebug() << "从压缩包中找到图片文件:" << imageList.size() << "个";
    qDebug() << "传递给缩略图部件的路径数量:" << thumbnailPaths.size();
}

bool ImageWidget::loadImageFromArchive(const QString &filePath)
{
    if (!isArchiveMode) return false;

    QByteArray imageData = archiveHandler.extractFile(filePath);
    if (imageData.isEmpty()) {
        return false;
    }

    QPixmap loadedPixmap;
    if (!loadedPixmap.loadFromData(imageData)) {
        return false;
    }

    // 保存原始图片并重置变换状态
    originalPixmap = loadedPixmap;
    pixmap = loadedPixmap;

    // 重置变换状态
    rotationAngle = 0;
    isHorizontallyFlipped = false;
    isVerticallyFlipped = false;

    // 设置视图状态
    switch (currentViewStateType) {
    case FitToWindow:
        fitToWindow();
        break;
    case ActualSize:
        actualSize();
        break;
    case ManualAdjustment:
        // 保持当前的缩放和偏移
        break;
    }

    currentImagePath = currentArchivePath + "|" + filePath;

    // 确保当前图片索引正确设置
    currentImageIndex = imageList.indexOf(filePath);

    update();
    updateWindowTitle();

    return true;
}

QPixmap ImageWidget::getArchiveThumbnail(const QString &archivePath)
{
    qDebug() << "=== getArchiveThumbnail 详细调试 ===";
    qDebug() << "输入路径:" << archivePath;

    // 使用完整路径作为缓存键
    if (archiveImageCache.contains(archivePath)) {
        qDebug() << "从缓存获取缩略图:" << archivePath;
        QPixmap cached = archiveImageCache.value(archivePath);
        qDebug() << "缓存图片是否为空:" << cached.isNull() << "尺寸:" << cached.size();
        return cached;
    }

    // 解析路径格式：压缩包路径|内部文件路径
    if (!archivePath.contains("|")) {
        qDebug() << "路径格式错误，不包含 | 分隔符";
        return createDefaultArchiveThumbnail();
    }

    QStringList parts = archivePath.split("|");
    if (parts.size() != 2) {
        qDebug() << "路径格式错误，分割后不是2部分";
        return createDefaultArchiveThumbnail();
    }

    QString archiveFile = parts[0];
    QString internalFile = parts[1];

    qDebug() << "解析结果:";
    qDebug() << "  - 压缩包:" << archiveFile;
    qDebug() << "  - 内部文件:" << internalFile;

    // 检查文件是否存在
    if (!QFile::exists(archiveFile)) {
        qDebug() << "压缩包文件不存在:" << archiveFile;
        return createDefaultArchiveThumbnail();
    }

    qDebug() << "从压缩包提取文件:" << internalFile;

    // 使用 ArchiveHandler 提取文件
    QByteArray imageData = archiveHandler.extractFile(internalFile);

    qDebug() << "提取结果:";
    qDebug() << "  - 数据大小:" << imageData.size();

    if (imageData.isEmpty()) {
        qDebug() << "!!! 提取的数据为空 !!!";

        // 创建错误提示图片
        QImage errorImage(thumbnailSize, QImage::Format_RGB32);
        errorImage.fill(Qt::red);

        QPainter painter(&errorImage);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 8, QFont::Bold));
        painter.drawText(errorImage.rect(), Qt::AlignCenter, "提取失败\n数据为空");
        painter.end();

        QPixmap errorThumb = QPixmap::fromImage(errorImage);

        QMutexLocker locker(&cacheMutex);
        archiveImageCache.insert(archivePath, errorThumb);
        return errorThumb;
    }

    // 检查数据前几个字节（图片文件签名）
    QByteArray header = imageData.left(8);
    qDebug() << "  - 数据前8字节(HEX):" << header.toHex();

    // 方法1: 使用 QImage 加载
    QImage image;
    if (image.loadFromData(imageData)) {
        qDebug() << "✅ QImage加载成功:";
        qDebug() << "  - 原始尺寸:" << image.size();
        qDebug() << "  - 格式:" << image.format();
        qDebug() << "  - 深度:" << image.depth();

        // 缩放到缩略图大小
        QImage scaledImage = image.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap thumbnail = QPixmap::fromImage(scaledImage);

        qDebug() << "  - 缩略图尺寸:" << thumbnail.size();

        // 缓存并返回
        QMutexLocker locker(&cacheMutex);
        archiveImageCache.insert(archivePath, thumbnail);
        return thumbnail;
    } else {
        qDebug() << "❌ QImage加载失败";
    }

    // 方法2: 使用 QPixmap 作为备选
    QPixmap pixmap;
    if (pixmap.loadFromData(imageData)) {
        qDebug() << "✅ QPixmap加载成功:";
        qDebug() << "  - 原始尺寸:" << pixmap.size();

        QPixmap thumbnail = pixmap.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        qDebug() << "  - 缩略图尺寸:" << thumbnail.size();

        QMutexLocker locker(&cacheMutex);
        archiveImageCache.insert(archivePath, thumbnail);
        return thumbnail;
    } else {
        qDebug() << "❌ QPixmap加载也失败";
    }

    qDebug() << "❌ 所有图片加载方法都失败";

    // 创建加载失败提示图片（不是压缩包图标）
    QImage failedImage(thumbnailSize, QImage::Format_RGB32);
    failedImage.fill(QColor(255, 100, 100));

    QPainter painter(&failedImage);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 8, QFont::Bold));
    painter.drawText(failedImage.rect(), Qt::AlignCenter,
                     QString("加载失败\n%1\n%2字节")
                         .arg(internalFile)
                         .arg(imageData.size()));
    painter.end();

    QPixmap failedThumb = QPixmap::fromImage(failedImage);

    QMutexLocker locker(&cacheMutex);
    archiveImageCache.insert(archivePath, failedThumb);
    return failedThumb;
}

// 创建默认的压缩包缩略图
QPixmap ImageWidget::createDefaultArchiveThumbnail()
{
    QPixmap thumbnail(thumbnailSize);
    thumbnail.fill(QColor(200, 200, 200)); // 灰色背景

    QPainter painter(&thumbnail);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制文件夹图标
    painter.setPen(QPen(Qt::darkGray, 2));
    painter.setBrush(QColor(100, 150, 255, 100));

    // 绘制简单的文件夹形状
    QPainterPath folderPath;
    folderPath.moveTo(20, 40);
    folderPath.lineTo(30, 20);
    folderPath.lineTo(thumbnailSize.width() - 20, 20);
    folderPath.lineTo(thumbnailSize.width() - 10, 40);
    folderPath.lineTo(20, 40);
    folderPath.lineTo(10, thumbnailSize.height() - 20);
    folderPath.lineTo(thumbnailSize.width() - 10, thumbnailSize.height() - 20);
    folderPath.lineTo(thumbnailSize.width() - 20, 40);

    painter.drawPath(folderPath);

    // 绘制文字
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 8));
    painter.drawText(thumbnail.rect(), Qt::AlignCenter, "ZIP");

    return thumbnail;
}

bool ImageWidget::isArchiveFile(const QString &fileName) const
{
    QString lowerName = fileName.toLower();
    return (lowerName.endsWith(".zip") || lowerName.endsWith(".rar") ||
            lowerName.endsWith(".7z") || lowerName.endsWith(".tar") ||
            lowerName.endsWith(".gz") || lowerName.endsWith(".bz2"));
}
