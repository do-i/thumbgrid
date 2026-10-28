#include "imagelib.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <thread>
#include <vector>

void ImageLib::recolor(QPixmap &pixmap, QColor color) {
    if(pixmap.isNull())
        return;

    QPainter p(&pixmap);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.setBrush(color);
    p.setPen(color);
    p.drawRect(pixmap.rect());
}

void ImageLib::sharpen(QImage &image) {
    if(image.width() < 3 || image.height() < 3)
        return;
    if(image.depth() != 32)
        image = image.convertToFormat(image.hasAlphaChannel() ? QImage::Format_ARGB32_Premultiplied
                                                              : QImage::Format_RGB32);
    constexpr int radius = 6; // OpenCV's kernel size for sigma 2
    constexpr int taps = 2 * radius + 1;
    constexpr int shift = 14;
    int kernel[taps];
    int kernelSum = 0;
    for(int i = 0; i < taps; i++) {
        kernel[i] = qRound((1 << shift) / (2 * std::sqrt(2 * M_PI)) * std::exp(-(i - radius) * (i - radius) / 8.0));
        kernelSum += kernel[i];
    }
    kernel[radius] += (1 << shift) - kernelSum; // exact unit gain, so flat areas stay untouched
    // OpenCV's default border (reflect-101)
    const auto reflect = [](int i, int n) {
        while(i < 0 || i >= n)
            i = (i < 0) ? -i : 2 * n - 2 - i;
        return i;
    };
    const int w = image.width(), h = image.height(), bytes = w * 4;
    QImage blur(w, h, image.format());
    const auto parallelRows = [h](const std::function<void(int, int)> &work) {
        const int threads = qBound(1, int(std::thread::hardware_concurrency()), 8);
        const int band = (h + threads - 1) / threads;
        std::vector<std::thread> pool;
        for(int start = band; start < h; start += band)
            pool.emplace_back(work, start, std::min(h, start + band));
        work(0, std::min(h, band));
        for(auto &t : pool)
            t.join();
    };
    parallelRows([&](int from, int to) {
        std::vector<uchar> line(size_t(w + 2 * radius) * 4);
        std::vector<int> acc(bytes);
        for(int y = from; y < to; y++) {
            const uchar *src = image.constScanLine(y);
            for(int i = -radius; i < w + radius; i++)
                std::memcpy(line.data() + (i + radius) * 4, src + reflect(i, w) * 4, 4);
            std::fill(acc.begin(), acc.end(), 1 << (shift - 1));
            for(int k = 0; k < taps; k++) {
                const uchar *p = line.data() + k * 4;
                const int weight = kernel[k];
                for(int x = 0; x < bytes; x++)
                    acc[x] += weight * p[x];
            }
            uchar *dst = blur.scanLine(y);
            for(int x = 0; x < bytes; x++)
                dst[x] = uchar(acc[x] >> shift);
        }
    });
    QImage original = image.hasAlphaChannel() ? image.copy() : QImage();
    parallelRows([&](int from, int to) {
        std::vector<int> acc(bytes);
        for(int y = from; y < to; y++) {
            std::fill(acc.begin(), acc.end(), 1 << (shift - 1));
            for(int k = 0; k < taps; k++) {
                const uchar *row = blur.constScanLine(reflect(y + k - radius, h));
                const int weight = kernel[k];
                for(int x = 0; x < bytes; x++)
                    acc[x] += weight * row[x];
            }
            // out = 1.25 * src - 0.25 * blur
            uchar *dst = image.scanLine(y);
            for(int x = 0; x < bytes; x++)
                dst[x] = uchar(qBound(0, (5 * dst[x] - (acc[x] >> shift) + 2) >> 2, 255));
        }
    });
    if(original.isNull())
        return;
    // alpha is not sharpened, and premultiplied colors stay within it
    constexpr int alphaByte = (Q_BYTE_ORDER == Q_LITTLE_ENDIAN) ? 3 : 0;
    const bool premultiplied = image.format() == QImage::Format_ARGB32_Premultiplied;
    for(int y = 0; y < h; y++) {
        const uchar *src = original.constScanLine(y);
        uchar *dst = image.scanLine(y);
        for(int x = 0; x < bytes; x += 4) {
            const uchar alpha = src[x + alphaByte];
            dst[x + alphaByte] = alpha;
            if(premultiplied)
                for(int c = 0; c < 4; c++)
                    dst[x + c] = std::min(dst[x + c], alpha);
        }
    }
}

