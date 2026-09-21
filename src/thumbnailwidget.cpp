
#include "thumbnailwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFileInfo>
#include <QApplication>
#include <QFutureWatcher>
#include <QtConcurrent>
#include "imagewidget.h"
#include <QPainterPath>
#include <QScrollArea>
#include <QElapsedTimer>
#include <QImageReader>
#include <QCache>
#include <QTimer>
#include <QFont>
#include <QThreadPool>
#include <QStandardPaths>
#include <QCryptographicHash>

// 初始化静态成员变量
QMap<QString, QPixmap> ThumbnailWidget::thumbnailCache;
QMutex ThumbnailWidget::cacheMutex;

ThumbnailWidget::ThumbnailWidget(ImageWidget *imageWidget, QWidget *parent)
    : QWidget(parent),
    imageWidget(imageWidget),
    thumbnailSize(250, 250),
    thumbnailSpacing(7),
    selectedIndex(-1),
    loadedCount(0),
    totalCount(0),
    futureWatcher(nullptr),
    isLoading(false),
    currentBatchIndex(0),
    batchLoadTimer(this),
    diagnosticTimer(nullptr)
{
    // ★ 在函数体里初始化，此时 perfConfig 已经构造好了
    smartThumbnailCache.setMaxCost(perfConfig.maxCacheMemoryMB * 1024 * 1024);

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // 缩略图专用线程池，最多用一半核心，避免抢主线程
    m_pool = new QThreadPool(this);
    int maxThreads = qMax(2, QThread::idealThreadCount() / 2);
    m_pool->setMaxThreadCount(maxThreads);
    //qDebug() << "缩略图线程池大小:" << maxThreads;
    // 缩略图磁盘缓存目录
    m_thumbCacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                      + "/thumbs";
    QDir().mkpath(m_thumbCacheDir);
    qDebug() << "缩略图缓存目录:" << m_thumbCacheDir;


    // 可选：设置缓存清理策略
    smartThumbnailCache.setMaxCost(perfConfig.maxCacheMemoryMB * 1024 * 1024);

    // 设置批量加载定时器
    batchLoadTimer.setSingleShot(true);
    batchLoadTimer.setInterval(perfConfig.batchLoadDelay);
    connect(&batchLoadTimer, &QTimer::timeout, this, &ThumbnailWidget::processBatchLoad);

    // 诊断定时器 - 每5秒检查一次加载状态
#ifdef QT_DEBUG
    diagnosticTimer = new QTimer(this);
    connect(diagnosticTimer, &QTimer::timeout, this, &ThumbnailWidget::logThumbnailStatus);
    diagnosticTimer->start(5000);
#else
    diagnosticTimer = nullptr;
#endif

    // 缩略图专用线程池（不占用全局线程池）
    m_pool->setMaxThreadCount(qMax(2, QThread::idealThreadCount() / 2));
    qDebug() << "缩略图线程池大小:" << m_pool->maxThreadCount();

}

ThumbnailWidget::~ThumbnailWidget()
{
    stopLoading();
    if (m_pool) {
        m_pool->waitForDone(2000);   // 最多 2 秒
    }
}

// 修改 setImageList 方法，加载所有缩略图
void ThumbnailWidget::setImageList(const QStringList &list, const QDir &dir)
{
    qDebug() << "[" << QTime::currentTime() << "] setImageList 数量:" << list.size();
    qDebug() << "设置缩略图列表，数量:" << list.size();

    // ★ 关键：作废所有在途任务
    m_generation.fetch_add(1);

    // ★ 清空队列中"还没开始跑"的任务（正在跑的等它跑完）
    if (m_pool) {
        m_pool->clear();
    }

    stopLoading();       // 停定时器

    imageList = list;
    currentDir = dir;
    selectedIndex = -1;
    loadedCount = 0;
    totalCount = list.size();
    currentBatchIndex = 0;
    pendingLoadRequests.clear();
    allFilesToLoad.clear();

    update();

    emit loadingProgress(0, totalCount);

    startLoadingAllThumbnails();                // ★ 重新开始批量加载
}

