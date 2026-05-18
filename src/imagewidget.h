

#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QDir>
#include <QTimer>
#include <QMap>
#include <QAction>
#include <QtConcurrent>
#include <QMutex>
#include <QScrollArea>
#include <QImage>

#include "thumbnailwidget.h"
#include "configmanager.h"
#include "canvascontrolpanel.h"
#include "archivehandler.h"
#include "canvasoverlay.h"

#include <QOpenGLWidget>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>

class ImageWidget : public QWidget
{
    Q_OBJECT

public:
    enum ViewMode { SingleView, ThumbnailView };

    ImageWidget(QWidget *parent = nullptr);
    ~ImageWidget();

    // ==================== 图片加载 ====================
    bool loadImage(const QString &filePath, bool fromCache = false);
    bool loadImageByIndex(int index, bool fromCache = true);
    void loadImageList();
    void loadNextImage();
    void loadPreviousImage();
    void preloadAllImages();
    void clearImageCache();

    // ==================== 幻灯片 ====================
    void startSlideshow();
    void stopSlideshow();
    void toggleSlideshow();
    void setSlideshowInterval(int interval);
    void slideshowNext();

    // ==================== 视图切换 ====================
    void switchToSingleView(int index = -1);
    void switchToThumbnailView();
    void setCurrentDir(const QDir &dir);
    void clearThumbnailCache() { thumbnailWidget->clearThumbnailCache(); }

    // ==================== 缩放 / 适配 ====================
    void fitToWindow();
    void actualSize();

    // ==================== 配置管理 ====================
    void loadConfiguration();
    void saveConfiguration();
    void applyConfiguration(const ConfigManager::Config &config);
    int getLastViewMode() const { return currentConfig.lastViewMode; }
    int getLastImageIndex() const { return currentConfig.lastImageIndex; }

    // ==================== 变换操作 ====================
    void setHorizontalFlip(bool enable);
    void setVerticalFlip(bool enable);
    void applyTransform();
    bool isTransformLocked() const { return transformLocked; }

    // ==================== 压缩包 ====================
    QPixmap getArchiveThumbnail(const QString &archivePath);

    // ==================== 工具方法 ====================
    void updateWindowTitle();
    QString getShortPathName(const QString &longPath);
    void logMessage(const QString &message);
    void registerFileAssociation(const QString &fileExtension,
                                 const QString &fileTypeName,
                                 const QString &openCommand);
    void openFolder();

    ConfigManager::Config currentConfig;
    ConfigManager *configManager;

public slots:
    // ==================== 图片操作 ====================
    void deleteCurrentImage();
    void deleteSelectedThumbnail();
    void performDeleteCurrentImage();

    // ==================== 压缩包 ====================
    void exitArchiveMode();

    // ==================== 画布 / 变换 ====================
    void toggleTransformLock();
    void toggleImmersiveMode();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    // ==================== 缩略图 ====================
    void onThumbnailClicked(int index);
    void onEnsureRectVisible(const QRect &rect);

    // ==================== 画布控制面板 ====================
    void onExitCanvasMode();
    void showShortcutHelp();

    // ==================== 菜单操作 ====================
    void openImageInNewWindow();
    void restartApplication();

private:
    // ==================== 枚举 ====================
    enum ViewStateType {
        ManualAdjustment,
        FitToWindow,
        ActualSize
    };

    // ==================== 图片数据 ====================
    QPixmap pixmap;
    QPixmap originalPixmap;
    QPixmap transformedPixmap;
    QImage currentImage;
    QString currentImagePath;
    QStringList imageList;
    int currentImageIndex;
    double scaleFactor;
    QPointF panOffset;

    // ==================== 变换状态 ====================
    int rotationAngle;
    bool isHorizontallyFlipped;
    bool isVerticallyFlipped;
    bool transformLocked;
    ViewStateType currentViewStateType;

    // ==================== 缓存 ====================
    QMap<QString, QPixmap> imageCache;
    QMutex cacheMutex;

    // ==================== 幻灯片 ====================
    bool isSlideshowActive;
    int slideshowInterval;
    QTimer *slideshowTimer;

    // ==================== 视图模式 ====================
    ViewMode currentViewMode;
    QSize thumbnailSize;
    int thumbnailSpacing;
    QScrollArea *scrollArea;
    ThumbnailWidget *thumbnailWidget;
    QDir currentDir;

    // ==================== 拖拽 / 平移 ====================
    bool isDraggingWindow;
    QPoint dragStartPosition;
    bool isPanningImage;
    QPointF panStartPosition;

    // ==================== 导航提示 ====================
    bool mouseInLeftQuarter;
    bool mouseInRightQuarter;
    bool mouseInImage;
    bool showNavigationHints;
    QTimer *hideHintsTimer;
    QPoint lastMousePos;
    void updateNavigationHintsVisibility(const QPoint& mousePos);
    bool isMouseInImageArea(const QPoint& mousePos) const;
    void startHideHintsTimer();
    void stopHideHintsTimer();

