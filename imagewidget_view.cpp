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

void ImageWidget::paintEvent(QPaintEvent *event)
{
    if (currentViewMode == SingleView) {
        Q_UNUSED(event);
        QPainter painter(this);

        // 如果启用了透明背景，使用透明颜色填充
        if (testAttribute(Qt::WA_TranslucentBackground)) {
            painter.fillRect(rect(), QColor(0, 0, 0, 0));
        } else {
            // 非透明背景时填充黑色
            painter.fillRect(rect(), Qt::black);
        }

        // 安全检查：确保pixmap有效
        if (pixmap.isNull()) {
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, tr("没有图片或图片加载失败"));
            return;
        }

        // 安全检查：确保scaleFactor有效
        if (scaleFactor <= 0) {
            scaleFactor = 1.0;
        }

        QSize scaledSize = pixmap.size() * scaleFactor;

        // 安全检查：确保缩放后的尺寸有效
        if (scaledSize.width() <= 0 || scaledSize.height() <= 0) {
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, tr("图片尺寸无效"));
            return;
        }

        //qDebug() << "绘制图片 - 原始尺寸:" << pixmap.size() << "缩放后:" << scaledSize;

        QPixmap scaledPixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 安全检查：确保缩放后的pixmap有效
        if (scaledPixmap.isNull()) {
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, tr("图片缩放失败"));
            return;
        }

        QPointF offset((width() - scaledPixmap.width()) / 2 + panOffset.x(),
                       (height() - scaledPixmap.height()) / 2 + panOffset.y());

        painter.drawPixmap(offset, scaledPixmap);

        // 添加变换状态提示
        if (isTransformed()) {
            painter.setPen(Qt::yellow);
            painter.setFont(QFont("Arial", 10));

            QString transformInfo;
            if (rotationAngle != 0) {
                transformInfo += QString("旋转: %1° ").arg(rotationAngle);
            }
            if (isHorizontallyFlipped) {
                transformInfo += "水平镜像 ";
            }
            if (isVerticallyFlipped) {
                transformInfo += "垂直镜像 ";
            }

            if (!transformInfo.isEmpty()) {
                painter.drawText(10, 30, transformInfo.trimmed());
            }
        }

        // 添加左右箭头提示（半透明，只在图片较大时显示）
        if (scaledSize.width() > 600 && scaledSize.height() > 600) {
            drawNavigationArrows(painter, offset, scaledSize);
        }

        //qDebug() << "图片绘制完成";
    }
}

