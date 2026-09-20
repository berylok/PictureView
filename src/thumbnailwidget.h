

// thumbnailwidget.h
#ifndef THUMBNAILWIDGET_H
#define THUMBNAILWIDGET_H

#include "qfuturewatcher.h"
#include <QWidget>
#include <QPixmap>
#include <QDir>
#include <QStringList>
#include <QMap>
#include <QMutex>
#include <QCache>
#include <QTimer>
#include <QSet>
#include <QThreadPool>
#include <atomic>


class ImageWidget;  // 前向声明

class ThumbnailWidget : public QWidget
{
    Q_OBJECT

public:
    ThumbnailWidget(ImageWidget *imageWidget = nullptr, QWidget *parent = nullptr);
    ~ThumbnailWidget();

    void setImageList(const QStringList &list, const QDir &dir);
    void setSelectedIndex(int index);
    int getSelectedIndex() const;
    void ensureVisible(int index);
    void clearThumbnailCache();
    static void clearThumbnailCacheForImage(const QString &imagePath);


    // 性能优化方法
    void setThumbnailSize(const QSize &size);
    void setCacheSize(int maxSizeMB);

    // 诊断方法


signals:
    void thumbnailClicked(int index);
    void ensureRectVisible(const QRect &rect);
    void loadingProgress(int loaded, int total);
    void thumbnailStatusReport(const QString &report);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:


private:
    // 核心方法

    QPixmap loadThumbnail(const QString &path);
    void updateThumbnails();


    // 性能优化方法




    // 缓存管理
    void cleanupOldCache();

    // 基础成员
    ImageWidget *imageWidget;
    QSize thumbnailSize;
    int thumbnailSpacing;
    QStringList imageList;
    QDir currentDir;
    int selectedIndex;

    // 加载相关
    int loadedCount;
    int totalCount;
    QFutureWatcher<QPixmap> *futureWatcher;
    bool isLoading;

    // 静态缓存 - 保持向后兼容
    static QMap<QString, QPixmap> thumbnailCache;
    static QMutex cacheMutex;

    // === 性能优化成员 ===

    // 智能缓存系统
    QCache<QString, QPixmap> smartThumbnailCache;

    // 批量加载系统
    QTimer batchLoadTimer;
    QSet<QString> pendingLoadRequests;
    QStringList allFilesToLoad;
    int currentBatchIndex;

    // 性能配置
    // thumbnailwidget.h

    struct PerformanceConfig {
        int maxCacheMemoryMB = 100;           // 最大缓存内存 100MB (改为MB单位)
        int batchLoadSize = 1;         // ★ 1 → 12（每批 12 张）
        int batchLoadDelay = 20;        // ★ 150 → 30ms
        int preloadRange = 1;                 // 预加载前后1个
        bool enableLazyLoading = true;        // 启用懒加载
        bool enablePriorityLoading = true;    // 启用优先级加载
    };
    PerformanceConfig perfConfig;

    // 诊断相关成员
    QSet<QString> failedThumbnails;
    QMap<QString, QString> loadingErrors;
    QTimer *diagnosticTimer;



private:
    // 缩略图加载相关
    QImage loadSingleThumbnail(const QString &fileName);
    QImage loadImageFileFast(const QString &filePath);
    QImage createArchiveIconImage() const;
    QImage scaleImageWithAspectRatio(const QImage &original) const;

    // 缓存相关
    QPixmap getCachedThumbnail(const QString &cacheKey);
    int calculateCostForPixmap(const QPixmap &pixmap) const;
    void logCacheStats();
    void logThumbnailStatus();
    void diagnoseLoadingIssues();
    void forceReloadAll();
    void retryFailedThumbnails();
    void finishLoading();

    // 布局 / 绘制相关
    void updateMinimumHeight();
    int calculateItemsPerRow() const;
    void selectThumbnailAtPosition(const QPoint &pos);
    void drawThumbnailItem(QPainter &painter, int index,
                           int x, int y, const QString &fileName,
                           const QPixmap &thumbnail, bool isArchive);
    void processBatchLoad();
    void loadThumbnailsBatch(const QStringList &fileNames);
    void startLoadingAllThumbnails();
    void stopLoading();
    QString getCacheKey(const QString &fileName) const;
    QString getDisplayName(const QString &fileName) const;
    bool isArchiveFile(const QString &fileName) const;


private:
    QThreadPool *m_pool = nullptr;   // 缩略图专用线程池
    std::atomic<int> m_generation{0};

private:
    QString m_thumbCacheDir;

};

#endif // THUMBNAILWIDGET_H