// 开始加载所有缩略图
void ThumbnailWidget::startLoadingAllThumbnails()
{
    if (imageList.isEmpty()) return;
    qDebug() << "[" << QTime::currentTime() << "] 开始加载缩略图，总数:" << imageList.size();
    allFilesToLoad = imageList;
    currentBatchIndex = 0;
    processBatchLoad();
}

// 处理批量加载
void ThumbnailWidget::processBatchLoad()
{
    if (currentBatchIndex >= allFilesToLoad.size()) { finishLoading(); return; }
    isLoading = true;

    int startIndex = currentBatchIndex;
    int endIndex = qMin(currentBatchIndex + perfConfig.batchLoadSize,
                        allFilesToLoad.size());

    QStringList batchFiles;
    for (int i = startIndex; i < endIndex; ++i) {
        const QString &fileName = allFilesToLoad[i];
        QString cacheKey = getCacheKey(fileName);

        // 主线程检查缓存，命中就跳过去加载
        if (smartThumbnailCache.contains(cacheKey)) {
            loadedCount++;
            continue;
        }
        batchFiles.append(fileName);
    }
    currentBatchIndex = endIndex;

    if (!batchFiles.isEmpty()) {
        qDebug() << "[" << QTime::currentTime() << "] 提交批次，"<< batchFiles.size() << "个任务到线程池";
        loadThumbnailsBatch(batchFiles);
    } else if (currentBatchIndex >= allFilesToLoad.size()
               && loadedCount >= totalCount) {
        finishLoading();
    }

    if (currentBatchIndex < allFilesToLoad.size()) {
        batchLoadTimer.start();
    }
}

// 批量加载缩略图
void ThumbnailWidget::loadThumbnailsBatch(const QStringList &fileNames)
{
    if (fileNames.isEmpty()) return;

    const int gen = m_generation.load();       // ★ 捕获当前代际
    QPointer<ThumbnailWidget> guard = this;

    m_pool->start( [guard, fileNames, gen]() {   // ★ 用 m_pool
        qDebug() << "[" << QTime::currentTime() << "] 工作线程启动，处理"<< fileNames.size() << "个文件";
        if (!guard) return;
        if (guard->m_generation.load() != gen) return;      // ★ 检查

        QList<QPair<QString, QImage>> loadedResults;

        for (const QString &fileName : fileNames) {
            if (!guard) return;
            if (guard->m_generation.load() != gen) return;  // ★ 每张图都检查

            QImage img = guard->loadSingleThumbnail(fileName);
            if (!img.isNull()) {
                loadedResults.append(qMakePair(fileName, img));
            }
        }

        if (!guard) return;
        if (guard->m_generation.load() != gen) return;


        // ⑤ 回主线程：QPixmap 只能在主线程创建
        const int resultCount = loadedResults.size();   // ★ 先记住数量
        QMetaObject::invokeMethod(guard.data(),
                                  [guard, loadedResults = std::move(loadedResults)]() {
            qDebug() << "[" << QTime::currentTime() << "] 批次完成，写缓存"<< loadedResults.size() << "张";
                                      if (!guard) return;   // ⑥ 主线程执行时再判一次

                                      for (const auto &pair : loadedResults) {
                                          QString cacheKey = guard->getCacheKey(pair.first);

                                          QPixmap thumbnail = QPixmap::fromImage(pair.second);
                                          if (thumbnail.isNull()) continue;

                                          int cost = guard->calculateCostForPixmap(thumbnail);

                                          guard->smartThumbnailCache.insert(
                                              cacheKey, new QPixmap(thumbnail), cost);

                                          {
                                              QMutexLocker locker(&guard->cacheMutex);
                                              guard->thumbnailCache.insert(cacheKey, thumbnail);
                                          }

                                          guard->loadedCount++;
                                      }

                                      emit guard->loadingProgress(guard->loadedCount, guard->totalCount);
                                      guard->update();

                                      if (guard->currentBatchIndex >= guard->allFilesToLoad.size() &&
                                          guard->loadedCount >= guard->totalCount) {
                                          guard->finishLoading();
                                      }
                                  }, Qt::QueuedConnection);

         qDebug() << "[" << QTime::currentTime() << "] worker done,"<< resultCount << "个";
    });
}
// 新增完成处理函数
void ThumbnailWidget::finishLoading()
{
    isLoading = false;
    //qDebug() << "所有缩略图批次加载完成";
    emit loadingProgress(loadedCount, totalCount);
    update();
}