void ImageLib::drawTransparencyGrid(QPainter *painter, const QRectF &rect) {
    static const QPixmap pattern = []() {
        QPixmap pixmap(16, 16);
        pixmap.fill(QColor(242, 242, 242));
        QPainter patternPainter(&pixmap);
        patternPainter.fillRect(0, 0, 8, 8, QColor(218, 218, 218));
        patternPainter.fillRect(8, 8, 8, 8, QColor(218, 218, 218));
        return pixmap;
    }();
    painter->drawTiledPixmap(rect, pattern);
}

std::unique_ptr<QImage> ImageLib::rotatedRaw(const QImage *src, int grad) {
    if(!src)
        return std::make_unique<QImage>();
    auto img = std::make_unique<QImage>();
    QTransform transform;
    transform.rotate(grad);
    *img = src->transformed(transform, Qt::SmoothTransformation);
    return img;
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::rotated(const std::shared_ptr<const QImage>& src, int grad) {
    return rotatedRaw(src.get(), grad);
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::croppedRaw(const QImage *src, QRect newRect) {
    if(src && src->rect().contains(newRect, false)) {
        auto img = std::make_unique<QImage>(newRect.size(), src->format());
        *img = src->copy(newRect);
        return img;
    } else {
        return std::make_unique<QImage>();
    }
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::cropped(const std::shared_ptr<const QImage>& src, QRect newRect) {
    return croppedRaw(src.get(), newRect);
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::flippedHRaw(const QImage *src) {
    if(!src)
        return std::make_unique<QImage>();
    else
        return std::make_unique<QImage>(src->flipped(Qt::Horizontal));
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::flippedH(const std::shared_ptr<const QImage>& src) {
    return flippedHRaw(src.get());
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::flippedVRaw(const QImage *src) {
    if(!src)
        return std::make_unique<QImage>();
    else
        return std::make_unique<QImage>(src->flipped(Qt::Vertical));
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::flippedV(const std::shared_ptr<const QImage>& src) {
    return flippedVRaw(src.get());
}
//------------------------------------------------------------------------------
// nullptr when the orientation needs no transform
static std::unique_ptr<QImage> exifTransformed(const QImage *src, int orientation) {
    switch(orientation) {
    case 1: return ImageLib::flippedHRaw(src);
    case 2: return ImageLib::flippedVRaw(src);
    case 3: return ImageLib::flippedVRaw(ImageLib::flippedHRaw(src).get());
    case 4: return ImageLib::rotatedRaw(src, 90);
    case 5: return ImageLib::rotatedRaw(ImageLib::flippedHRaw(src).get(), 90);
    case 6: return ImageLib::rotatedRaw(ImageLib::flippedVRaw(src).get(), 90);
    case 7: return ImageLib::rotatedRaw(src, -90);
    default: return nullptr;
    }
}

std::unique_ptr<const QImage> ImageLib::exifRotated(std::unique_ptr<const QImage> src, int orientation) {
    if(auto out = exifTransformed(src.get(), orientation))
        return out;
    return src;
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::exifRotated(std::unique_ptr<QImage> src, int orientation) {
    if(auto out = exifTransformed(src.get(), orientation))
        return out;
    return src;
}
//------------------------------------------------------------------------------

std::unique_ptr<QImage> ImageLib::scaled(const std::shared_ptr<const QImage>& source, QSize destSize, ScalingFilter filter) {
    if(!source)
        return std::make_unique<QImage>();
    auto scaleTarget = source;
    if(source->format() == QImage::Format_Indexed8) {
        auto newFmt = QImage::Format_RGB32;
        if(source->hasAlphaChannel())
            newFmt = QImage::Format_ARGB32;
        scaleTarget.reset(new QImage(source->convertToFormat(newFmt)));
    }
#ifdef USE_OPENCV
    if(filter >= QI_FILTER_CV_BILINEAR_SHARPEN && filter <= QI_FILTER_CV_CUBIC_SHARPEN &&
       !QtOcv::isSupported(scaleTarget->format()))
        filter = QI_FILTER_BILINEAR;
#endif
    switch (filter) {
        case QI_FILTER_NEAREST:
            return scaled_Qt(scaleTarget, destSize, false);
        case QI_FILTER_BILINEAR:
            return scaled_Qt(scaleTarget, destSize, true);
        case QI_FILTER_QT_SMOOTH_SHARPEN: {
            auto dest = scaled_Qt(scaleTarget, destSize, true);
            if(destSize.width() < scaleTarget->width() / 2)
                sharpen(*dest);
            return dest;
        }
#ifdef USE_OPENCV
        case QI_FILTER_CV_BILINEAR_SHARPEN:
            return scaled_CV(scaleTarget, destSize, cv::INTER_LINEAR, 0);
        case QI_FILTER_CV_CUBIC:
            return scaled_CV(scaleTarget, destSize, cv::INTER_CUBIC, 0);
        case QI_FILTER_CV_CUBIC_SHARPEN:
            return scaled_CV(scaleTarget, destSize, cv::INTER_CUBIC, 1);
#endif
        default:
            return scaled_Qt(scaleTarget, destSize, true);
    }
}

std::unique_ptr<QImage> ImageLib::scaled_Qt(const std::shared_ptr<const QImage>& source, QSize destSize, bool smooth) {
    if(!source)
        return std::make_unique<QImage>();
    auto dest = std::make_unique<QImage>();
    Qt::TransformationMode mode = smooth ? Qt::SmoothTransformation : Qt::FastTransformation;
    *dest = source->scaled(destSize.width(), destSize.height(), Qt::IgnoreAspectRatio, mode);
    return dest;
}

#ifdef USE_OPENCV
// this probably leaks, needs checking
std::unique_ptr<QImage> ImageLib::scaled_CV(const std::shared_ptr<const QImage>& source, QSize destSize, cv::InterpolationFlags filter, int sharpen) {
    if(!source)
        return std::make_unique<QImage>();
    QtOcv::MatColorOrder order;
    cv::Mat srcMat = QtOcv::image2Mat_shared(*source.get(), &order);
    cv::Size destSizeCv(destSize.width(), destSize.height());
    auto dest = std::make_unique<QImage>();
    if(destSize == source->size()) {
        // TODO: should this return a copy?
    } else if(destSize.width() > source.get()->width()) { // upscale
        cv::Mat dstMat(destSizeCv, srcMat.type());
        cv::resize(srcMat, dstMat, destSizeCv, 0, 0, filter);
        *dest = QtOcv::mat2Image(dstMat, order, source->format());
    } else { // downscale
        float scale = (float)destSize.width() / source->width();
        if(scale < 0.5f && filter != cv::INTER_NEAREST) {
            if(filter == cv::INTER_CUBIC)
                sharpen = 1;
            filter = cv::INTER_AREA;
        }
        cv::Mat dstMat(destSizeCv, srcMat.type());
        cv::resize(srcMat, dstMat, destSizeCv, 0, 0, filter);
        if(!sharpen || filter == cv::INTER_NEAREST) {
            *dest = QtOcv::mat2Image(dstMat, order, source->format());
        } else {
            // todo: tweak this
            double amount = 0.25 * sharpen;
            // unsharp mask
            cv::Mat dstMat_sharpened;
            cv::GaussianBlur(dstMat, dstMat_sharpened, cv::Size(0, 0), 2);
            cv::addWeighted(dstMat, 1.0 + amount, dstMat_sharpened, -amount, 0, dstMat_sharpened);
            *dest = QtOcv::mat2Image(dstMat_sharpened, order, source->format());
        }
    }
    return dest;
}
#endif
