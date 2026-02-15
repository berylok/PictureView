#include "thumbnailwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFileInfo>
#include <QApplication>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <imagewidget.h>
#include <QPainterPath>
#include <QScrollArea>
#include <QElapsedTimer>
#include <QImageReader>
#include <QCache>
#include <QTimer>
#include <QFont>

// 初始化静态成员变量
QMap<QString, QPixmap> ThumbnailWidget::thumbnailCache;
QMutex ThumbnailWidget::cacheMutex;

ThumbnailWidget::ThumbnailWidget(ImageWidget *imageWidget, QWidget *parent)
    : QWidget(parent),
    imageWidget(imageWidget),
    thumbnailSize(250, 250),
    thumbnailSpacing(17),
    selectedIndex(-1),
    loadedCount(0),
    totalCount(0),
    futureWatcher(nullptr),
    isLoading(false),
    smartThumbnailCache(perfConfig.maxCacheSize),
    currentBatchIndex(0),
    batchLoadTimer(this),
    diagnosticTimer(nullptr)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // 设置批量加载定时器
    batchLoadTimer.setSingleShot(true);
    batchLoadTimer.setInterval(perfConfig.batchLoadDelay);
    connect(&batchLoadTimer, &QTimer::timeout, this, &ThumbnailWidget::processBatchLoad);

    // 诊断定时器 - 每10秒检查一次加载状态
    diagnosticTimer = new QTimer(this);
    connect(diagnosticTimer, &QTimer::timeout, this, &ThumbnailWidget::logThumbnailStatus);
    diagnosticTimer->start(10000);
}

ThumbnailWidget::~ThumbnailWidget()
{
    stopLoading();
}

// 修改 setImageList 方法，加载所有缩略图
void ThumbnailWidget::setImageList(const QStringList &list, const QDir &dir)
{
    qDebug() << "设置缩略图列表，数量:" << list.size();

    // 停止之前的加载
    stopLoading();

    imageList = list;
    currentDir = dir;
    selectedIndex = -1;
    loadedCount = 0;
    totalCount = list.size();

    // 重置加载状态
    currentBatchIndex = 0;
    pendingLoadRequests.clear();
    allFilesToLoad.clear();

    update();

    emit loadingProgress(0, totalCount);

    // 开始加载所有缩略图
    startLoadingAllThumbnails();
}

// 开始加载所有缩略图
void ThumbnailWidget::startLoadingAllThumbnails()
{
    if (imageList.isEmpty()) return;

    qDebug() << "开始加载所有缩略图，总数:" << imageList.size();

    // 准备所有需要加载的文件
    allFilesToLoad = imageList;
    currentBatchIndex = 0;

    // 立即开始第一批加载
    processBatchLoad();
}

// 处理批量加载
void ThumbnailWidget::processBatchLoad()
{
    if (currentBatchIndex >= allFilesToLoad.size()) {
        // 所有批次都已加载完成
        qDebug() << "所有缩略图加载完成";
        isLoading = false;
        return;
    }

    isLoading = true;

    // 计算当前批次的范围
    int startIndex = currentBatchIndex;
    int endIndex = qMin(currentBatchIndex + perfConfig.batchLoadSize, allFilesToLoad.size());

    QStringList batchFiles;
    for (int i = startIndex; i < endIndex; i++) {
        batchFiles.append(allFilesToLoad[i]);
    }

    qDebug() << "加载批次:" << startIndex << "-" << (endIndex-1) << "共" << batchFiles.size() << "个文件";

    // 加载当前批次
    loadThumbnailsBatch(batchFiles);

    // 更新索引
    currentBatchIndex = endIndex;

    // 如果不是最后一批，安排下一批加载
    if (currentBatchIndex < allFilesToLoad.size()) {
        batchLoadTimer.start();
    } else {
        isLoading = false;
        qDebug() << "所有缩略图加载完成，总计:" << allFilesToLoad.size();
    }
}