// 加载单个缩略图
QImage ThumbnailWidget::loadSingleThumbnail(const QString &fileName)
{
    QImage result;

    try {
        if (fileName.contains("|")) {
            // 压缩包内图片：ImageWidget 已返回 QImage
            if (imageWidget) {
                result = imageWidget->getArchiveThumbnailImage(fileName);
                if (result.isNull()) {
                    result = createArchiveIconImage();
                }
            } else {
                result = createArchiveIconImage();
            }
        } else if (isArchiveFile(fileName)) {
            // 顶层压缩包直接返回图标
            result = createArchiveIconImage();
        } else {
            QString fullPath = currentDir.absoluteFilePath(fileName);
            if (QFile::exists(fullPath)) {
                result = loadImageFileFast(fullPath);
                if (result.isNull()) {
                    result = createArchiveIconImage();
                }
            } else {
                result = createArchiveIconImage();
            }
        }
    } catch (const std::exception &e) {
        //qDebug() << "加载缩略图异常:" << e.what() << "文件:" << fileName;
        result = createArchiveIconImage();
    } catch (...) {
        //qDebug() << "加载缩略图未知异常，文件:" << fileName;
        result = createArchiveIconImage();
    }

    if (result.isNull()) {
        result = createArchiveIconImage();
    }
    return result;
}

QImage ThumbnailWidget::loadImageFileFast(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isReadable() || fi.size() == 0) return QImage();

    // ---------- 1. 磁盘缓存查找 ----------
    QString key = QCryptographicHash::hash(filePath.toUtf8(),
                                           QCryptographicHash::Md5).toHex();
    QString cachePath = m_thumbCacheDir + "/" + key + ".jpg";

    if (QFile::exists(cachePath)) {
        // 检查原文件是否比缓存新（原文件改了要重解）
        if (fi.lastModified() <= QFileInfo(cachePath).lastModified()) {
            QImage cached(cachePath);
            if (!cached.isNull()) {
                return cached;
            }
        }
    }

    // ---------- 2. 解码原图 ----------
    QImageReader reader(filePath);
    if (!reader.canRead()) return QImage();

    reader.setAutoTransform(true);

    QSize origSize = reader.size();
    if (origSize.isValid() && !origSize.isEmpty()) {
        QSize decodeTarget = origSize.scaled(thumbnailSize * 2, Qt::KeepAspectRatio);
        reader.setScaledSize(decodeTarget);
    }

    QImage image;
    if (!reader.read(&image) || image.isNull()) return QImage();

    QImage thumb = (image.size() == thumbnailSize)
                       ? image
                       : image.scaled(thumbnailSize, Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation);

    // ---------- 3. 写缓存（JPEG 质量 85） ----------
    if (!thumb.isNull()) {
        thumb.save(cachePath, "JPEG", 85);
    }

    return thumb;
}

