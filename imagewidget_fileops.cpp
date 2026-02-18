// imagewidget_fileops.cpp
#include "imagewidget.h"
#include "qimagereader.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <platform_compat.h>

#ifdef _WIN32
#include <shellapi.h>
#else
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#endif


bool ImageWidget::moveFileToRecycleBin(const QString &filePath)
{
    return PlatformCompat::moveToRecycleBin(filePath);
}

bool ImageWidget::loadImage(const QString &filePath, bool fromCache)
{
    qDebug() << "=== loadImage 开始 ===";
    qDebug() << "文件路径:" << filePath;

    // 设置更高的内存分配限制（512MB）
    QImageReader::setAllocationLimit(512);
    qDebug() << "设置内存分配限制为 512MB";

    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qDebug() << "错误: 文件不存在";
        return false;
    }

    qDebug() << "文件大小:" << fileInfo.size() << "字节";

    // 检查是否是压缩包
    if (ArchiveHandler::isSupportedArchive(filePath)) {
        qDebug() << "检测为压缩包文件";
        return openArchive(filePath);
    }

    // 如果是压缩包模式，从压缩包加载
    if (isArchiveMode) {
        qDebug() << "压缩包模式，从压缩包加载";
        return loadImageFromArchive(filePath);
    }

    //QFileInfo fileInfo(filePath);
    qDebug() << "文件是否存在:" << fileInfo.exists();
    qDebug() << "文件大小:" << fileInfo.size();
    qDebug() << "文件权限:" << fileInfo.permissions();

    if (!fileInfo.exists()) {
        qDebug() << "错误: 文件不存在";
        return false;
    }

    // 直接加载，绕过缓存进行测试
    QPixmap loadedPixmap;
    qDebug() << "开始加载图片...";

    if (!loadedPixmap.load(filePath)) {
        qDebug() << "错误: 直接加载失败";

        // 尝试使用 QImage 加载
        QImage image;
        if (image.load(filePath)) {
            qDebug() << "使用 QImage 加载成功";
            loadedPixmap = QPixmap::fromImage(image);
        } else {
            qDebug() << "错误: QImage 加载也失败";
            return false;
        }
    } else {
        qDebug() << "直接加载成功，图片尺寸:" << loadedPixmap.size();
    }

    if (loadedPixmap.isNull()) {
        qDebug() << "错误: 加载后的 pixmap 为空";
        return false;
    }

    // 继续原有逻辑...
    originalPixmap = loadedPixmap;
    pixmap = loadedPixmap;
    qDebug() << "图片设置完成";

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

    currentImagePath = filePath;
    qDebug() << "当前图片路径设置为:" << currentImagePath;

    // 检查目录是否改变
    bool dirChanged = (currentDir != fileInfo.absoluteDir());
    if (dirChanged) {
        currentDir = fileInfo.absoluteDir();
        loadImageList();
    }

    // 确保当前图片索引正确设置
    currentImageIndex = imageList.indexOf(fileInfo.fileName());
    qDebug() << "当前图片索引:" << currentImageIndex;

    update();
    updateWindowTitle();
    qDebug() << "=== loadImage 完成 ===";

    return true;
}

void ImageWidget::loadImageList()
{
    QStringList newImageList;

    QFileInfoList fileList = currentDir.entryInfoList(QDir::Files);
    QStringList imageFilters = {"*.png",  "*.jpg", "*.bmp",  "*.jpeg",
                                "*.webp", "*.gif", "*.tiff", "*.tif"};
    QStringList archiveFilters = {"*.zip", "*.rar", "*.7z", "*.tar",
                                  "*.gz",  "*.bz2"}; // 添加压缩包过滤器

    foreach (const QFileInfo &fileInfo, fileList) {
        bool isImage = false;
        foreach (const QString &filter, imageFilters) {
            if (fileInfo.fileName().endsWith(filter.mid(1), Qt::CaseInsensitive)) {
                newImageList.append(fileInfo.fileName());
                isImage = true;
                break;
            }
        }

        // 如果不是图片，检查是否是压缩包
        if (!isImage) {
            foreach (const QString &filter, archiveFilters) {
                if (fileInfo.fileName().endsWith(filter.mid(1),
                                                 Qt::CaseInsensitive)) {
                    newImageList.append(fileInfo.fileName());
                    break;
                }
            }
        }
    }

    newImageList.sort();

    // 只有当文件列表实际发生变化时才更新和输出日志
    if (newImageList != imageList) {
        imageList = newImageList;
        thumbnailWidget->setImageList(imageList, currentDir);
        qDebug() << "找到文件:" << imageList.size() << "个（包含图片和压缩包）";
    }
}