// 批量加载缩略图
void ThumbnailWidget::loadThumbnailsBatch(const QStringList &fileNames)
{
    QtConcurrent::run([this, fileNames]() {
        QList<QPair<QString, QPixmap>> results;

        for (const QString &fileName : fileNames) {
            QPixmap thumbnail = loadSingleThumbnail(fileName);
            if (!thumbnail.isNull()) {
                QString cacheKey = getCacheKey(fileName);
                results.append(qMakePair(cacheKey, thumbnail));
            }
        }

        // 在主线程更新
        QMetaObject::invokeMethod(this, [this, results]() {
            for (const auto &result : results) {
                // 更新智能缓存
                smartThumbnailCache.insert(result.first, new QPixmap(result.second));

                // 同时更新静态缓存以保持兼容性
                QMutexLocker locker(&cacheMutex);
                thumbnailCache.insert(result.first, result.second);
            }

            // 更新加载计数
            loadedCount += results.size();
            emit loadingProgress(loadedCount, totalCount);

            // 更新UI
            update();

        }, Qt::QueuedConnection);
    });
}

// 加载单个缩略图
QPixmap ThumbnailWidget::loadSingleThumbnail(const QString &fileName)
{
    QString cacheKey = getCacheKey(fileName);

    //qDebug() << "加载缩略图:" << fileName << "缓存键:" << cacheKey;

    // 快速缓存检查
    if (QPixmap* cached = smartThumbnailCache.object(cacheKey)) {
        if (!cached->isNull()) {
            qDebug() << "从智能缓存获取:" << fileName;
            return *cached;
        } else {
            qDebug() << "智能缓存中的缩略图为空，重新加载:" << fileName;
            smartThumbnailCache.remove(cacheKey);
        }
    }

    // 回退到静态缓存检查
    {
        QMutexLocker locker(&cacheMutex);
        if (thumbnailCache.contains(cacheKey)) {
            QPixmap cached = thumbnailCache.value(cacheKey);
            if (!cached.isNull()) {
                qDebug() << "从静态缓存获取:" << fileName;
                return cached;
            } else {
                qDebug() << "静态缓存中的缩略图为空，移除:" << fileName;
                thumbnailCache.remove(cacheKey);
            }
        }
    }

    QPixmap result;

    try {
        // 压缩包文件处理
        if (fileName.contains("|")) {
            qDebug() << "处理压缩包文件:" << fileName;

            if (imageWidget) {
                result = imageWidget->getArchiveThumbnail(fileName);
                if (!result.isNull()) {
                    qDebug() << "成功获取压缩包缩略图:" << fileName << "尺寸:" << result.size();
                } else {
                    qDebug() << "压缩包缩略图获取失败:" << fileName;
                    result = createArchiveIcon();
                    failedThumbnails.insert(cacheKey);
                    loadingErrors.insert(cacheKey, "压缩包缩略图获取失败");
                }
            } else {
                qDebug() << "没有有效的imageWidget，使用默认图标:" << fileName;
                result = createArchiveIcon();
            }
        } else {
            // 普通文件 - 使用高效加载
            QString fullPath = currentDir.absoluteFilePath(fileName);
            //qDebug() << "加载普通文件:" << fullPath;

            if (QFile::exists(fullPath)) {
                result = loadImageFileFast(fullPath);

                if (result.isNull()) {
                    qDebug() << "普通文件加载失败:" << fileName;
                    result = createArchiveIcon(); // 使用压缩包图标作为通用错误图标
                    failedThumbnails.insert(cacheKey);
                    loadingErrors.insert(cacheKey, "图片文件加载失败");
                }
            } else {
                qDebug() << "文件不存在:" << fullPath;
                result = createArchiveIcon();
                failedThumbnails.insert(cacheKey);
                loadingErrors.insert(cacheKey, "文件不存在");
            }
        }
    } catch (const std::exception& e) {
        qDebug() << "加载缩略图时发生异常:" << e.what() << "文件:" << fileName;
        result = createArchiveIcon();
        failedThumbnails.insert(cacheKey);
        loadingErrors.insert(cacheKey, QString("异常: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "加载缩略图时发生未知异常，文件:" << fileName;
        result = createArchiveIcon();
        failedThumbnails.insert(cacheKey);
        loadingErrors.insert(cacheKey, "未知异常");
    }

    // 确保结果有效
    if (result.isNull()) {
        qDebug() << "最终结果为空，使用默认图标:" << fileName;
        result = createArchiveIcon();
    }

    return result;
}

// 高效图片加载
QPixmap ThumbnailWidget::loadImageFileFast(const QString &filePath)
{
    // 检查文件是否存在和可读
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qDebug() << "文件不存在:" << filePath;
        return QPixmap();
    }

    if (!fileInfo.isReadable()) {
        qDebug() << "文件不可读:" << filePath;
        return QPixmap();
    }

    if (fileInfo.size() == 0) {
        qDebug() << "文件大小为0:" << filePath;
        return QPixmap();
    }

    // 方法1: 使用 QImageReader（最可靠）
    QImageReader reader(filePath);

    // 检查格式支持
    if (!reader.canRead()) {
        qDebug() << "QImageReader 不支持此格式:" << filePath << "格式:" << reader.format();
        // 继续尝试其他方法
    } else {
        // 设置优化参数
        reader.setAutoTransform(true);
        reader.setQuality(50);

        QImage image;
        if (reader.read(&image)) {
            //qDebug() << "QImageReader 加载成功:" << filePath << "原始尺寸:" << image.size();

            if (image.isNull()) {
                qDebug() << "QImageReader 读取的图像为空:" << filePath;
            } else {
                // 保持宽高比进行缩放
                QImage scaled = scaleImageWithAspectRatio(image);
                return QPixmap::fromImage(scaled);
            }
        } else {
            qDebug() << "QImageReader 加载失败:" << filePath << "错误:" << reader.errorString();
        }
    }

    // 方法2: 直接使用 QImage（备用方法）
    QImage image2;
    if (image2.load(filePath)) {
        qDebug() << "QImage 直接加载成功:" << filePath << "原始尺寸:" << image2.size();

        if (image2.isNull()) {
            qDebug() << "QImage 读取的图像为空:" << filePath;
        } else {
            // 保持宽高比进行缩放
            QImage scaled = scaleImageWithAspectRatio(image2);
            return QPixmap::fromImage(scaled);
        }
    } else {
        qDebug() << "QImage 直接加载也失败:" << filePath;
    }

    // 方法3: 尝试使用 QPixmap（最后的手段）
    QPixmap pixmap;
    if (pixmap.load(filePath)) {
        qDebug() << "QPixmap 加载成功:" << filePath << "原始尺寸:" << pixmap.size();

        if (pixmap.isNull()) {
            qDebug() << "QPixmap 读取的图像为空:" << filePath;
        } else {
            // 保持宽高比进行缩放
            return scaleThumbnailWithAspectRatio(pixmap);
        }
    } else {
        qDebug() << "所有加载方法都失败:" << filePath;
    }

    return QPixmap();
}

// 保持宽高比缩放
QPixmap ThumbnailWidget::scaleThumbnailWithAspectRatio(const QPixmap &original) const
{
    if (original.isNull()) return QPixmap();

    // 保持宽高比进行缩放
    return original.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
                         tr("欢迎使用图片查看器！\n\n"
                            "使用方法：\n"
                            "• 按 Ctrl+O 打开文件夹浏览图片\n"
                            "• 按 Ctrl+Shift+O 打开单张图片\n"
                            "• 按 F1 查看详细使用说明\n\n"
                            "祝您使用愉快！"
                            "\n\n"
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
            thumbnail = createArchiveIcon();
        }

        drawThumbnailItem(painter, i, currentX, currentY, fileName, thumbnail, isTopLevelArchive);
    }

    // 显示加载状态
    if (isLoading) {
        painter.setPen(QColor(200, 200, 200));
        painter.drawText(10, 20, QString("加载中: %1/%2").arg(loadedCount).arg(totalCount));
    }

    updateMinimumHeight();
}