QImage ThumbnailWidget::scaleImageWithAspectRatio(const QImage &original) const
{
    if (original.isNull()) return QImage();

    // 保持宽高比进行缩放
    return original.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

// 绘制方法
void ThumbnailWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(25, 25, 25)); // 深色背景更好看

    if (imageList.isEmpty()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("欢迎使用""PictureView""！\n\n"
                            "使用方法：\n"
                            "• 按 Ctrl+O 打开文件夹浏览图片\n"
                            "• 按 Ctrl+Shift+O 打开单张图片\n"
                            "• 按 F1 查看详细使用说明\n\n"
                            "祝您使用愉快！""\n\n"
                            "没有图片文件\n拖拽图片文件夹到此窗口或右键选择打开文件夹"
                            "\n\n"
                            "F1 查看帮助 或右键弹出菜单使用"));
        return;
    }

    // 设置只绘制脏矩形区域
    painter.setClipRect(event->rect());

    int itemsPerRow = calculateItemsPerRow();

    // 绘制所有缩略图
    for (int i = 0; i < imageList.size(); ++i) {
        QString fileName = imageList.at(i);

        // 计算位置
        int row = i / itemsPerRow;
        int col = i % itemsPerRow;
        int currentX = thumbnailSpacing + col * (thumbnailSize.width() + thumbnailSpacing);
        int currentY = thumbnailSpacing + row * (thumbnailSize.height() + thumbnailSpacing + 25);

        QRect thumbRect(currentX, currentY, thumbnailSize.width(), thumbnailSize.height() + 25);

        // 只绘制在脏矩形区域内的缩略图
        if (!event->rect().intersects(thumbRect)) {
            continue;
        }

        QString cacheKey = getCacheKey(fileName);
        bool isTopLevelArchive = isArchiveFile(fileName) && !fileName.contains("|");

        // 获取缩略图（智能缓存优先）
        QPixmap thumbnail = getCachedThumbnail(cacheKey);
        if (thumbnail.isNull() && isTopLevelArchive) {
            thumbnail = QPixmap::fromImage(createArchiveIconImage());   // 主线程转，OK
        }

        drawThumbnailItem(painter, i, currentX, currentY, fileName, thumbnail, isTopLevelArchive);
    }

    // 显示加载状态
    if (isLoading) {
        painter.setPen(QColor(200, 200, 200));
        painter.drawText(10, 20, QString(tr("Loading: %1/%2")).arg(loadedCount).arg(totalCount));
    }

    updateMinimumHeight();
}



void ThumbnailWidget::drawThumbnailItem(QPainter &painter, int index,
                                        int x, int y, const QString &fileName,
                                        const QPixmap &thumbnail, bool isArchive)
{
    QRect borderRect(x, y, thumbnailSize.width(), thumbnailSize.height());

    // 绘制选中状态
    if (index == selectedIndex) {
        QPainterPath path;
        path.addRoundedRect(borderRect.adjusted(-3, -3, 3, 3), 5, 5);
        // ★ 从 ImageWidget 的 currentConfig 拿颜色
        QColor hl("#00A0E9");
        if (imageWidget) {
            hl = QColor(imageWidget->currentConfig.highlightColor);
            if (!hl.isValid()) hl = QColor("#00A0E9");
        }
        hl.setAlpha(200);
        painter.fillPath(path, hl);
    }

    // 绘制背景
    painter.fillRect(borderRect, QColor(45, 45, 45));

    // 绘制缩略图或占位符
    if (!thumbnail.isNull()) {
        // 计算居中位置
        int thumbX = x + (thumbnailSize.width() - thumbnail.width()) / 2;
        int thumbY = y + (thumbnailSize.height() - thumbnail.height()) / 2;
        QRect thumbRect(thumbX, thumbY, thumbnail.width(), thumbnail.height());

        painter.drawPixmap(thumbRect, thumbnail);

        // 绘制边框
        painter.setPen(QColor(100, 100, 100));
        painter.drawRect(borderRect);
    } else if (isArchive) {
        // 压缩包图标 - 已经保持比例
        painter.drawImage(borderRect, createArchiveIconImage());   // 不需要转 Pixmap
    } else {
        // 加载中占位符
        painter.setPen(QColor(150, 150, 150));
        painter.drawText(borderRect, Qt::AlignCenter, tr("加载中..."));
    }

    // 绘制文件名
    QRect textRect(x, y + thumbnailSize.height(), thumbnailSize.width(), 20);
    QString displayName = getDisplayName(fileName);

    painter.setPen(Qt::white);
    painter.setFont(QFont("Microsoft YaHei", 8));
    painter.drawText(textRect, Qt::AlignCenter | Qt::TextElideMode::ElideMiddle, displayName);
}