bool ImageWidget::loadImageByIndex(int index, bool fromCache)
{
    if (imageList.isEmpty() || index < 0 || index >= imageList.size()) {
        return false;
    }

    bool result = false;

    if (isArchiveMode) {
        // 压缩包模式：使用内部文件名
        QString imagePath = imageList.at(index);
        result = loadImageFromArchive(imagePath);

        // 更新当前图片路径为压缩包路径 + 内部文件路径
        if (result) {
            currentImagePath = currentArchivePath + "|" + imagePath;
        }
    } else {
        // 普通文件模式：构建完整文件路径
        QString imagePath = currentDir.absoluteFilePath(imageList.at(index));
        result = loadImage(imagePath, fromCache);
    }

    // 更新当前索引
    if (result) {
        currentImageIndex = index;

        // 如果当前是缩略图模式，更新选中项
        if (currentViewMode == ThumbnailView) {
            thumbnailWidget->setSelectedIndex(currentImageIndex);
        }

        // 预加载下一张图片（用于幻灯片）
        if (isSlideshowActive) {
            int nextIndex = (currentImageIndex + 1) % imageList.size();

            if (isArchiveMode) {
                // 压缩包模式预加载
                QString nextPath = imageList.at(nextIndex);
                if (!archiveImageCache.contains(nextPath)) {
                    QtConcurrent::run([this, nextIndex]() {
                        QString nextPath = imageList.at(nextIndex);
                        QByteArray imageData = archiveHandler.extractFile(nextPath);
                        if (!imageData.isEmpty()) {
                            QPixmap tempPixmap;
                            if (tempPixmap.loadFromData(imageData)) {
                                QMutexLocker locker(&cacheMutex);
                                archiveImageCache.insert(nextPath, tempPixmap);
                                qDebug() << "预加载压缩包图片:" << nextPath;
                            }
                        }
                    });
                }
            } else {
                // 普通文件模式预加载
                QString nextPath =
                    currentDir.absoluteFilePath(imageList.at(nextIndex));
                if (!imageCache.contains(nextPath)) {
                    QtConcurrent::run([this, nextIndex]() {
                        QString nextPath =
                            currentDir.absoluteFilePath(imageList.at(nextIndex));
                        QPixmap tempPixmap;
                        if (tempPixmap.load(nextPath)) {
                            QMutexLocker locker(&cacheMutex);
                            imageCache.insert(nextPath, tempPixmap);
                        }
                    });
                }
            }
        }
    }

    return result;
}

void ImageWidget::loadNextImage()
{
    qDebug() << "=== loadNextImage 开始 ===";
    qDebug() << "当前模式:" << (currentViewMode == SingleView ? "单张" : "缩略图");
    qDebug() << "当前索引:" << currentImageIndex << "，图片总数:" << imageList.size();

    if (imageList.isEmpty()) {
        qDebug() << "图片列表为空，返回";
        return;
    }

    int nextIndex = (currentImageIndex + 1) % imageList.size();
    qDebug() << "计算出的下一个索引:" << nextIndex;

    if (currentViewMode == SingleView) {
        qDebug() << "单张模式，加载图片";
        loadImageByIndex(nextIndex, true);
    } else {
        // 缩略图模式下，只更新索引和选中状态
        qDebug() << "缩略图模式，更新选中状态";
        currentImageIndex = nextIndex;
        thumbnailWidget->setSelectedIndex(currentImageIndex);
        thumbnailWidget->ensureVisible(currentImageIndex);
        updateWindowTitle();

        qDebug() << "更新后的当前索引:" << currentImageIndex;
    }

    qDebug() << "=== loadNextImage 结束 ===";
}

