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

// // 初始化静态成员变量
// QMap<QString, QPixmap> ThumbnailWidget::thumbnailCache;
// QMutex ThumbnailWidget::cacheMutex;

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
    // ✅ 使用 maxCacheMemoryMB 并转换为字节
    smartThumbnailCache(perfConfig.maxCacheMemoryMB * 1024 * 1024),
    currentBatchIndex(0),
    batchLoadTimer(this),
    diagnosticTimer(nullptr)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // 可选：设置缓存清理策略
    smartThumbnailCache.setMaxCost(perfConfig.maxCacheMemoryMB * 1024 * 1024);

    // 设置批量加载定时器
    batchLoadTimer.setSingleShot(true);
    batchLoadTimer.setInterval(perfConfig.batchLoadDelay);
    connect(&batchLoadTimer, &QTimer::timeout, this, &ThumbnailWidget::processBatchLoad);

    // 诊断定时器 - 每5秒检查一次加载状态
    diagnosticTimer = new QTimer(this);
    connect(diagnosticTimer, &QTimer::timeout, this, &ThumbnailWidget::logThumbnailStatus);
    diagnosticTimer->start(1000);
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

    logCacheStats();  // 查看加载后的缓存状态
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
        finishLoading();  // 所有批次完成，重置状态
        return;
    }

    isLoading = true;  // 仅在真正开始加载时设置

    int startIndex = currentBatchIndex;
    int endIndex = qMin(currentBatchIndex + perfConfig.batchLoadSize, allFilesToLoad.size());

    QStringList batchFiles;
    for (int i = startIndex; i < endIndex; ++i) {
        batchFiles.append(allFilesToLoad[i]);
    }

    loadThumbnailsBatch(batchFiles);
    currentBatchIndex = endIndex;

    // 安排下一批次（如果未完成）
    if (currentBatchIndex < allFilesToLoad.size()) {
        batchLoadTimer.start();
    } else {
        // 注意：这里不能直接设 isLoading = false，因为批次是异步的
        // 需要在所有异步加载完成的回调中处理
    }
}
// 批量加载缩略图
void ThumbnailWidget::loadThumbnailsBatch(const QStringList &fileNames)
{
    if (fileNames.isEmpty()) return;

    // 在工作线程中只做文件 I/O 和解码
    QtConcurrent::run([this, fileNames]() {
        QList<QPair<QString, QPixmap>> loadedResults;

        for (const QString &fileName : fileNames) {
            QString cacheKey = getCacheKey(fileName);

            // 线程安全：只检查只读的静态缓存？不行，静态缓存也可能被主线程修改
            // 因此工作线程完全不触碰任何缓存，只负责加载原始图片
            QPixmap pixmap = loadSingleThumbnail(fileName);
            if (!pixmap.isNull()) {
                loadedResults.append(qMakePair(fileName, pixmap));
            }
        }

        // 将结果传回主线程
        if (!loadedResults.isEmpty()) {
            QMetaObject::invokeMethod(this, [this, loadedResults]() {
                // 主线程安全地更新缓存
                for (const auto &pair : loadedResults) {
                    QString cacheKey = getCacheKey(pair.first);
                    QPixmap thumbnail = pair.second;

                    int cost = calculateCostForPixmap(thumbnail);
                    smartThumbnailCache.insert(cacheKey, new QPixmap(thumbnail), cost);
                    {
                    }
                    loadedCount++;
                }

                emit loadingProgress(loadedCount, totalCount);
                update(visibleRegion().boundingRect());

                // 检查是否所有批次都已完成
                if (currentBatchIndex >= allFilesToLoad.size() && loadedCount >= totalCount) {
                    finishLoading();
                }
            }, Qt::QueuedConnection);
        } else {
            // 即使没有加载成功，也要更新计数并检查完成状态
            QMetaObject::invokeMethod(this, [this]() {
                // 这里实际上 loadedCount 没变，但我们可以通过批次索引判断
                if (currentBatchIndex >= allFilesToLoad.size() && loadedCount >= totalCount) {
                    finishLoading();
                }
            }, Qt::QueuedConnection);
        }
    });
}

// 新增完成处理函数
void ThumbnailWidget::finishLoading()
{
    isLoading = false;
    qDebug() << "所有缩略图批次加载完成";
    emit loadingProgress(loadedCount, totalCount);
    update();
}