// 工具方法
QString ThumbnailWidget::getCacheKey(const QString &fileName) const
{
    return fileName.contains("|") ? fileName : currentDir.absoluteFilePath(fileName);
}

QString ThumbnailWidget::getDisplayName(const QString &fileName) const
{
    if (fileName.contains("|")) {
        QStringList parts = fileName.split("|");
        return QFileInfo(parts[1]).fileName();
    }
    return QFileInfo(fileName).fileName();
}

void ThumbnailWidget::updateMinimumHeight()
{
    if (imageList.isEmpty()) {
        setMinimumHeight(300);
        return;
    }

    int itemsPerRow = calculateItemsPerRow();
    int rows = (imageList.size() + itemsPerRow - 1) / itemsPerRow;
    int minHeight = thumbnailSpacing + rows * (thumbnailSize.height() + thumbnailSpacing + 25);
    setMinimumHeight(minHeight);
}

int ThumbnailWidget::calculateItemsPerRow() const
{
    int maxWidth = width();
    return qMax(1, (maxWidth - thumbnailSpacing) /
                       (thumbnailSize.width() + thumbnailSpacing));
}

// 压缩包图标
QImage ThumbnailWidget::createArchiveIconImage() const
{
    // 用 QImage 而不是 QPixmap
    QImage icon(thumbnailSize, QImage::Format_ARGB32_Premultiplied);
    icon.fill(QColor(70, 130, 180, 200));

    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(QColor(100, 160, 210, 150));

    QRectF rect(icon.width() * 0.2, icon.height() * 0.3,
                icon.width() * 0.6, icon.height() * 0.4);
    painter.drawRoundedRect(rect, 5, 5);

    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(icon.rect(), Qt::AlignCenter, "ZIP");
    painter.end();
    return icon;
}
// 停止加载
void ThumbnailWidget::stopLoading()
{
    batchLoadTimer.stop();
    if (futureWatcher && futureWatcher->isRunning()) {
        futureWatcher->cancel();
        futureWatcher->waitForFinished();
    }
    isLoading = false;
}

// 诊断方法
void ThumbnailWidget::diagnoseLoadingIssues()
{
    //qDebug() << "=== 缩略图加载问题诊断 ===";
    //qDebug() << "总图片数量:" << imageList.size();
    //qDebug() << "智能缓存数量:" << smartThumbnailCache.size();
    //qDebug() << "静态缓存数量:" << thumbnailCache.size();
    //qDebug() << "已加载数量:" << loadedCount;
    //qDebug() << "失败缩略图:" << failedThumbnails.size();

    // 检查每个文件的状态
    for (int i = 0; i < imageList.size(); ++i) {
        QString fileName = imageList.at(i);
        QString cacheKey = getCacheKey(fileName);

        bool inSmartCache = smartThumbnailCache.contains(cacheKey);
        bool inStaticCache = thumbnailCache.contains(cacheKey);
        bool isFailed = failedThumbnails.contains(cacheKey);

        if (!inSmartCache && !inStaticCache && !isFailed) {
            //qDebug() << "未加载的文件:" << fileName;
            //qDebug() << "  - 索引:" << i;
            //qDebug() << "  - 缓存键:" << cacheKey;
            //qDebug() << "  - 是否压缩包:" << (fileName.contains("|") || isArchiveFile(fileName));
        }
    }

    //qDebug() << "=== 诊断结束 ===";
}

void ThumbnailWidget::logThumbnailStatus()
{
    if (imageList.isEmpty()) return;

    int loaded = 0;
    int failed = failedThumbnails.size();
    int total = imageList.size();

    for (int i = 0; i < imageList.size(); ++i) {
        QString fileName = imageList.at(i);
        QString cacheKey = getCacheKey(fileName);

        if (smartThumbnailCache.contains(cacheKey) || thumbnailCache.contains(cacheKey)) {
            loaded++;
        }
    }

    // //qDebug() << "缩略图状态 - 已加载:" << loaded << "/" << total
    //          << "失败:" << failed;

    // 如果有很多失败的，尝试重新加载
    if (failed > total * 0.3) { // 超过30%失败
        //qDebug() << "检测到大量失败，尝试重新加载...";
        forceReloadAll();
    }
}

