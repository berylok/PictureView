
#include "imagewidget.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>
#include <QFileInfo>
#include <QApplication>
#include <QClipboard>

void ImageWidget::openFolder()
{
    QString initialPath = currentConfig.lastOpenPath;
    if (initialPath.isEmpty() || !QDir(initialPath).exists()) {
        initialPath =
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    QString folderPath = QFileDialog::getExistingDirectory(
        nullptr, tr("选择图片文件夹"), initialPath);

    if (!folderPath.isEmpty()) {
        // 更新最后打开路径
        currentConfig.lastOpenPath = folderPath;
        saveConfiguration();

        // 重置为合适大小模式
        currentViewStateType = FitToWindow;

        currentDir = QDir(folderPath);

        // ★ 切目录前清缓存（避免新目录第一次翻图读盘）
        {
            QMutexLocker locker(&cacheMutex);
            imageCache.clear();
            archiveImageCache.clear();
        }
        pixmapCache.clear();

        loadImageList();

        // 确保切换到缩略图模式
        switchToThumbnailView();

        if (!imageList.isEmpty()) {
            currentImageIndex = 0;
            thumbnailWidget->setSelectedIndex(0);
        } else {
            currentImageIndex = -1;
        }

        // 确保窗口正常显示
        ensureWindowVisible();

        update();
    }
}

void ImageWidget::openImage() {
    QString initialPath = currentConfig.lastOpenPath;
    if (initialPath.isEmpty() || !QDir(initialPath).exists()) {
        initialPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    // 更清晰的文件过滤器
    QString filter =
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.tiff *.tif);;"
        "PNG图片 (*.png);;"
        "JPEG图片 (*.jpg *.jpeg);;"
        "BMP图片 (*.bmp);;"
        "WebP图片 (*.webp);;"
        "GIF图片 (*.gif);;"
        "TIFF图片 (*.tiff *.tif);;"
        "压缩包 (*.zip *.rar *.7z *.tar *.gz *.bz2);;"
        "所有文件 (*.*)";

    QString fileName = QFileDialog::getOpenFileName(
        nullptr, tr("打开图片或压缩包"), initialPath, filter);

    if (fileName.isEmpty()) return;

    QFileInfo fileInfo(fileName);
    currentConfig.lastOpenPath = fileInfo.absolutePath();
    saveConfiguration();

    if (ArchiveHandler::isSupportedArchive(fileName)) {
        // ============ 压缩包：切到所在目录的缩略图模式，并高亮它 ============
        QDir newDir = fileInfo.absoluteDir();
        bool dirChanged = (newDir != currentDir);

        if (dirChanged) {
            // ★ 切目录前清缓存
            {
                QMutexLocker locker(&cacheMutex);
                imageCache.clear();
                archiveImageCache.clear();
            }
            pixmapCache.clear();
        }

        currentDir = newDir;
        loadImageList();

        int idx = imageList.indexOf(fileInfo.fileName());
        if (idx >= 0) {
            currentImageIndex = idx;
        } else {
            currentImageIndex = -1;
        }

        switchToThumbnailView();   // 内部会处理选中和滚动
    } else {
        // ============ 普通图片：直接进单图模式 ============
        // loadImage 内部会检测 dirChanged 并调用 loadImageList
        loadImage(fileName);
        switchToSingleView();
    }
}

void ImageWidget::saveImage()
{
    if (pixmap.isNull()) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this, tr("保存图片"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "Images (*.png *.jpg *.bmp *.jpeg *.webp)");
    if (!fileName.isEmpty()) {
        if (pixmap.save(fileName)) {
            // 保存成功
        } else {
            // 保存失败
        }
    }
}

void ImageWidget::copyImageToClipboard()
{
    if (!pixmap.isNull()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setPixmap(pixmap);
    }
}

void ImageWidget::pasteImageFromClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    if (!mimeData || !mimeData->hasImage()) return;

    QImage image = clipboard->image();
    if (image.isNull()) return;

    // 切到单图模式
    currentViewMode = SingleView;
    scrollArea->hide();

    // 重置状态
    pixmap = QPixmap::fromImage(image);
    originalPixmap = pixmap;
    scaleFactor = 1.0;
    panOffset = QPointF(0, 0);
    currentImagePath.clear();
    currentImageIndex = -1;
    imageList.clear();

    // 重置变换
    rotationAngle = 0;
    isHorizontallyFlipped = false;
    isVerticallyFlipped = false;

    // 清缩略图列表
    if (thumbnailWidget) {
        thumbnailWidget->setImageList({}, QDir());
    }

    update();
    updateWindowTitle();
}

void ImageWidget::openImageInNewWindow()
{
    // 获取当前有效的图片路径（复用菜单中的逻辑）
    auto getCurrentImagePath = [this]() -> QString {
        if (!currentImagePath.isEmpty() && QFile::exists(currentImagePath) && !isArchiveMode) {
            return currentImagePath;
        }
        if (currentViewMode == ThumbnailView) {
            int selected = thumbnailWidget->getSelectedIndex();
            if (selected >= 0 && selected < imageList.size() && !isArchiveMode) {
                return currentDir.absoluteFilePath(imageList.at(selected));
            }
        }
        return QString();
    };

    QString path = getCurrentImagePath();
    if (path.isEmpty() || !QFile::exists(path)) {
        QMessageBox::warning(this, tr("警告"), tr("没有可用的图片文件"));
        return;
    }

    QString program = QCoreApplication::applicationFilePath();
    QStringList arguments;
    arguments << path;
    QString workingDir = QCoreApplication::applicationDirPath();

    qint64 pid = 0;
    bool success = QProcess::startDetached(program, arguments, workingDir, &pid);
    if (!success) {
#ifdef Q_OS_LINUX
        QString command = QString("\"%1\" \"%2\"").arg(program, path);
        success = QProcess::startDetached("/bin/sh", QStringList() << "-c" << command, workingDir, &pid);
#endif
    }

    if (!success) {
        QMessageBox::critical(this, tr("启动失败"),
                              tr("无法启动新窗口。\n请检查程序路径：\n%1").arg(program));
    } else {
        //qDebug() << "新窗口已启动，PID:" << pid;
    }
}
