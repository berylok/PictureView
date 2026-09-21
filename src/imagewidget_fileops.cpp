
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

#ifdef _WIN32
#include <shellapi.h>
#else
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#endif


#include <QPointer>       // 顶部加这个

// =====================================================================
// 统一的解码入口：按 maxDecode 限制最长边
// maxDecode = 0 表示不限制
// =====================================================================
static QImage loadImageWithLimit(const QString &filePath, int maxDecode)
{
    QImage img;

    if (maxDecode > 0) {
        QImageReader reader(filePath);
        reader.setAutoTransform(true);

        QSize origSize = reader.size();
        if (origSize.isValid() &&
            (origSize.width() > maxDecode || origSize.height() > maxDecode)) {
            QSize target = origSize.scaled(maxDecode, maxDecode, Qt::KeepAspectRatio);
            reader.setScaledSize(target);
            //qDebug() << "限制解码:" << origSize << "→" << target << filePath;
        }

        if (reader.read(&img)) return img;
        //qDebug() << "QImageReader 失败:" << reader.errorString() << filePath;
    }

    // 未限制 / 读取失败 → 普通加载兜底
    if (img.load(filePath)) return img;

    return QImage();
}



bool ImageWidget::moveFileToRecycleBin(const QString &filePath)
{
    return PlatformCompat::moveToRecycleBin(filePath);
}

bool ImageWidget::loadImage(const QString &filePath, bool fromCache)
{
    //qDebug() << "=== loadImage 开始 ===";
    //qDebug() << "文件路径:" << filePath;



    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        //qDebug() << "错误: 文件不存在";
        return false;
    }

    //qDebug() << "文件大小:" << fileInfo.size() << "字节";

    // 检查是否是压缩包
    if (ArchiveHandler::isSupportedArchive(filePath)) {
        //qDebug() << "检测为压缩包文件";
        return openArchive(filePath);
    }

    // 如果是压缩包模式，从压缩包加载
    if (isArchiveMode) {
        //qDebug() << "压缩包模式，从压缩包加载";
        return loadImageFromArchive(filePath);
    }

    //QFileInfo fileInfo(filePath);
    //qDebug() << "文件是否存在:" << fileInfo.exists();
    //qDebug() << "文件大小:" << fileInfo.size();
    //qDebug() << "文件权限:" << fileInfo.permissions();

    if (!fileInfo.exists()) {
        //qDebug() << "错误: 文件不存在";
        return false;
    }


    // 直接加载，绕过缓存进行测试
    QPixmap loadedPixmap;

    if (fromCache) {
        // ★ 第 1 级：pixmapCache（QCache，瞬时）
        if (QPixmap *cached = pixmapCache.object(filePath)) {
            if (!cached->isNull()) {
                loadedPixmap = *cached;
                //qDebug() << "命中 pixmap 缓存:" << filePath;
            }
        }

        // ★ 第 2 级：imageCache（QImage，要转换）
        if (loadedPixmap.isNull()) {
            QImage cached;
            {
                QMutexLocker locker(&cacheMutex);
                auto it = imageCache.constFind(filePath);
                if (it != imageCache.constEnd()) cached = it.value();
            }
            if (!cached.isNull()) {
                loadedPixmap = QPixmap::fromImage(cached);
                pixmapCache.insert(filePath, new QPixmap(loadedPixmap));   // 提升到第 1 级
                //qDebug() << "命中 QImage 缓存:" << filePath;
            }
        }
    }

    // 都没命中 → 从磁盘加载
    if (loadedPixmap.isNull()) {
        QImage img = loadImageWithLimit(filePath, currentConfig.maxDecodeSize);
        if (img.isNull()) {
            //qDebug() << "错误: 图片加载失败:" << filePath;
            return false;
        }
        loadedPixmap = QPixmap::fromImage(img);

        // 写两个缓存
        // 从磁盘加载后：
        {
            QMutexLocker locker(&cacheMutex);
            imageCache.insert(filePath, img);
        }
        pixmapCache.insert(filePath, new QPixmap(loadedPixmap));   // ★
    }

    if (loadedPixmap.isNull()) {
        //qDebug() << "错误: 加载后的 pixmap 为空";
        return false;
    }

    // 继续原有逻辑...
    // 修改这部分 - 只有未锁定时才重置变换
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
    //qDebug() << "图片设置完成";



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
    //qDebug() << "当前图片路径设置为:" << currentImagePath;

    // 检查目录是否改变
    bool dirChanged = (currentDir != fileInfo.absoluteDir());
    if (dirChanged) {
        currentDir = fileInfo.absoluteDir();
        loadImageList();
    }

    // 确保当前图片索引正确设置
    currentImageIndex = imageList.indexOf(fileInfo.fileName());
    //qDebug() << "当前图片索引:" << currentImageIndex;

    update();
    updateWindowTitle();
    //qDebug() << "=== loadImage 完成 ===";

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
        m_cacheGeneration.fetch_add(1);   // ★ 旧任务作废
        thumbnailWidget->setImageList(imageList, currentDir);
    }
}

