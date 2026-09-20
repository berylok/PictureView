
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
        //qDebug() << "无法打开压缩包:" << filePath;
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
    //qDebug() << "成功打开压缩包，包含" << imageList.size() << "个文件";
    return true;
}

void ImageWidget::exitArchiveMode()
{
    if (!isArchiveMode) return;

    //qDebug() << "退出压缩包模式";

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
    //qDebug() << "已返回到目录:" << currentDir.absolutePath();
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

    //qDebug() << "=== 加载压缩包图片列表 ===";

    QStringList archiveImageList = archiveHandler.getImageFiles();
    archiveImageList.sort();

    //qDebug() << "排序后的图片列表:";
    for (int i = 0; i < archiveImageList.size(); ++i) {
        //qDebug() << "  " << i << ":" << archiveImageList[i];
    }

    // 保存原始文件名列表
    imageList = archiveImageList;

    // 构建用于缩略图显示的完整路径列表
    QStringList thumbnailPaths;
    for (const QString &fileName : std::as_const(archiveImageList)) {
        QString fullPath = currentArchivePath + "|" + fileName;
        thumbnailPaths.append(fullPath);
        //qDebug() << "构建缩略图路径:" << fullPath;
    }

    // 传递给缩略图部件
    thumbnailWidget->setImageList(thumbnailPaths, QDir());

    //qDebug() << "从压缩包中找到图片文件:" << imageList.size() << "个";
    //qDebug() << "传递给缩略图部件的路径数量:" << thumbnailPaths.size();
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

// ---------------------------------------------------------------------
// 说明：本函数会被 ThumbnailWidget 在工作线程调用，
//       全程使用 QImage，禁止出现任何 QPixmap。
//       如果你头文件里的名字是 getArchiveThumbnailImage，
//       把下面的函数名改成 getArchiveThumbnailImage 即可。
// ---------------------------------------------------------------------
QImage ImageWidget::getArchiveThumbnailImage(const QString &archivePath)
{
    // 顶层压缩包（无 "|"）→ 默认图标
    if (!archivePath.contains('|')) {
        static QImage defaultArchiveIcon;
        if (defaultArchiveIcon.isNull()) {
            defaultArchiveIcon = createDefaultArchiveThumbnail();
        }
        return defaultArchiveIcon;
    }

    // 缓存命中
    {
        QMutexLocker locker(&cacheMutex);
        auto it = archiveImageCache.constFind(archivePath);
        if (it != archiveImageCache.constEnd()) {
            return it.value();
        }
    }

    QStringList parts = archivePath.split("|");
    if (parts.size() != 2) {
        return createDefaultArchiveThumbnail();
    }

    QString archiveFile  = parts[0];
    QString internalFile = parts[1];

    if (!QFile::exists(archiveFile)) {
        return createDefaultArchiveThumbnail();
    }

    QByteArray imageData = archiveHandler.extractFile(internalFile);

    // ---------- 数据为空：错误占位图 ----------
    if (imageData.isEmpty()) {
        QImage errorImage(thumbnailSize, QImage::Format_ARGB32_Premultiplied);
        errorImage.fill(Qt::red);
        {
            QPainter painter(&errorImage);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 8, QFont::Bold));
            painter.drawText(errorImage.rect(), Qt::AlignCenter,
                             "提取失败\n数据为空");
        }
        {
            QMutexLocker locker(&cacheMutex);
            archiveImageCache.insert(archivePath, errorImage);
        }
        return errorImage;
    }

    // ---------- 正常解码 ----------
    QImage image;
    QImageReader reader;
    QBuffer buffer;
    buffer.setData(imageData);
    buffer.open(QIODevice::ReadOnly);
    reader.setDevice(&buffer);
    reader.setAutoTransform(true);

    int maxDecode = currentConfig.maxDecodeSize;     //
    QSize origSize = reader.size();
    if (origSize.isValid() &&
        (origSize.width() > maxDecode || origSize.height() > maxDecode)) {
        reader.setScaledSize(origSize.scaled(maxDecode, maxDecode, Qt::KeepAspectRatio));
    }
    reader.read(&image);

    // ---------- 解码失败：错误占位图 ----------
    QImage failedImage(thumbnailSize, QImage::Format_ARGB32_Premultiplied);
    failedImage.fill(QColor(255, 100, 100));
    {
        QPainter painter(&failedImage);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 8, QFont::Bold));
        painter.drawText(failedImage.rect(), Qt::AlignCenter,
                         QString("加载失败\n%1\n%2字节")
                             .arg(internalFile)
                             .arg(imageData.size()));
    }
    {
        QMutexLocker locker(&cacheMutex);
        archiveImageCache.insert(archivePath, failedImage);
    }
    return failedImage;
}
// 创建默认的压缩包缩略图
QImage ImageWidget::createDefaultArchiveThumbnail()
{
    QImage thumbnail(thumbnailSize, QImage::Format_ARGB32_Premultiplied);
    thumbnail.fill(QColor(200, 200, 200));

    QPainter painter(&thumbnail);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::darkGray, 2));
    painter.setBrush(QColor(100, 150, 255, 100));

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