void ImageWidget::loadPreviousImage()
{
    qDebug() << "=== loadPreviousImage 开始 ===";
    qDebug() << "当前模式:" << (currentViewMode == SingleView ? "单张" : "缩略图");
    qDebug() << "当前索引:" << currentImageIndex << "，图片总数:" << imageList.size();

    if (imageList.isEmpty()) {
        qDebug() << "图片列表为空，返回";
        return;
    }

    int prevIndex = (currentImageIndex - 1 + imageList.size()) % imageList.size();
    qDebug() << "计算出的上一个索引:" << prevIndex;

    if (currentViewMode == SingleView) {
        qDebug() << "单张模式，加载图片";
        loadImageByIndex(prevIndex, true);
    } else {
        // 缩略图模式下，只更新索引和选中状态
        qDebug() << "缩略图模式，更新选中状态";
        currentImageIndex = prevIndex;
        thumbnailWidget->setSelectedIndex(currentImageIndex);
        thumbnailWidget->ensureVisible(currentImageIndex);
        updateWindowTitle();

        qDebug() << "更新后的当前索引:" << currentImageIndex;
    }

    qDebug() << "=== loadPreviousImage 结束 ===";
}

void ImageWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void ImageWidget::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (urlList.isEmpty()) {
            return;
        }

        // 只处理第一个拖拽项
        QString filePath = urlList.first().toLocalFile();

        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            return;
        }

        if (fileInfo.isDir()) {
            // 更新最后打开路径（使用图片所在文件夹）
            currentConfig.lastOpenPath = fileInfo.absolutePath();
            saveConfiguration();

            // 处理文件夹拖拽 - 切换到缩略图模式
            currentDir = QDir(filePath);
            loadImageList();

            // 无论当前是什么模式，都切换到缩略图模式
            currentViewMode = ThumbnailView;
            scrollArea->show();
            currentImageIndex = -1;
            update();
        } else if (fileInfo.isFile()) {
            // 处理文件拖拽
            if (loadImage(filePath)) {
                // 更新最后打开路径（使用图片所在文件夹）
                currentConfig.lastOpenPath = fileInfo.absolutePath();
                saveConfiguration();

                // 如果是单张模式，保持单张模式；如果是缩略图模式，切换到单张模式
                if (currentViewMode == ThumbnailView) {
                    switchToSingleView();
                } else {
                    update();
                }
                updateWindowTitle();
            }
        }

        event->acceptProposedAction();
    }
}

void ImageWidget::deleteCurrentImage()
{
    if (currentImagePath.isEmpty() || !QFile::exists(currentImagePath)) {
        QMessageBox::warning(this, tr("警告"), tr("没有可删除的图片"));
        return;
    }

    // 确认对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("确认删除"),
                                  tr("确定要将图片 '") +
                                      QFileInfo(currentImagePath).fileName() +
                                      tr("' 移动到回收站吗？"),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString imageToDelete = currentImagePath;
    int indexToDelete = currentImageIndex;

    // 移动到回收站而不是直接删除
    if (moveFileToRecycleBin(imageToDelete)) {
        // 从缓存中移除
        imageCache.remove(imageToDelete);

        // 从缩略图缓存中移除
        ThumbnailWidget::clearThumbnailCacheForImage(imageToDelete);

        // 从图片列表中移除
        if (indexToDelete >= 0 && indexToDelete < imageList.size()) {
            imageList.removeAt(indexToDelete);

            // 更新缩略图
            thumbnailWidget->setImageList(imageList, currentDir);

            // 确定下一张要显示的图片
            if (imageList.isEmpty()) {
                // 如果没有图片了
                pixmap = QPixmap();
                currentImagePath.clear();
                currentImageIndex = -1;
                if (currentViewMode == SingleView) {
                    switchToThumbnailView();
                }
            } else {
                // 计算新的索引
                int newIndex = indexToDelete;
                if (newIndex >= imageList.size()) {
                    newIndex = imageList.size() - 1;
                }

                if (currentViewMode == SingleView) {
                    // 单张视图模式下加载新图片
                    loadImageByIndex(newIndex);
                    // 强制刷新掩码和 X11 形状
                    updateMask();
                } else {
                    // 缩略图模式下更新选中项
                    currentImageIndex = newIndex;
                    thumbnailWidget->setSelectedIndex(newIndex);
                }
            }

            update();
            updateWindowTitle();
        }
    } else {
        QMessageBox::critical(this, tr("错误"), tr("移动图片到回收站失败"));
    }
}