bool ImageWidget::loadImageByIndex(int index, bool fromCache)
{
    if (imageList.isEmpty() || index < 0 || index >= imageList.size()) {
        return false;
    }

    bool result = false;

    // ==================== ① 加载当前图片 ====================
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

    // ==================== ③ 预加载前后 N 张 ====================
    if (imageList.size() > 1 && currentConfig.preloadRange > 0) {
        const int n = imageList.size();
        const int range = qMin(currentConfig.preloadRange, (n - 1) / 2);   // 不超过一半

        // 收集需要预加载的索引：近的先加载
        QList<int> idxToLoad;
        for (int d = 1; d <= range; ++d) {
            int pIdx = (currentImageIndex - d + n) % n;
            int nIdx = (currentImageIndex + d) % n;
            if (pIdx != currentImageIndex && !idxToLoad.contains(pIdx))
                idxToLoad.append(pIdx);
            if (nIdx != currentImageIndex && !idxToLoad.contains(nIdx))
                idxToLoad.append(nIdx);
        }

        // 按顺序提交到线程池（最近的先跑）
        for (int idx : std::as_const(idxToLoad)) {
            if (isArchiveMode) {
                QString itemPath = imageList.at(idx);
                bool needLoad = false;
                {
                    QMutexLocker locker(&cacheMutex);
                    needLoad = !archiveImageCache.contains(itemPath);
                }
                if (!needLoad) continue;

                // 提交任务前记录当前代际
                const int currentGen = m_cacheGeneration.load();
                int maxDecode = currentConfig.maxDecodeSize;
                QPointer<ImageWidget> guard(this);

                QThreadPool::globalInstance()->start([guard, itemPath, maxDecode, currentGen]() {
                    if (!guard) return;

                    QImage img = loadImageWithLimit(itemPath, maxDecode);
                    if (img.isNull()) return;
                    if (!guard) return;

                    // ★ 写缓存前检查代际
                    if (guard->m_cacheGeneration.load() != currentGen) {
                        qDebug() << "丢弃旧代际预加载:" << itemPath;
                        return;
                    }

                    QMutexLocker locker(&guard->cacheMutex);
                    guard->imageCache.insert(itemPath, img);
                });
            } else {
                QString itemPath = currentDir.absoluteFilePath(imageList.at(idx));
                bool needLoad = false;
                {
                    QMutexLocker locker(&cacheMutex);
                    needLoad = !imageCache.contains(itemPath);
                }
                if (!needLoad) continue;

                int maxDecode = currentConfig.maxDecodeSize;
                QPointer<ImageWidget> guard(this);

                QThreadPool::globalInstance()->start([guard, itemPath, maxDecode]() {
                    if (!guard) return;

                    QImage img = loadImageWithLimit(itemPath, maxDecode);
                    if (img.isNull()) return;
                    if (!guard) return;

                    QMutexLocker locker(&guard->cacheMutex);
                    guard->imageCache.insert(itemPath, img);
                });
            }
        }
    }

    return result;
}

void ImageWidget::loadNextImage()
{
    //qDebug() << "=== loadNextImage 开始 ===";
    //qDebug() << "当前模式:" << (currentViewMode == SingleView ? "单张" : "缩略图");
    //qDebug() << "当前索引:" << currentImageIndex << "，图片总数:" << imageList.size();

    if (imageList.isEmpty()) {
        //qDebug() << "图片列表为空，返回";
        return;
    }

    int nextIndex = (currentImageIndex + 1) % imageList.size();
    //qDebug() << "计算出的下一个索引:" << nextIndex;

    if (currentViewMode == SingleView) {
        //qDebug() << "单张模式，加载图片";
        loadImageByIndex(nextIndex, true);
    } else {
        // 缩略图模式下，只更新索引和选中状态
        //qDebug() << "缩略图模式，更新选中状态";
        currentImageIndex = nextIndex;
        thumbnailWidget->setSelectedIndex(currentImageIndex);
        thumbnailWidget->ensureVisible(currentImageIndex);
        updateWindowTitle();

        //qDebug() << "更新后的当前索引:" << currentImageIndex;
    }

    //qDebug() << "=== loadNextImage 结束 ===";
}

void ImageWidget::loadPreviousImage()
{
    //qDebug() << "=== loadPreviousImage 开始 ===";
    //qDebug() << "当前模式:" << (currentViewMode == SingleView ? "单张" : "缩略图");
    //qDebug() << "当前索引:" << currentImageIndex << "，图片总数:" << imageList.size();

    if (imageList.isEmpty()) {
        //qDebug() << "图片列表为空，返回";
        return;
    }

    int prevIndex = (currentImageIndex - 1 + imageList.size()) % imageList.size();
    //qDebug() << "计算出的上一个索引:" << prevIndex;

    if (currentViewMode == SingleView) {
        //qDebug() << "单张模式，加载图片";
        loadImageByIndex(prevIndex, true);
    } else {
        // 缩略图模式下，只更新索引和选中状态
        //qDebug() << "缩略图模式，更新选中状态";
        currentImageIndex = prevIndex;
        thumbnailWidget->setSelectedIndex(currentImageIndex);
        thumbnailWidget->ensureVisible(currentImageIndex);
        updateWindowTitle();

        //qDebug() << "更新后的当前索引:" << currentImageIndex;
    }

    //qDebug() << "=== loadPreviousImage 结束 ===";
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

    if (!moveFileToRecycleBin(imageToDelete)) {
        QMessageBox::critical(this, tr("错误"), tr("移动图片到回收站失败"));
        return;
    }

    // ★ 锁只包住 map 的 remove，立刻释放
    {
        QMutexLocker locker(&cacheMutex);
        imageCache.remove(imageToDelete);
        archiveImageCache.remove(imageToDelete);   // 如果删的是压缩包内图，也清
    }

    // ★ QCache 是主线程专用，无需锁
    pixmapCache.remove(imageToDelete);

    // ★ 缩略图缓存（静态，内部自己加锁）
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
                loadImageByIndex(newIndex);      // 这里会再锁 cacheMutex，但现在锁已释放 ✓
            } else {
                currentImageIndex = newIndex;
                thumbnailWidget->setSelectedIndex(newIndex);
            }
        }

        update();
        updateWindowTitle();
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


