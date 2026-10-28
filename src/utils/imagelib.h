#pragma once
#include <QImage>
#include <QPainter>
#include <QPixmapCache>
#include <QDebug>
#include <memory>
#include <QElapsedTimer>
#include <QProcess>
#include "sourcecontainers/documentinfo.h"
#include "settings.h"

class ImageLib {
    public:
        static std::unique_ptr<QImage> rotatedRaw(const QImage *src, int grad);
        static std::unique_ptr<QImage> rotated(const std::shared_ptr<const QImage>& src, int grad);

        static std::unique_ptr<QImage> croppedRaw(const QImage *src, QRect newRect);
        static std::unique_ptr<QImage> cropped(const std::shared_ptr<const QImage>& src, QRect newRect);

        static std::unique_ptr<QImage> flippedHRaw(const QImage *src);
        static std::unique_ptr<QImage> flippedH(const std::shared_ptr<const QImage>& src);

        static std::unique_ptr<QImage> flippedVRaw(const QImage *src);
        static std::unique_ptr<QImage> flippedV(const std::shared_ptr<const QImage>& src);

        static std::unique_ptr<QImage> scaled(const std::shared_ptr<const QImage>& source, QSize destSize, ScalingFilter filter);

        static std::unique_ptr<QImage> scaled_Qt(const QImage *source, QSize destSize, bool smooth);
        static std::unique_ptr<QImage> scaled_Qt(const std::shared_ptr<const QImage>& source, QSize destSize, bool smooth);
        static void drawTransparencyGrid(QPainter *painter, const QRectF &rect);
        // unsharp mask (gaussian sigma 2, amount 0.25)
        static void sharpen(QImage &image);
        static QImage bicubicScaled(const QImage &image, QSize destSize);
        static std::unique_ptr<QImage> scaledHighQuality(const QImage &source, QSize destSize, ScalingFilter filter);
        static std::unique_ptr<const QImage> exifRotated(std::unique_ptr<const QImage> src, int orientation);
        static std::unique_ptr<QImage> exifRotated(std::unique_ptr<QImage> src, int orientation);
        static void recolor(QPixmap &pixmap, QColor color);
};