QPixmap ThumbnailWidget::getCachedThumbnail(const QString &cacheKey)
{
    // 优先使用智能缓存
    if (QPixmap* cached = smartThumbnailCache.object(cacheKey)) {
        return *cached;
    }

    // 回退到静态缓存
    QMutexLocker locker(&cacheMutex);
    if (thumbnailCache.contains(cacheKey)) {
        return thumbnailCache.value(cacheKey);
    }

    return QPixmap();
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
        painter.fillPath(path, QColor(0, 120, 215, 200));
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
        QPixmap archiveIcon = createArchiveIcon();
        painter.drawPixmap(borderRect, archiveIcon);
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
QPixmap ThumbnailWidget::createArchiveIcon() const
{
    QPixmap icon(thumbnailSize);
    icon.fill(QColor(70, 130, 180, 200));

    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);

    // 简化的图标绘制
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(QColor(100, 160, 210, 150));

    QRectF rect(icon.width() * 0.2, icon.height() * 0.3,
                icon.width() * 0.6, icon.height() * 0.4);
    painter.drawRoundedRect(rect, 5, 5);

    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(icon.rect(), Qt::AlignCenter, "ZIP");

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
    qDebug() << "=== 缩略图加载问题诊断 ===";
    qDebug() << "总图片数量:" << imageList.size();
    qDebug() << "智能缓存数量:" << smartThumbnailCache.size();
    qDebug() << "静态缓存数量:" << thumbnailCache.size();
    qDebug() << "已加载数量:" << loadedCount;
    qDebug() << "失败缩略图:" << failedThumbnails.size();

    // 检查每个文件的状态
    for (int i = 0; i < imageList.size(); ++i) {
        QString fileName = imageList.at(i);
        QString cacheKey = getCacheKey(fileName);

        bool inSmartCache = smartThumbnailCache.contains(cacheKey);
        bool inStaticCache = thumbnailCache.contains(cacheKey);
        bool isFailed = failedThumbnails.contains(cacheKey);

        if (!inSmartCache && !inStaticCache && !isFailed) {
            qDebug() << "未加载的文件:" << fileName;
            qDebug() << "  - 索引:" << i;
            qDebug() << "  - 缓存键:" << cacheKey;
            qDebug() << "  - 是否压缩包:" << (fileName.contains("|") || isArchiveFile(fileName));
        }
    }

    qDebug() << "=== 诊断结束 ===";
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

    qDebug() << "缩略图状态 - 已加载:" << loaded << "/" << total
             << "失败:" << failed;

    // 如果有很多失败的，尝试重新加载
    if (failed > total * 0.3) { // 超过30%失败
        qDebug() << "检测到大量失败，尝试重新加载...";
        forceReloadAll();
    }
}