void ImageWidget::drawNavigationArrows(QPainter &painter, const QPointF &offset,
                                       const QSize &scaledSize)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(0.5); // 半透明

    // 左箭头
    QPainterPath leftArrow;
    leftArrow.moveTo(offset.x() + 20, offset.y() + scaledSize.height() / 2);
    leftArrow.lineTo(offset.x() + 40, offset.y() + scaledSize.height() / 2 - 15);
    leftArrow.lineTo(offset.x() + 40, offset.y() + scaledSize.height() / 2 + 15);
    leftArrow.closeSubpath();
    painter.fillPath(leftArrow, Qt::white);

    // 右箭头
    QPainterPath rightArrow;
    rightArrow.moveTo(offset.x() + scaledSize.width() - 20, offset.y() + scaledSize.height() / 2);
    rightArrow.lineTo(offset.x() + scaledSize.width() - 40, offset.y() + scaledSize.height() / 2 - 15);
    rightArrow.lineTo(offset.x() + scaledSize.width() - 40, offset.y() + scaledSize.height() / 2 + 15);
    rightArrow.closeSubpath();
    painter.fillPath(rightArrow, Qt::white);

    painter.setOpacity(1.0); // 恢复不透明
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
    if (currentViewMode == SingleView) {
        // 标记为手动调整
        currentViewStateType = ManualAdjustment;

        double zoomFactor = 1.15;
        double oldScaleFactor = scaleFactor;

        if (event->angleDelta().y() > 0) {
            scaleFactor *= zoomFactor;
        } else {
            scaleFactor /= zoomFactor;
        }

        scaleFactor = qBound(0.03, scaleFactor, 8.0);

        // 获取鼠标位置
        QPointF mousePos = event->position();
        QPointF viewCenter(width() / 2.0, height() / 2.0);

        // 计算以鼠标为中心的缩放偏移
        QPointF imagePos = (mousePos - viewCenter - panOffset) / oldScaleFactor;
        panOffset = mousePos - viewCenter - imagePos * scaleFactor;

        updateMask();
        update();


    }
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

    // 非单图模式或无图片时，清除所有形状（X11 + Qt）
    if (currentViewMode != SingleView || pixmap.isNull()) {
        clearX11Shape();
        clearMask();
        return;
    }

    // 只有透明模式时才需要掩码
    if (!testAttribute(Qt::WA_TranslucentBackground)) {
        clearX11Shape();
        clearMask();
        return;
    }

    // 1. 获取当前完整缩放图片及其位置
    QSize scaledSize = pixmap.size() * scaleFactor;
    if (scaledSize.isEmpty()) {
        clearX11Shape();
        clearMask();
        return;
    }

    QPixmap scaledPixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPointF offset((width() - scaledPixmap.width()) / 2.0 + panOffset.x(),
                   (height() - scaledPixmap.height()) / 2.0 + panOffset.y());

    // 2. 裁剪出窗口中实际可见的图片区域
    QRect imageRect = QRect(offset.toPoint(), scaledPixmap.size()).intersected(rect());
    if (imageRect.isEmpty()) {
        clearX11Shape();
        clearMask();
        return;
    }

    // 3. 快速路径：无透明通道 → 整个矩形可点击
    if (!pixmap.hasAlphaChannel()) {
        setX11ShapeRect(imageRect);
        return;
    }

    // 4. 提取可见区域的图片（从缩放后的完整图片中截取）
    QPixmap visiblePixmap = scaledPixmap.copy(imageRect.translated(-offset.toPoint()));

    // 5. 性能优化：将可见区域缩小到不超过 300x300 进行逐像素检测
    const int MAX_MASK_SIZE = 300;
    QPixmap maskPixmap = visiblePixmap;
    double scale = 1.0;
    if (maskPixmap.width() > MAX_MASK_SIZE || maskPixmap.height() > MAX_MASK_SIZE) {
        scale = qMin(MAX_MASK_SIZE / (double)maskPixmap.width(),
                     MAX_MASK_SIZE / (double)maskPixmap.height());
        maskPixmap = maskPixmap.scaled(maskPixmap.size() * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // 6. 生成 alpha 掩码位图
    QImage alphaImage = maskPixmap.toImage().convertToFormat(QImage::Format_Alpha8);
    QBitmap maskBitmap(alphaImage.size());
    maskBitmap.fill(Qt::color0);  // 全不可点击

    QPainter maskPainter(&maskBitmap);
    maskPainter.setPen(Qt::color1);
    maskPainter.setBrush(Qt::color1);

    for (int y = 0; y < alphaImage.height(); ++y) {
        for (int x = 0; x < alphaImage.width(); ++x) {
            if (qAlpha(alphaImage.pixel(x, y)) > 30) {
                maskPainter.drawPoint(x, y);
            }
        }
    }
    maskPainter.end();

    // 7. 如果对掩码图片进行了缩小，需要放大回原始可见区域大小
    if (scale < 1.0) {
        maskBitmap = maskBitmap.scaled(visiblePixmap.size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    // 8. 将掩码放置到窗口的正确位置（imageRect 左上角）
    QRegion finalMask;
    QRegion imageRegion(maskBitmap);
    finalMask += imageRegion.translated(imageRect.topLeft());

    // 9. 通过 X11 设置形状
    setX11Shape(finalMask);
}

#include <execinfo.h>
#include <cxxabi.h>

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
        XRectangle xr;
        xr.x = static_cast<short>(rect.x());
        xr.y = static_cast<short>(rect.y());
        xr.width = static_cast<unsigned short>(rect.width());
        xr.height = static_cast<unsigned short>(rect.height());
        XShapeCombineRectangles(display, windowId, ShapeInput, 0, 0, &xr, 1, ShapeSet, YXBanded);
        XFlush(display);
        qDebug() << "X11 形状设置为矩形:" << rect;
    }
    XCloseDisplay(display);
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
