// imagewidget_slideshow.cpp
#include "imagewidget.h"

void ImageWidget::startSlideshow()
{
    if (imageList.size() > 1) {
        isSlideshowActive = true;
        slideshowTimer->start(slideshowInterval);
        updateWindowTitle();
    }
}

void ImageWidget::stopSlideshow()
{
    isSlideshowActive = false;
    slideshowTimer->stop();
    updateWindowTitle();
}

void ImageWidget::toggleSlideshow()
{
    if (isSlideshowActive) stopSlideshow();
    else                  startSlideshow();
}

void ImageWidget::setSlideshowInterval(int interval)
{
    slideshowInterval = interval;
    if (isSlideshowActive) {
        slideshowTimer->setInterval(slideshowInterval);
    }
}

// ---------------------------------------------------------------------
// 幻灯片推进 + 预加载下一张
// 注意：工作线程内只允许使用 QImage，禁止 QPixmap。
// ---------------------------------------------------------------------
void ImageWidget::slideshowNext()
{
    if (imageList.isEmpty()) {
        stopSlideshow();
        return;
    }

    int nextIndex = (currentImageIndex + 1) % imageList.size();

    // ---------- 压缩包模式预加载 ----------
    if (isArchiveMode) {
        QString nextPath = imageList.at(nextIndex);
        bool needLoad = false;
        {
            QMutexLocker locker(&cacheMutex);
            needLoad = !archiveImageCache.contains(nextPath);
        }
        if (needLoad) {
            QThreadPool::globalInstance()->start([this, nextPath]() {
                QByteArray data = archiveHandler.extractFile(nextPath);
                if (data.isEmpty()) return;

                QImage img;
                if (!img.loadFromData(data)) return;

                QMutexLocker locker(&cacheMutex);
                archiveImageCache.insert(nextPath, img);
                //qDebug() << "预加载压缩包图片:" << nextPath;
            });
        }
    }
    // ---------- 普通文件模式预加载 ----------
    else {
        QString nextPath = currentDir.absoluteFilePath(imageList.at(nextIndex));
        bool needLoad = false;
        {
            QMutexLocker locker(&cacheMutex);
            needLoad = !imageCache.contains(nextPath);
        }
        if (needLoad) {
            QThreadPool::globalInstance()->start([this, nextPath]() {
                QImage img;
                if (!img.load(nextPath)) return;

                QMutexLocker locker(&cacheMutex);
                imageCache.insert(nextPath, img);
            });
        }
    }

    loadImageByIndex(nextIndex, true);
}

// 主线程全量预加载（保留）
void ImageWidget::preloadAllImages()
{
    {
        QMutexLocker locker(&cacheMutex);
        imageCache.clear();
    }

    for (const QString &fileName : std::as_const(imageList)) {
        QString filePath = currentDir.absoluteFilePath(fileName);
        QImage img;
        if (img.load(filePath)) {
            QMutexLocker locker(&cacheMutex);
            imageCache.insert(filePath, img);
        }
    }
    updateWindowTitle();
}

void ImageWidget::clearImageCache()
{
    m_cacheGeneration.fetch_add(1);   // ★
    {
        QMutexLocker locker(&cacheMutex);
        imageCache.clear();
    }
    pixmapCache.clear();
    updateWindowTitle();
}