// 加载单个缩略图
QPixmap ThumbnailWidget::loadSingleThumbnail(const QString &fileName)
{
    QString cacheKey = getCacheKey(fileName);

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

            // 普通文件 - 先检查是否是压缩包，若是则直接返回默认图标
            if (isArchiveFile(fileName)) {
                qDebug() << "检测到压缩包文件，使用默认图标:" << fileName;
                result = createArchiveIcon();
            } else {
                QString fullPath = currentDir.absoluteFilePath(fileName);
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
    QImageReader reader(filePath);
    if (!reader.canRead()) {
        return QPixmap();
    }

    // ✅ 关键优化：设置 QImageReader 的目标尺寸
    QSize originalSize = reader.size();
    if (originalSize.isValid()) {
        // 计算缩放比例，确保解码时直接缩放到接近缩略图大小
        QSize scaledSize = originalSize.scaled(thumbnailSize, Qt::KeepAspectRatio);
        reader.setScaledSize(scaledSize);
    }

    reader.setAutoTransform(true);
    reader.setQuality(50); // 降低质量以加快速度

    QImage image = reader.read();
    if (image.isNull()) return QPixmap();

    // 此时得到的 image 已经是接近缩略图尺寸，直接转换即可
    return QPixmap::fromImage(image);
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
            thumbnail = createArchiveIcon();
        }

        drawThumbnailItem(painter, i, currentX, currentY, fileName, thumbnail, isTopLevelArchive);
    }

    // 显示加载状态
    if (isLoading) {
        painter.setPen(QColor(200, 200, 200));
        painter.drawText(10, 20, QString("Loading: %1/%2").arg(loadedCount).arg(totalCount));
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

    qDebug() << "已加载数量:" << loadedCount;
    qDebug() << "失败缩略图:" << failedThumbnails.size();

    // 检查每个文件的状态
    for (int i = 0; i < imageList.size(); ++i) {
        QString fileName = imageList.at(i);
        QString cacheKey = getCacheKey(fileName);

        bool inSmartCache = smartThumbnailCache.contains(cacheKey);

        bool isFailed = failedThumbnails.contains(cacheKey);


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

        if (smartThumbnailCache.contains(cacheKey) ) {
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
                    // QMutexLocker locker(&cacheMutex);
                    // thumbnailCache.insert(cacheKey, thumbnail);
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
    smartThumbnailCache.clear();
}

void ThumbnailWidget::clearThumbnailCacheForImage(const QString &imagePath)
{

}

void ThumbnailWidget::setThumbnailSize(const QSize &size)
{
    if (thumbnailSize != size) {
        thumbnailSize = size;
        // 尺寸变化时清空缓存
        smartThumbnailCache.clear();
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
    qDebug() << "========================================";
    qDebug() << "=== 缩略图缓存统计 ===";
    qDebug() << "智能缓存条目数:" << smartThumbnailCache.size();
    qDebug() << "智能缓存总成本:" << smartThumbnailCache.totalCost() / (1024.0 * 1024.0) << "MB";
    qDebug() << "智能缓存最大容量:" << smartThumbnailCache.maxCost() / (1024.0 * 1024.0) << "MB";
    qDebug() << "智能缓存使用率:" << QString::number(smartThumbnailCache.totalCost() * 100.0 /
                                                         smartThumbnailCache.maxCost(), 'f', 1) << "%";

    // 加载统计
    qDebug() << "已加载数量:" << loadedCount << "/" << totalCount;
    qDebug() << "加载进度:" << QString::number(loadedCount * 100.0 / totalCount, 'f', 1) << "%";

    // 失败统计
    qDebug() << "失败缩略图数量:" << failedThumbnails.size();
    if (!failedThumbnails.isEmpty() && failedThumbnails.size() <= 10) {
        qDebug() << "失败列表:";
        for (const QString& key : failedThumbnails) {
            qDebug() << "  -" << key;
        }
    }

    // 性能配置
    qDebug() << "=== 性能配置 ===";
    qDebug() << "最大缓存内存:" << perfConfig.maxCacheMemoryMB << "MB";
    qDebug() << "批量加载大小:" << perfConfig.batchLoadSize;
    qDebug() << "批量加载延迟:" << perfConfig.batchLoadDelay << "ms";
    qDebug() << "懒加载模式:" << (perfConfig.enableLazyLoading ? "启用" : "禁用");
    qDebug() << "加载状态:" << (isLoading ? "加载中" : "空闲");
    qDebug() << "========================================";
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

    qDebug() << "缩略图缓存大小设置为:" << maxSizeMB << "MB";
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
    return QPixmap();
}

// thumbnailwidget.cpp

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