void ThumbnailWidget::forceReloadAll()
{
    qDebug() << "强制重新加载所有缩略图";

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

    // 将失败的缩略图重新加入加载队列
    for (const QString &cacheKey : failedThumbnails) {
        // 从缓存键解析文件名
        QString fileName;
        if (cacheKey.contains("|")) {
            fileName = cacheKey;
        } else {
            QFileInfo fileInfo(cacheKey);
            fileName = fileInfo.fileName();
        }

        // 重新加载这个文件
        QtConcurrent::run([this, fileName]() {
            QPixmap thumbnail = loadSingleThumbnail(fileName);

            QMetaObject::invokeMethod(this, [this, fileName, thumbnail]() {
                QString cacheKey = getCacheKey(fileName);
                if (!thumbnail.isNull()) {
                    smartThumbnailCache.insert(cacheKey, new QPixmap(thumbnail));
                    QMutexLocker locker(&cacheMutex);
                    thumbnailCache.insert(cacheKey, thumbnail);
                    failedThumbnails.remove(cacheKey);
                    loadingErrors.remove(cacheKey);
                    loadedCount++;
                    update();
                }
            }, Qt::QueuedConnection);
        });
    }

    update();
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
        qDebug() << "ThumbnailWidget 选中索引:" << index;
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

void ThumbnailWidget::setCacheSize(int maxSizeMB)
{
    perfConfig.maxCacheSize = maxSizeMB * 1024 * 1024;
    smartThumbnailCache.setMaxCost(perfConfig.maxCacheSize);
}

// 鼠标和键盘事件处理保持不变...
void ThumbnailWidget::mousePressEvent(QMouseEvent *event)
{
    qDebug() << "ThumbnailWidget 鼠标按下，位置:" << event->pos();

    if (event->button() == Qt::LeftButton) {
        selectThumbnailAtPosition(event->pos());
        setFocus();
    } else if (event->button() == Qt::RightButton) {
        QMouseEvent newEvent(event->type(),
                             mapToParent(event->pos()),
                             event->globalPos(),
                             event->button(),
                             event->buttons(),
                             event->modifiers());
        QApplication::sendEvent(parentWidget(), &newEvent);
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
    qDebug() << "ThumbnailWidget 接收到按键:" << event->key();

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
        qDebug() << "处理回车键，选中索引:" << selectedIndex;
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
