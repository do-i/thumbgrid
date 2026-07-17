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

#ifdef USE_OPENCV
#include "3rdparty/QtOpenCV/cvmatandqimage.h"
#include <opencv2/imgproc.hpp>
#endif

class ImageLib {
    public:
        static QImage *rotatedRaw(const QImage *src, int grad);
        static QImage *rotated(const std::shared_ptr<const QImage>& src, int grad);

        static QImage *croppedRaw(const QImage *src, QRect newRect);
        static QImage *cropped(const std::shared_ptr<const QImage>& src, QRect newRect);

        static QImage *flippedHRaw(const QImage *src);
        static QImage *flippedH(const std::shared_ptr<const QImage>& src);

        static QImage *flippedVRaw(const QImage *src);
        static QImage *flippedV(const std::shared_ptr<const QImage>& src);

        static QImage *scaled(const std::shared_ptr<const QImage>& source, QSize destSize, ScalingFilter filter);

        static QImage *scaled_Qt(const QImage *source, QSize destSize, bool smooth);
        static QImage *scaled_Qt(const std::shared_ptr<const QImage>& source, QSize destSize, bool smooth);
        static void drawTransparencyGrid(QPainter *painter, const QRectF &rect);

#ifdef USE_OPENCV
        static QImage *scaled_CV(const std::shared_ptr<const QImage>& source, QSize destSize, cv::InterpolationFlags filter, int sharpen);
#endif
        static std::unique_ptr<const QImage> exifRotated(std::unique_ptr<const QImage> src, int orientation);
        static std::unique_ptr<QImage> exifRotated(std::unique_ptr<QImage> src, int orientation);
        static void recolor(QPixmap &pixmap, QColor color);
};