void ThumbnailWidget::forceReloadAll()
{
    //qDebug() << "强制重新加载所有缩略图";

    // 停止所有正在进行的加载
    stopLoading();

    // 清空所有缓存和状态
    smartThumbnailCache.clear();
    {
        QMutexLocker locker(&cacheMutex);
        thumbnailCache.clear();
    }

    pendingLoadRequests.clear();
    failedThumbnails.clear();
    loadingErrors.clear();
    currentBatchIndex = 0;

    // 重新开始加载
    startLoadingAllThumbnails();
    update();
}

void ThumbnailWidget::retryFailedThumbnails()
{
    qDebug() << "重试失败的缩略图，数量:" << failedThumbnails.size();

    const QList<QString> retryKeys = failedThumbnails.values();
    if (retryKeys.isEmpty()) return;

    const int gen = m_generation.load();       // ★ 捕获当前代际
    QPointer<ThumbnailWidget> guard = this;

    for (const QString &cacheKey : retryKeys) {
        QString fileName = cacheKey.contains("|") ? cacheKey
                                                  : QFileInfo(cacheKey).fileName();

        m_pool->start([guard, fileName, gen]() {
            if (!guard) return;
            if (guard->m_generation.load() != gen) return;     // ★ 检查代际

            QImage thumbnail = guard->loadSingleThumbnail(fileName);
            if (thumbnail.isNull()) return;
            if (!guard) return;
            if (guard->m_generation.load() != gen) return;     // ★ 再检查一次

            QMetaObject::invokeMethod(guard.data(),
                                      [guard, fileName, thumbnail, gen]() {           // ★ 捕获 gen
                                          if (!guard) return;
                                          if (guard->m_generation.load() != gen) return;  // ★ 主线程再检查

                                          QString key = guard->getCacheKey(fileName);

                                          QPixmap pixmap = QPixmap::fromImage(thumbnail);
                                          if (pixmap.isNull()) return;

                                          int cost = guard->calculateCostForPixmap(pixmap);
                                          guard->smartThumbnailCache.insert(key, new QPixmap(pixmap), cost);

                                          {
                                              QMutexLocker locker(&cacheMutex);
                                              thumbnailCache.insert(key, pixmap);
                                          }

                                          guard->failedThumbnails.remove(key);
                                          guard->loadingErrors.remove(key);
                                          guard->loadedCount++;
                                          guard->update();
                                      }, Qt::QueuedConnection);
        });
    }
}

bool ThumbnailWidget::isArchiveFile(const QString &fileName) const
{
    QString lowerName = fileName.toLower();
    return (lowerName.endsWith(".zip") || lowerName.endsWith(".rar") ||
            lowerName.endsWith(".7z") || lowerName.endsWith(".tar") ||
            lowerName.endsWith(".gz") || lowerName.endsWith(".bz2"));
}

// 其他现有方法保持不变...
void ThumbnailWidget::setSelectedIndex(int index)
{
    if (index >= -1 && index < imageList.size() && index != selectedIndex) {
        selectedIndex = index;
        update();
        ensureVisible(index);
        //qDebug() << "ThumbnailWidget 选中索引:" << index;
    }
}

int ThumbnailWidget::getSelectedIndex() const
{
    return selectedIndex;
}

void ThumbnailWidget::ensureVisible(int index)
{
    if (index < 0 || index >= imageList.size()) return;

    int maxWidth = width();
    int itemsPerRow = qMax(1, (maxWidth - thumbnailSpacing) /
                                  (thumbnailSize.width() + thumbnailSpacing));

    int row = index / itemsPerRow;
    int col = index % itemsPerRow;

    int x = thumbnailSpacing + col * (thumbnailSize.width() + thumbnailSpacing);
    int y = thumbnailSpacing + row * (thumbnailSize.height() + thumbnailSpacing + 25);

    // 创建一个稍大的矩形区域，确保缩略图完全可见
    QRect visibleRect(x, y, thumbnailSize.width(), thumbnailSize.height() + 25);
    emit ensureRectVisible(visibleRect);
}