    // ==================== 窗口状态 ====================
    bool wasMaximizedBeforeCanvas;
    QRect normalGeometryBeforeCanvas;
    Qt::WindowFlags windowFlagsBeforeCanvas;
    double m_windowOpacity;
    bool m_transparentBackgroundReady;
    bool m_immersiveUseTransparent;

    bool isAlwaysOnTop() const;
    bool hasTitleBar() const;
    bool hasTransparentBackground() const;
    void setWindowOpacityValue(double opacity);
    void restoreNormalWindowState();

    // ==================== 画布模式 ====================
    bool canvasMode;
    CanvasControlPanel *controlPanel;
    CanvasOverlay* canvasOverlay = nullptr;

    void toggleCanvasMode();
    void enableCanvasMode();
    void disableCanvasMode();
    bool isCanvasModeEnabled();
    void createControlPanel();
    void destroyControlPanel();
    void positionControlPanel();

    // ==================== 鼠标穿透 ====================
    bool mousePassthrough;
    void enableMousePassthrough();
    void disableMousePassthrough();
    void ensureWindowVisible();
    void ensureFocus();
    void updateMousePassthroughRegion();
    bool shouldPassthroughMouse(const QPoint& pos) const;
    bool isInImageArea(const QPoint& pos) const;
    void updateCanvasModePassthrough();
    void clearAllPassthrough();

    // ==================== 窗口形状 / 掩码 ====================
    QRect getImageDisplayRect() const;
    void cleanImageEdges();
    void updateMask();
    bool m_maskDirty = false;
    void setMask(const QRegion &region);
    void clearMask();
    void clearX11Shape();
    void setX11ShapeRect(const QRect &rect);
    void setX11Shape(const QRegion &region);
    void forceX11ShapeRefresh();

    // ==================== 压缩包 ====================
    ArchiveHandler archiveHandler;
    bool isArchiveMode;
    QString currentArchivePath;
    QMap<QString, QPixmap> archiveImageCache;
    QDir previousDir;
    QStringList previousImageList;
    int previousImageIndex;
    ViewMode previousViewMode;

    bool openArchive(const QString &filePath);
    void closeArchive();
    void loadArchiveImageList();
    bool loadImageFromArchive(const QString &filePath);
    bool isArchiveFile(const QString &fileName) const;
    QPixmap createDefaultArchiveThumbnail();

    // ==================== 菜单 / 操作 ====================
    void navigateThumbnails(int key);
    void toggleTitleBar();
    void toggleAlwaysOnTop();
    void toggleTransparentBackground();
    void copyImageToClipboard();
    void pasteImageFromClipboard();
    void saveImage();
    void openImage();
    void openSelectedImage();
    void showContextMenu(const QPoint &globalPos);
    void mirrorHorizontal();
    void mirrorVertical();
    void rotate90CW();
    void rotate90CCW();
    void rotate180();
    void resetTransform();
    void applyTransformations();
    bool isTransformed() const;
    bool moveFileToRecycleBin(const QString &filePath);
    bool shouldShowNavigationArrows(const QSize &scaledSize);
    void showAboutDialog();
    void testKeyboard();

    // ==================== 菜单动作 ====================
    QAction *toggleTitleBarAction;
    QAction *toggleAlwaysOnTopAction;
    QAction *toggleTransparentBackgroundAction;
    QAction *openFolderAction;
    QAction *openImageAction;
    QAction *saveImageAction;
    QAction *copyImageAction;
    QAction *pasteImageAction;
    QAction *aboutAction;
    QAction *openInNewWindowAction;

    void createShortcutActions();

    // ==================== 图标 ====================
    QIcon createMultiResolutionIcon();
    void createFallbackIcon();

    // ==================== 定时器 ====================
    QTimer m_wheelTimer;

    // OpenGL 加速相关
    bool m_useOpenGL = false;
    QOpenGLWidget* m_glWidget = nullptr;
    QOpenGLTexture* m_glTexture = nullptr;
    QOpenGLShaderProgram* m_shaderProgram = nullptr;
    QOpenGLBuffer* m_vertexBuffer = nullptr;
    QMatrix4x4 m_transformMatrix;

    // OpenGL 初始化函数
    void initOpenGL();
    void updateGLTexture();
    void updateTransformMatrix();
    void renderWithOpenGL(QPainter* painter);

public:
    // 添加公共接口
    void enableOpenGLAcceleration(bool enable = true);

    // 在 imagewidget.h 的 private 部分添加
private:
    // ==================== 异步加载 ====================
    void loadImageAsync(const QString &filePath, std::function<void(bool)> callback);
    void preloadImagesAround(int centerIndex, int count = 2);
};

#endif // IMAGEWIDGET_H
