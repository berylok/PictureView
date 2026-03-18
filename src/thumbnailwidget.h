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
    void stopLoading();

    // 性能优化方法
    void setThumbnailSize(const QSize &size);
    void setCacheSize(int maxSizeMB);

    // 诊断方法
    void diagnoseLoadingIssues();
    void logThumbnailStatus();
    void retryFailedThumbnails();
    void forceReloadAll();

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
    void processBatchLoad();

private:
    // 核心方法
    bool isArchiveFile(const QString &fileName) const;
    QPixmap createArchiveIcon() const;
    QPixmap loadThumbnail(const QString &path);
    void updateThumbnails();
    void selectThumbnailAtPosition(const QPoint &pos);

    // 性能优化方法
    void startLoadingAllThumbnails();
    void loadThumbnailsBatch(const QStringList &fileNames);
    QPixmap loadSingleThumbnail(const QString &fileName);
    QPixmap loadImageFileFast(const QString &filePath);
    int calculateItemsPerRow() const;
    void drawThumbnailItem(QPainter &painter, int index, int x, int y,
                           const QString &fileName, const QPixmap &thumbnail, bool isArchive);
    QString getCacheKey(const QString &fileName) const;
    QString getDisplayName(const QString &fileName) const;
    void updateMinimumHeight();

    // 缓存管理
    void cleanupOldCache();
    QPixmap getCachedThumbnail(const QString &cacheKey);
    QPixmap scaleThumbnailWithAspectRatio(const QPixmap &original) const;
    QImage scaleImageWithAspectRatio(const QImage &original) const;

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
    struct PerformanceConfig {
        int maxCacheSize = 200 * 1024 * 1024; // 200MB
        int batchLoadSize = 10;  // 每次批量加载10个
        int batchLoadDelay = 50; // 批次间延迟50ms
        bool enableMemoryOptimization = true;
    };
    PerformanceConfig perfConfig;

    // 诊断相关成员
    QSet<QString> failedThumbnails;
    QMap<QString, QString> loadingErrors;
    QTimer *diagnosticTimer;
};

#endif // THUMBNAILWIDGET_H