void ImageWidget::deleteSelectedThumbnail()
{
    if (currentViewMode == ThumbnailView) {
        int selectedIndex = thumbnailWidget->getSelectedIndex();
        if (selectedIndex >= 0 && selectedIndex < imageList.size()) {
            // 临时切换到要删除的图片，然后调用删除函数
            QString imagePath =
                currentDir.absoluteFilePath(imageList.at(selectedIndex));
            loadImage(imagePath);
            currentImageIndex = selectedIndex;
            deleteCurrentImage();
        } else {
            QMessageBox::warning(this, tr("警告"), tr("请先选择要删除的图片"));
        }
    }
}



void ImageWidget::permanentlyDeleteCurrentImage()
{
    if (currentImagePath.isEmpty() || !QFile::exists(currentImagePath)) {
        QMessageBox::warning(this, tr("警告"), tr("没有可删除的图片"));
        return;
    }

    // 警告对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::warning(
        this, tr("永久删除警告"),
        tr("确定要永久删除图片 '") + QFileInfo(currentImagePath).fileName() +
            "' 吗？\n"
            "此操作无法撤销，文件将无法恢复！",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString imageToDelete = currentImagePath;
    int indexToDelete = currentImageIndex;

    // 直接删除文件
    if (QFile::remove(imageToDelete)) {
        // 其余代码与 deleteCurrentImage 相同
        imageCache.remove(imageToDelete);
        ThumbnailWidget::clearThumbnailCacheForImage(imageToDelete);

        if (indexToDelete >= 0 && indexToDelete < imageList.size()) {
            imageList.removeAt(indexToDelete);
            thumbnailWidget->setImageList(imageList, currentDir);

            if (imageList.isEmpty()) {
                pixmap = QPixmap();
                currentImagePath.clear();
                currentImageIndex = -1;
                if (currentViewMode == SingleView) {
                    switchToThumbnailView();
                }
            } else {
                int newIndex = indexToDelete;
                if (newIndex >= imageList.size()) {
                    newIndex = imageList.size() - 1;
                }

                if (currentViewMode == SingleView) {
                    loadImageByIndex(newIndex);
                } else {
                    currentImageIndex = newIndex;
                    thumbnailWidget->setSelectedIndex(newIndex);
                }
            }

            update();
            updateWindowTitle();
        }
    } else {
        QMessageBox::critical(this, tr("错误"), tr("删除图片失败，可能没有权限或文件被占用"));
    }
}

void ImageWidget::permanentlyDeleteSelectedThumbnail()
{
    if (currentViewMode == ThumbnailView) {
        int selectedIndex = thumbnailWidget->getSelectedIndex();
        if (selectedIndex >= 0 && selectedIndex < imageList.size()) {
            QString imagePath =
                currentDir.absoluteFilePath(imageList.at(selectedIndex));
            loadImage(imagePath);
            currentImageIndex = selectedIndex;
            permanentlyDeleteCurrentImage();
        } else {
            QMessageBox::warning(this, tr("警告"), tr("请先选择要删除的图片"));
        }
    }
}