void ThumbnailWidget::clearThumbnailCache()
{
    QMutexLocker locker(&cacheMutex);
    thumbnailCache.clear();
    smartThumbnailCache.clear();
}

void ThumbnailWidget::clearThumbnailCacheForImage(const QString &imagePath)
{
    QMutexLocker locker(&cacheMutex);
    thumbnailCache.remove(imagePath);
}

void ThumbnailWidget::setThumbnailSize(const QSize &size)
{
    if (thumbnailSize != size) {
        thumbnailSize = size;
        // 尺寸变化时清空缓存
        smartThumbnailCache.clear();
        QMutexLocker locker(&cacheMutex);
        thumbnailCache.clear();
        update();
    }
}



void ThumbnailWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        selectThumbnailAtPosition(event->pos());
        if (selectedIndex >= 0) {
            emit thumbnailClicked(selectedIndex);
        }
    }
}

void ThumbnailWidget::selectThumbnailAtPosition(const QPoint &pos)
{
    if (imageList.isEmpty()) return;

    int x = thumbnailSpacing;
    int y = thumbnailSpacing;
    int maxWidth = width();

    for (int i = 0; i < imageList.size(); ++i) {
        QRect clickArea(x, y, thumbnailSize.width(), thumbnailSize.height() + 25);

        if (clickArea.contains(pos)) {
            selectedIndex = i;
            update();
            ensureVisible(i);
            return;
        }

        x += thumbnailSize.width() + thumbnailSpacing;
        if (x + thumbnailSize.width() > maxWidth) {
            x = thumbnailSpacing;
            y += thumbnailSize.height() + thumbnailSpacing + 25;
        }
    }

    selectedIndex = -1;
    update();
}

void ThumbnailWidget::keyPressEvent(QKeyEvent *event)
{
    //qDebug() << "ThumbnailWidget 接收到按键:" << event->key();

    if (imageList.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    {
        int newIndex = selectedIndex;
        if (newIndex < 0) newIndex = 0;

        if (event->key() == Qt::Key_Left) {
            newIndex = (newIndex - 1 + imageList.size()) % imageList.size();
        } else {
            newIndex = (newIndex + 1) % imageList.size();
        }

        if (newIndex != selectedIndex) {
            selectedIndex = newIndex;
            update();
            ensureVisible(selectedIndex);
        }
        event->accept();
    }
    break;
    case Qt::Key_Enter:
    case Qt::Key_Return:
        //qDebug() << "处理回车键，选中索引:" << selectedIndex;
        if (selectedIndex >= 0) {
            emit thumbnailClicked(selectedIndex);
        }
        event->accept();
        break;

    default:
        QWidget::keyPressEvent(event);
    }
}

void ThumbnailWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateMinimumHeight();
}

void ThumbnailWidget::updateThumbnails()
{
    // 这个方法现在不需要了，因为我们使用批量加载
}


// 计算 pixmap 的实际内存占用（字节）
int ThumbnailWidget::calculateCostForPixmap(const QPixmap &pixmap) const
{
    if (pixmap.isNull()) return 0;

    // 方法1：使用 QImage 的 byteCount（更准确）
    QImage image = pixmap.toImage();
    if (!image.isNull()) {
        return image.sizeInBytes();
    }

    // 方法2：估算（宽*高*4字节RGBA）
    return pixmap.width() * pixmap.height() * 4;
}

// 添加缓存统计方法
// thumbnailwidget.cpp

