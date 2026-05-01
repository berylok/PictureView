#include "imagewidget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QPainterPath>
#include <QFont>
#include <QScreen>
#include <QGuiApplication>
#include <QGuiApplication>

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#endif

#include <execinfo.h>
#include <cxxabi.h>

void ImageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    // 1. 背景填充
    if (testAttribute(Qt::WA_TranslucentBackground))
        painter.fillRect(rect(), QColor(0, 0, 0, 0));
    else
        painter.fillRect(rect(), Qt::black);

    // 2. 空图 / 无效缩放保护
    if (pixmap.isNull()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("没有图片或图片加载失败"));
        return;
    }
    if (scaleFactor <= 0) scaleFactor = 1.0;

    // 3. 计算图像在窗口中的缩放后尺寸和偏移
    QSizeF scaledSize = QSizeF(pixmap.size()) * scaleFactor;
    QPointF offset((width()  - scaledSize.width())  / 2.0 + panOffset.x(),
                   (height() - scaledSize.height()) / 2.0 + panOffset.y());
    QRectF imageCanvasRect(offset, scaledSize);

    // 4. ★ 关键判断：缩放后图片是否大于窗口
    bool largerThanWindow = (scaledSize.width()  > width() ||
                             scaledSize.height() > height());

    if (largerThanWindow) {
        // ---------- 按需渲染：只处理窗口内的可见部分 ----------
        QRectF visibleRectF = imageCanvasRect.intersected(QRectF(rect()));
        if (visibleRectF.isEmpty()) return;

        // 映射回原图子矩形
        QPointF srcTopLeft = (visibleRectF.topLeft() - offset) / scaleFactor;
        QSizeF  srcSize    = visibleRectF.size() / scaleFactor;
        QRect sourceRect(srcTopLeft.toPoint(), srcSize.toSize());

        // 缩放可见块
        QSize targetSize = visibleRectF.size().toSize();
        QPixmap piece = pixmap.copy(sourceRect)
                            .scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(visibleRectF.topLeft(), piece);

    } else {
        // ---------- 图片完全在窗口内：直接缩放全图 ----------
        QPixmap scaledPixmap = pixmap.scaled(scaledSize.toSize(),
                                             Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation);
        painter.drawPixmap(offset, scaledPixmap);
    }

    // 5. 变换状态提示（保持不变）
    if (isTransformed()) {
        painter.setPen(Qt::yellow);
        painter.setFont(QFont("Arial", 10));
        QStringList info;
        if (rotationAngle != 0)
            info << QString("旋转: %1°").arg(rotationAngle);
        if (isHorizontallyFlipped) info << "水平镜像";
        if (isVerticallyFlipped)   info << "垂直镜像";
        if (!info.isEmpty())
            painter.drawText(10, 30, info.join(" "));
    }
}


bool ImageWidget::shouldShowNavigationArrows(const QSize &scaledSize)
{
    return scaledSize.width() > 600 && scaledSize.height() > 600;
}

void ImageWidget::fitToWindow()
{
    if (pixmap.isNull()) return;

    QSize windowSize = this->size();
    if (windowSize.isEmpty() || windowSize.width() <= 0 || windowSize.height() <= 0) {
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect desktopRect = screen->availableGeometry();
        windowSize = desktopRect.size();
    }

    QSize imageSize = pixmap.size();
    double widthRatio = static_cast<double>(windowSize.width()) / imageSize.width();
    double heightRatio = static_cast<double>(windowSize.height()) / imageSize.height();

    scaleFactor = qMin(widthRatio, heightRatio);
    panOffset = QPointF(0, 0);
    currentViewStateType = FitToWindow; // 设置为合适大小模式
    updateMask(); //掩码更新
    update();

}

void ImageWidget::actualSize()
{
    if (pixmap.isNull()) return;

    // 获取当前鼠标位置
    QPointF mousePos = mapFromGlobal(QCursor::pos());

    // 获取当前视图中心
    QPointF viewCenter(width() / 2.0, height() / 2.0);

    // 计算鼠标相对于视图中心的位置
    QPointF relativeMousePos = mousePos - viewCenter;

    // 如果当前不是实际大小，保存当前的偏移量
    static QPointF lastPanOffset = panOffset;
    static double lastScaleFactor = scaleFactor;

    if (scaleFactor != 1.0) {
        lastPanOffset = panOffset;
        lastScaleFactor = scaleFactor;

        // 计算以鼠标为中心切换到实际大小时的偏移
        // 公式：新偏移 = 鼠标位置 - 视图中心 - (鼠标位置 - 视图中心 - 旧偏移)/旧缩放因子
        QPointF imagePos = (mousePos - viewCenter - panOffset) / lastScaleFactor;
        panOffset = mousePos - viewCenter - imagePos * 1.0;
    } else {
        // 如果当前已经是实际大小，重置偏移
        panOffset = QPointF(0, 0);
    }

    scaleFactor = 1.0;
    currentViewStateType = ActualSize; // 设置为实际大小模式
    updateMask(); //掩码更新
    update();

}

