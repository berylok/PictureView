// imagewidget_slideshow.cpp
#include "imagewidget.h"

void ImageWidget::startSlideshow()
{
    if (imageList.size() > 1) {
        // 不再预加载所有图片，改为按需加载
        isSlideshowActive = true;
        slideshowTimer->start(slideshowInterval);
        updateWindowTitle();
        // 移除日志消息，避免干扰
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
    if (isSlideshowActive) {
        stopSlideshow();
    } else {
        startSlideshow();
    }
}

void ImageWidget::setSlideshowInterval(int interval)
{
    slideshowInterval = interval;
    if (isSlideshowActive) {
        slideshowTimer->setInterval(slideshowInterval);
    }
}

void ImageWidget::slideshowNext()
{
    if (imageList.isEmpty()) {
        stopSlideshow();
        return;
    }

    int nextIndex = (currentImageIndex + 1) % imageList.size();

    // 预加载下一张图片（如果不在缓存中）
    if (!imageCache.contains(currentDir.absoluteFilePath(imageList.at(nextIndex)))) {
        QtConcurrent::run([this, nextIndex]() {
            QString nextPath = currentDir.absoluteFilePath(imageList.at(nextIndex));
            QPixmap tempPixmap;
            if (tempPixmap.load(nextPath)) {
                QMutexLocker locker(&cacheMutex);
                imageCache.insert(nextPath, tempPixmap);
            }
        });
    }

    // 加载当前图片
    loadImageByIndex(nextIndex, true);
}

void ImageWidget::preloadAllImages()
{
    imageCache.clear();

    int loadedCount = 0;
    for (const QString &fileName : imageList) {
        QString filePath = currentDir.absoluteFilePath(fileName);
        QPixmap tempPixmap;
        if (tempPixmap.load(filePath)) {
            imageCache.insert(filePath, tempPixmap);
            loadedCount++;
            // 移除单条日志消息，减少干扰
        }
    }

    updateWindowTitle();
}

void ImageWidget::clearImageCache()
{
    int cacheSize = imageCache.size();
    imageCache.clear();
    updateWindowTitle();
}