void ThumbnailWidget::logCacheStats()
{
    //qDebug() << "========================================";
    //qDebug() << "=== 缩略图缓存统计 ===";
    //qDebug() << "智能缓存条目数:" << smartThumbnailCache.size();
    //qDebug() << "智能缓存总成本:" << smartThumbnailCache.totalCost() / (1024.0 * 1024.0) << "MB";
    //qDebug() << "智能缓存最大容量:" << smartThumbnailCache.maxCost() / (1024.0 * 1024.0) << "MB";
    //qDebug() << "智能缓存使用率:" << QString::number(smartThumbnailCache.totalCost() * 100.0 /
                                                         //smartThumbnailCache.maxCost(), 'f', 1) << "%";

    // 静态缓存统计
    QMutexLocker locker(&cacheMutex);
    qDebug() << "静态缓存条目数:" << thumbnailCache.size();

    // 计算静态缓存估算内存（粗略）
    int staticCacheMemory = 0;
    for (auto it = thumbnailCache.begin(); it != thumbnailCache.end(); ++it) {
        staticCacheMemory += it->width() * it->height() * 4;
    }
    qDebug() << "静态缓存估算内存:" << staticCacheMemory / (1024.0 * 1024.0) << "MB";



    if (!failedThumbnails.isEmpty() && failedThumbnails.size() <= 10) {
        for (const QString& key : failedThumbnails) {
        }
    }

    // 性能配置
    //qDebug() << "=== 性能配置 ===";
    //qDebug() << "最大缓存内存:" << perfConfig.maxCacheMemoryMB << "MB";
    //qDebug() << "批量加载大小:" << perfConfig.batchLoadSize;
    //qDebug() << "批量加载延迟:" << perfConfig.batchLoadDelay << "ms";
    //qDebug() << "预加载范围:" << preloadRange;
    //qDebug() << "懒加载模式:" << (perfConfig.enableLazyLoading ? "启用" : "禁用");
    //qDebug() << "加载状态:" << (isLoading ? "加载中" : "空闲");
    //qDebug() << "========================================";
}

// 设置缓存大小（MB）
void ThumbnailWidget::setCacheSize(int maxSizeMB)
{
    perfConfig.maxCacheMemoryMB = maxSizeMB;
    int maxCostBytes = maxSizeMB * 1024 * 1024;

    // 更新 QCache 的最大容量
    smartThumbnailCache.setMaxCost(maxCostBytes);

    // 可选：清理超出部分
    smartThumbnailCache.clear();

    //qDebug() << "缩略图缓存大小设置为:" << maxSizeMB << "MB";
}

// 优化获取缓存方法
QPixmap ThumbnailWidget::getCachedThumbnail(const QString &cacheKey)
{
    // 优先使用智能缓存（自动管理内存）
    if (QPixmap* cached = smartThumbnailCache.object(cacheKey)) {
        if (!cached->isNull()) {
            return *cached;
        }
    }

    // 回退到静态缓存
    QMutexLocker locker(&cacheMutex);
    if (thumbnailCache.contains(cacheKey)) {
        QPixmap pixmap = thumbnailCache.value(cacheKey);
        if (!pixmap.isNull()) {
            // 提升到智能缓存，并正确计算成本
            int cost = calculateCostForPixmap(pixmap);
            smartThumbnailCache.insert(cacheKey, new QPixmap(pixmap), cost);
        }
        return pixmap;
    }

    return QPixmap();
}

// thumbnailwidget.cpp

void ThumbnailWidget::mousePressEvent(QMouseEvent *event)
{
    //qDebug() << "ThumbnailWidget 鼠标按下，位置:" << event->pos();

    if (event->button() == Qt::LeftButton) {
        selectThumbnailAtPosition(event->pos());
        setFocus();
    } else if (event->button() == Qt::RightButton) {
        QMouseEvent newEvent(event->type(),
                             mapToParent(event->pos()),
                             event->globalPosition(),
                             event->button(),
                             event->buttons(),
                             event->modifiers());
        QApplication::sendEvent(parentWidget(), &newEvent);
    }
}

void ThumbnailWidget::cleanupOldCache()
{
    QDir dir(m_thumbCacheDir);
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-30);

    for (const QFileInfo &fi : dir.entryInfoList(QDir::Files)) {
        if (fi.lastRead() < cutoff && fi.lastModified() < cutoff) {
            QFile::remove(fi.absoluteFilePath());
        }
    }
}
