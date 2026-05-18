
// imagewidget_fileops.cpp
#include "imagewidget.h"
#include "qimagereader.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include "platform_compat.h"
#include <QCheckBox>
#include <QDebug>
#include <QtConcurrent>

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

// ==================== 优化的图片加载函数 ====================
bool ImageWidget::loadImage(const QString &filePath, bool fromCache)
{
    qDebug() << "=== loadImage 开始 ===";
    qDebug() << "文件路径:" << filePath;

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

    // ========== 新增：使用 QImageReader 进行降采样优化 ==========
    QImage loadedImage;

    // 获取目标显示尺寸（当前窗口大小）
    QSize targetSize = size();
    if (targetSize.isEmpty() || targetSize.width() <= 1) {
        targetSize = QSize(1920, 1080); // 默认目标尺寸
    }

    qDebug() << "目标显示尺寸:" << targetSize;

    // 使用 QImageReader 进行智能加载
    QImageReader reader(filePath);
    reader.setAutoTransform(true);

    // 获取原始图片尺寸（不加载数据）
    QSize originalSize = reader.size();
    if (originalSize.isValid()) {
        qDebug() << "原始图片尺寸:" << originalSize;

        // 计算降采样比例（对于 4K/8K 图片尤其重要）
        int widthScale = originalSize.width() / targetSize.width();
        int heightScale = originalSize.height() / targetSize.height();
        int scale = qMax(1, qMin(widthScale, heightScale));

        // 限制最大缩放倍数，避免过度降采样
        if (scale > 8) scale = 8;

        if (scale > 1) {
            QSize scaledSize = originalSize / scale;
            reader.setScaledSize(scaledSize);
            qDebug() << "降采样比例:" << scale << "解码尺寸:" << scaledSize;
        }

        // 可选：限制内存使用（Qt 6.0+）
        // reader.setAllocationLimit(200 * 1024 * 1024); // 200MB 上限
    }

    // 加载图片
    qDebug() << "开始加载图片...";
    loadedImage = reader.read();

    if (loadedImage.isNull()) {
        qDebug() << "QImageReader 加载失败，尝试 QImage 直接加载...";

        // 回退到传统加载方式
        if (!loadedImage.load(filePath)) {
            qDebug() << "错误: 所有加载方式都失败";
            return false;
        }
        qDebug() << "QImage 直接加载成功，尺寸:" << loadedImage.size();
    } else {
        qDebug() << "QImageReader 加载成功，尺寸:" << loadedImage.size();
    }

    // 转换为 QPixmap 用于显示
    QPixmap loadedPixmap = QPixmap::fromImage(loadedImage);
    if (loadedPixmap.isNull()) {
        qDebug() << "错误: 转换为 QPixmap 失败";
        return false;
    }

    // ========== OpenGL 加速：更新纹理 ==========
#ifdef HAS_QT6_OPENGL
    if (m_useOpenGL && m_glWidget) {
        // 保存为 QImage 用于 OpenGL 纹理
        currentImage = loadedImage;
        updateGLTexture();
        qDebug() << "OpenGL 纹理已更新";
    } else {
        currentImage = loadedImage;
    }
#else
    currentImage = loadedImage;
#endif

    // 继续原有逻辑...
    if (!transformLocked) {
        rotationAngle = 0;
        isHorizontallyFlipped = false;
        isVerticallyFlipped = false;
    }

    // 保存原始图片
    originalPixmap = loadedPixmap;

    // 如果锁定状态，需要重新应用变换
    if (transformLocked) {
        applyTransformations();
    } else {
        pixmap = loadedPixmap;
    }
    qDebug() << "图片设置完成";

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

// ==================== 异步加载函数（避免 UI 卡顿） ====================
void ImageWidget::loadImageAsync(const QString &filePath, std::function<void(bool)> callback)
{
    QtConcurrent::run([this, filePath, callback]() {
        bool result = loadImage(filePath, false);
        if (callback) {
            // 使用 QMetaObject::invokeMethod 确保在主线程执行回调
            QMetaObject::invokeMethod(this, [callback, result]() {
                callback(result);
            }, Qt::QueuedConnection);
        }
    });
}

// ==================== 批量预加载（优化幻灯片体验） ====================
void ImageWidget::preloadImagesAround(int centerIndex, int count)
{
    if (imageList.isEmpty()) return;

    int startIndex = qMax(0, centerIndex - count);
    int endIndex = qMin(imageList.size() - 1, centerIndex + count);

    for (int i = startIndex; i <= endIndex; ++i) {
        if (i == centerIndex) continue; // 跳过当前图片

        QString imagePath = currentDir.absoluteFilePath(imageList.at(i));

        // 检查缓存
        if (!imageCache.contains(imagePath)) {
            QtConcurrent::run([this, imagePath]() {
                // 使用 QImageReader 进行降采样预加载
                QImageReader reader(imagePath);
                reader.setAutoTransform(true);

                QSize originalSize = reader.size();
                if (originalSize.isValid()) {
                    // 预加载时使用较小的尺寸
                    QSize previewSize(800, 600);
                    int widthScale = originalSize.width() / previewSize.width();
                    int heightScale = originalSize.height() / previewSize.height();
                    int scale = qMax(1, qMin(widthScale, heightScale));

                    if (scale > 1) {
                        reader.setScaledSize(originalSize / scale);
                    }
                }

                QImage preview = reader.read();
                if (!preview.isNull()) {
                    QMutexLocker locker(&cacheMutex);
                    imageCache.insert(imagePath, QPixmap::fromImage(preview));
                    qDebug() << "预加载完成:" << imagePath;
                }
            });
        }
    }
}

// ==================== 优化的缩略图生成 ====================
QPixmap ImageWidget::getArchiveThumbnail(const QString &archivePath)
{
    // 检查缓存
    if (archiveImageCache.contains(archivePath)) {
        return archiveImageCache[archivePath];
    }

    // 使用 QImageReader 生成缩略图
    QImageReader reader(archivePath);
    reader.setAutoTransform(true);

    // 设置缩略图目标尺寸
    QSize targetSize(256, 256);
    QSize originalSize = reader.size();

    if (originalSize.isValid()) {
        int widthScale = originalSize.width() / targetSize.width();
        int heightScale = originalSize.height() / targetSize.height();
        int scale = qMax(1, qMin(widthScale, heightScale));

        if (scale > 1) {
            reader.setScaledSize(originalSize / scale);
        }
    }

    QImage thumbnail = reader.read();
    if (!thumbnail.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(thumbnail);
        archiveImageCache.insert(archivePath, pixmap);
        return pixmap;
    }

    return createDefaultArchiveThumbnail();
}

// ==================== 清晰图片缓存（可选） ====================
void ImageWidget::clearImageCache()
{
    QMutexLocker locker(&cacheMutex);
    imageCache.clear();

#ifdef HAS_QT6_OPENGL
    // 清理 OpenGL 纹理
    if (m_glTexture) {
        delete m_glTexture;
        m_glTexture = nullptr;
    }
#endif

    qDebug() << "图片缓存已清除";
}

// ==================== 其余现有函数保持不变 ====================

void ImageWidget::loadImageList()
{
    QStringList newImageList;

    QFileInfoList fileList = currentDir.entryInfoList(QDir::Files);
    QStringList imageFilters = {"*.png",  "*.jpg", "*.bmp",  "*.jpeg",
                                "*.webp", "*.gif", "*.tiff", "*.tif"};
    QStringList archiveFilters = {"*.zip", "*.rar", "*.7z", "*.tar",
                                  "*.gz",  "*.bz2"};

    foreach (const QFileInfo &fileInfo, fileList) {
        bool isImage = false;
        foreach (const QString &filter, imageFilters) {
            if (fileInfo.fileName().endsWith(filter.mid(1), Qt::CaseInsensitive)) {
                newImageList.append(fileInfo.fileName());
                isImage = true;
                break;
            }
        }

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
        QString imagePath = imageList.at(index);
        result = loadImageFromArchive(imagePath);
        if (result) {
            currentImagePath = currentArchivePath + "|" + imagePath;
        }
    } else {
        QString imagePath = currentDir.absoluteFilePath(imageList.at(index));
        result = loadImage(imagePath, fromCache);
    }

    if (result) {
        currentImageIndex = index;

        if (currentViewMode == ThumbnailView) {
            thumbnailWidget->setSelectedIndex(currentImageIndex);
        }

        // 预加载前后图片
        preloadImagesAround(currentImageIndex, 2);

        if (isSlideshowActive) {
            int nextIndex = (currentImageIndex + 1) % imageList.size();
            if (isArchiveMode) {
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
                            }
                        }
                    });
                }
            } else {
                QString nextPath = currentDir.absoluteFilePath(imageList.at(nextIndex));
                if (!imageCache.contains(nextPath)) {
                    QtConcurrent::run([this, nextPath]() {
                        // 使用降采样预加载
                        QImageReader reader(nextPath);
                        reader.setAutoTransform(true);
                        QSize originalSize = reader.size();
                        if (originalSize.isValid()) {
                            QSize previewSize(800, 600);
                            int widthScale = originalSize.width() / previewSize.width();
                            int heightScale = originalSize.height() / previewSize.height();
                            int scale = qMax(1, qMin(widthScale, heightScale));
                            if (scale > 1) {
                                reader.setScaledSize(originalSize / scale);
                            }
                        }
                        QImage preview = reader.read();
                        if (!preview.isNull()) {
                            QMutexLocker locker(&cacheMutex);
                            imageCache.insert(nextPath, QPixmap::fromImage(preview));
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

    // 如果配置了跳过确认，直接执行删除
    if (currentConfig.skipMoveToTrashConfirmation) {
        performDeleteCurrentImage(); // 将实际删除操作提取为独立函数
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("确认删除"));
    msgBox.setText(tr("确定要将图片 '%1' 移动到回收站吗？")
                       .arg(QFileInfo(currentImagePath).fileName()));
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);

    QCheckBox *cb = new QCheckBox(tr("不再询问"));
    msgBox.setCheckBox(cb);

    if (msgBox.exec() == QMessageBox::Yes) {
        performDeleteCurrentImage(); // 执行删除
        if (cb->isChecked()) {
            currentConfig.skipMoveToTrashConfirmation = true;
            saveConfiguration();
        }
    }
}

void ImageWidget::performDeleteCurrentImage()
{
    QString imageToDelete = currentImagePath;
    int indexToDelete = currentImageIndex;

    if (moveFileToRecycleBin(imageToDelete)) {
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

void ImageWidget::updateGLTexture()
{
    if (!m_glTexture) {
        m_glTexture = new QOpenGLTexture(currentImage.mirrored());
        m_glTexture->setMinificationFilter(QOpenGLTexture::Linear);
        m_glTexture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_glTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    } else {
        m_glTexture->setData(currentImage.mirrored());
    }
}