void ImageWidget::wheelEvent(QWheelEvent *event)
{
    if (currentViewMode != SingleView) return;

    currentViewStateType = ManualAdjustment;
    double zoomFactor = 1.15;
    double oldScale = scaleFactor;

    if (event->angleDelta().y() > 0)
        scaleFactor *= zoomFactor;
    else
        scaleFactor /= zoomFactor;
    scaleFactor = qBound(0.03, scaleFactor, 8.0);

    QPointF mousePos = event->position();
    QPointF viewCenter(width()/2.0, height()/2.0);
    QPointF imagePos = (mousePos - viewCenter - panOffset) / oldScale;
    panOffset = mousePos - viewCenter - imagePos * scaleFactor;

    // 只重绘，不立即更新掩码
    m_maskDirty = true;   // 标记掩码需要更新
    update();
}

void ImageWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (currentViewMode == ThumbnailView) {
        thumbnailWidget->update();
        // ====== ADD THIS LINE ======
        clearMask(); // 强制清除掩码，防止单图模式残留
        // ==========================
    } else if (currentViewMode == SingleView) {
        if (currentViewStateType == FitToWindow && !pixmap.isNull()) {
            fitToWindow();
        }
        if (testAttribute(Qt::WA_TranslucentBackground) && !pixmap.isNull()) {
            updateMask();
        }
    }
}





void ImageWidget::updateMask()
{
    m_maskDirty = false;

    // 非单图 / 无图 / 非透明背景 → 清除所有形状
    if (currentViewMode != SingleView || pixmap.isNull() ||
        !testAttribute(Qt::WA_TranslucentBackground)) {
        clearX11Shape();
        clearMask();
        return;
    }

    // 1. 计算虚拟画布和偏移（与 paintEvent 完全相同）
    QSizeF scaledSize = QSizeF(pixmap.size()) * scaleFactor;
    QPointF offset((width()  - scaledSize.width())  / 2.0 + panOffset.x(),
                   (height() - scaledSize.height()) / 2.0 + panOffset.y());
    QRectF imageCanvasRect(offset, scaledSize);

    // 2. 只取窗口内可见区域
    QRectF visibleRectF = imageCanvasRect.intersected(QRectF(rect()));
    if (visibleRectF.isEmpty()) {
        clearX11Shape();
        clearMask();
        return;
    }
    QRect visibleRect = visibleRectF.toRect();

    // 3. 映射回原图区域（关键：只拷贝这一小块）
    QPointF srcTopLeft = (visibleRectF.topLeft() - offset) / scaleFactor;
    QSizeF  srcSize    = visibleRectF.size() / scaleFactor;
    QRect sourceRect(srcTopLeft.toPoint(), srcSize.toSize());

    QPixmap piece = pixmap.copy(sourceRect);
    QPixmap scaledPiece = piece.scaled(visibleRect.size(),
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);

    // 4. 无透明通道 → 直接用矩形（极速）
    if (!pixmap.hasAlphaChannel()) {
        setX11ShapeRect(visibleRect);
        return;
    }

    // 5. 有透明通道：缩小到 200x200 以内做逐像素检测（进一步减轻 CPU）
    const int MAX_MASK_SIZE = 200;   // 减小尺寸，加快速度
    QPixmap smallMask = scaledPiece;
    double maskScale = 1.0;
    if (smallMask.width() > MAX_MASK_SIZE || smallMask.height() > MAX_MASK_SIZE) {
        maskScale = qMin(MAX_MASK_SIZE / (double)smallMask.width(),
                         MAX_MASK_SIZE / (double)smallMask.height());
        smallMask = smallMask.scaled(smallMask.size() * maskScale,
                                     Qt::KeepAspectRatio,
                                     Qt::FastTransformation);  // 快速缩放即可
    }

    QImage alphaImg = smallMask.toImage().convertToFormat(QImage::Format_Alpha8);
    QBitmap maskBitmap(alphaImg.size());
    maskBitmap.fill(Qt::color0);
    {
        QPainter mp(&maskBitmap);
        mp.setPen(Qt::color1);
        mp.setBrush(Qt::color1);
        for (int y = 0; y < alphaImg.height(); ++y) {
            for (int x = 0; x < alphaImg.width(); ++x) {
                if (qAlpha(alphaImg.pixel(x, y)) > 30) {
                    mp.drawPoint(x, y);
                }
            }
        }
    }

    if (maskScale < 1.0) {
        maskBitmap = maskBitmap.scaled(scaledPiece.size(),
                                       Qt::KeepAspectRatio,
                                       Qt::FastTransformation);
    }

    QRegion finalMask;
    finalMask += QRegion(maskBitmap).translated(visibleRect.topLeft());
    setX11Shape(finalMask);
}



void ImageWidget::setMask(const QRegion &region)
{
    // 🚨 缩略图模式下禁止任何 setMask 调用！
    if (currentViewMode != SingleView) {
        qDebug() << "\n🔥 非法 setMask 调用！当前模式:"
                 << (currentViewMode == SingleView ? "SingleView" : "ThumbnailView")
                 << "，已拦截。堆栈如下：";
        void* callstack[128];
        int frames = backtrace(callstack, 128);
        char** strs = backtrace_symbols(callstack, frames);
        for (int i = 0; i < frames; ++i) {
            qDebug() << "  " << strs[i];
        }
        free(strs);
        return;  // ⚡ 不调用 QWidget::setMask，直接返回！
    }

    QWidget::setMask(region);
}

void ImageWidget::clearMask()
{
    qDebug() << "【clearMask】调用，模式:"
             << (currentViewMode == SingleView ? "SingleView" : "ThumbnailView");
    QWidget::clearMask();
}

// 清除 X11 输入形状
void ImageWidget::clearX11Shape()
{
#ifdef Q_OS_LINUX
    if (!QGuiApplication::platformName().contains("xcb")) return;
    Display *display = XOpenDisplay(nullptr);
    if (!display) return;
    Window windowId = (Window)winId();
    if (windowId) {
        // 清除输入形状，恢复全窗口可点
        XShapeCombineMask(display, windowId, ShapeInput, 0, 0, None, ShapeSet);
        XFlush(display);
        qDebug() << "X11 形状已清除";
    }
    XCloseDisplay(display);
#endif
}

// 设置单个矩形形状
void ImageWidget::setX11ShapeRect(const QRect &rect)
{
#ifdef Q_OS_LINUX
    if (!QGuiApplication::platformName().contains("xcb")) return;
    Display *display = XOpenDisplay(nullptr);
    if (!display) return;
    Window windowId = (Window)winId();
    if (windowId) {
        XRectangle xr = { (short)rect.x(), (short)rect.y(),
                         (unsigned short)rect.width(), (unsigned short)rect.height() };
        XShapeCombineRectangles(display, windowId, ShapeInput, 0, 0, &xr, 1, ShapeSet, YXBanded);
        XFlush(display);

        // 读取当前输入形状并打印
        int count = 0;
        int ordering = 0;
        XRectangle *rects = XShapeGetRectangles(display, windowId, ShapeInput, &count, &ordering);
        qDebug() << "实际 X11 输入形状矩形数:" << count;
        for (int i = 0; i < count; ++i) {
            qDebug() << "  " << rects[i].x << rects[i].y << rects[i].width << rects[i].height;
        }
        XFree(rects);

        XCloseDisplay(display);
    }
#endif
}

// 设置复杂形状（多个矩形）
void ImageWidget::setX11Shape(const QRegion &region)
{
#ifdef Q_OS_LINUX
    if (!QGuiApplication::platformName().contains("xcb")) return;
    Display *display = XOpenDisplay(nullptr);
    if (!display) return;
    Window windowId = (Window)winId();
    if (windowId) {
        // 先清除旧的形状
        XShapeCombineMask(display, windowId, ShapeInput, 0, 0, None, ShapeSet);

        // 将 QRegion 转换为 XRectangle 列表
        QVector<XRectangle> rects;
        for (const QRect &r : region) {
            XRectangle xr;
            xr.x = static_cast<short>(r.x());
            xr.y = static_cast<short>(r.y());
            xr.width = static_cast<unsigned short>(r.width());
            xr.height = static_cast<unsigned short>(r.height());
            rects.append(xr);
        }
        XShapeCombineRectangles(display, windowId, ShapeInput,
                                0, 0, rects.data(), rects.size(), ShapeSet, YXBanded);
        XFlush(display);
        qDebug() << "X11 形状已更新，矩形数:" << rects.size();
    }
    XCloseDisplay(display);
#endif
}
