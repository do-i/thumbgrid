#include "imagelib.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <thread>
#include <vector>

// Runs work(from, to) over row bands of [0, rows) on up to 8 threads.
static void forEachRowBand(int rows, const std::function<void(int, int)> &work) {
    const int threads = qBound(1, int(std::thread::hardware_concurrency()), 8);
    const int band = (rows + threads - 1) / threads;
    std::vector<std::thread> pool;
    for(int start = band; start < rows; start += band)
        pool.emplace_back(work, start, std::min(rows, start + band));
    work(0, std::min(rows, band));
    for(auto &t : pool)
        t.join();
}

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
    // detach once here: scanLine() from several threads would race to detach a shared image
    uchar *const pixels = image.bits();
    const qsizetype stride = image.bytesPerLine();
    QImage blur(w, h, image.format());
    uchar *const blurred = blur.bits();
    const qsizetype blurStride = blur.bytesPerLine();
    forEachRowBand(h, [&](int from, int to) {
        std::vector<uchar> line(size_t(w + 2 * radius) * 4);
        std::vector<int> acc(bytes);
        for(int y = from; y < to; y++) {
            const uchar *src = pixels + y * stride;
            for(int i = -radius; i < w + radius; i++)
                std::memcpy(line.data() + (i + radius) * 4, src + reflect(i, w) * 4, 4);
            std::fill(acc.begin(), acc.end(), 1 << (shift - 1));
            for(int k = 0; k < taps; k++) {
                const uchar *p = line.data() + k * 4;
                const int weight = kernel[k];
                for(int x = 0; x < bytes; x++)
                    acc[x] += weight * p[x];
            }
            uchar *dst = blurred + y * blurStride;
            for(int x = 0; x < bytes; x++)
                dst[x] = uchar(acc[x] >> shift);
        }
    });
    QImage original = image.hasAlphaChannel() ? image.copy() : QImage();
    forEachRowBand(h, [&](int from, int to) {
        std::vector<int> acc(bytes);
        for(int y = from; y < to; y++) {
            std::fill(acc.begin(), acc.end(), 1 << (shift - 1));
            for(int k = 0; k < taps; k++) {
                const uchar *row = blurred + reflect(y + k - radius, h) * blurStride;
                const int weight = kernel[k];
                for(int x = 0; x < bytes; x++)
                    acc[x] += weight * row[x];
            }
            // out = 1.25 * src - 0.25 * blur
            uchar *dst = pixels + y * stride;
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
    switch (filter) {
        case QI_FILTER_NEAREST:
            return scaled_Qt(scaleTarget, destSize, false);
        case QI_FILTER_HIGH_QUALITY:
        case QI_FILTER_HIGH_QUALITY_SHARPEN:
        case QI_FILTER_HIGH_QUALITY_SOFT:
            return scaledHighQuality(*scaleTarget, destSize, filter);
        default:
            return scaled_Qt(scaleTarget, destSize, true);
    }
}

// Below half size: Qt's smooth (area-averaging) scaling, sharpened unless SOFT.
// Otherwise bicubic, sharpened when shrinking with the SHARPEN variant.
std::unique_ptr<QImage> ImageLib::scaledHighQuality(const QImage &source, QSize destSize, ScalingFilter filter) {
    auto dest = std::make_unique<QImage>();
    if(source.isNull() || destSize.isEmpty())
        return dest;
    const bool shrinking = destSize.width() < source.width();
    if(destSize.width() < source.width() / 2.0) {
        *dest = source.scaled(destSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if(filter != QI_FILTER_HIGH_QUALITY_SOFT)
            sharpen(*dest);
    } else {
        *dest = bicubicScaled(source, destSize);
        if(filter == QI_FILTER_HIGH_QUALITY_SHARPEN && shrinking)
            sharpen(*dest);
    }
    return dest;
}

// Separable bicubic with the Keys kernel (a = -0.75) and pixel-center sampling,
// edges replicated: the same result as OpenCV's INTER_CUBIC to within 1 level.
QImage ImageLib::bicubicScaled(const QImage &image, QSize destSize) {
    QImage src = image;
    if(src.format() != QImage::Format_RGB32 && src.format() != QImage::Format_ARGB32_Premultiplied)
        src = src.convertToFormat(src.hasAlphaChannel() ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32);
    constexpr float a = -0.75f;
    struct Taps { int index[4]; float weight[4]; };
    const auto taps = [](int dstLen, int srcLen) {
        std::vector<Taps> result(dstLen);
        const double step = double(srcLen) / dstLen;
        for(int d = 0; d < dstLen; d++) {
            const double pos = (d + 0.5) * step - 0.5;
            const int base = int(std::floor(pos));
            const float x = float(pos - base);
            float *w = result[d].weight;
            w[0] = ((a * (x + 1) - 5 * a) * (x + 1) + 8 * a) * (x + 1) - 4 * a;
            w[1] = ((a + 2) * x - (a + 3)) * x * x + 1;
            w[2] = ((a + 2) * (1 - x) - (a + 3)) * (1 - x) * (1 - x) + 1;
            w[3] = 1.f - w[0] - w[1] - w[2];
            for(int k = 0; k < 4; k++)
                result[d].index[k] = std::clamp(base - 1 + k, 0, srcLen - 1);
        }
        return result;
    };
    const auto tx = taps(destSize.width(), src.width()), ty = taps(destSize.height(), src.height());
    const int rowBytes = destSize.width() * 4;
    // horizontal pass into float rows, then vertical
    std::vector<float> rows(size_t(src.height()) * rowBytes);
    forEachRowBand(src.height(), [&](int from, int to) {
        for(int y = from; y < to; y++) {
            const uchar *line = src.constScanLine(y);
            float *row = rows.data() + size_t(y) * rowBytes;
            for(int x = 0; x < destSize.width(); x++)
                for(int c = 0; c < 4; c++) {
                    float sum = 0;
                    for(int k = 0; k < 4; k++)
                        sum += tx[x].weight[k] * line[tx[x].index[k] * 4 + c];
                    row[x * 4 + c] = sum;
                }
        }
    });
    QImage out(destSize, src.format());
    uchar *const outBits = out.bits();
    const qsizetype outStride = out.bytesPerLine();
    const bool premultiplied = src.format() == QImage::Format_ARGB32_Premultiplied;
    constexpr int alphaByte = (Q_BYTE_ORDER == Q_LITTLE_ENDIAN) ? 3 : 0;
    forEachRowBand(destSize.height(), [&](int from, int to) {
        for(int y = from; y < to; y++) {
            const float *in[4];
            for(int k = 0; k < 4; k++)
                in[k] = rows.data() + size_t(ty[y].index[k]) * rowBytes;
            uchar *line = outBits + y * outStride;
            for(int i = 0; i < rowBytes; i++) {
                float sum = 0;
                for(int k = 0; k < 4; k++)
                    sum += ty[y].weight[k] * in[k][i];
                line[i] = uchar(std::clamp(int(std::lrint(sum)), 0, 255));
            }
            // cubic overshoot must not push premultiplied colors past alpha
            if(premultiplied)
                for(int i = 0; i < rowBytes; i += 4)
                    for(int c = 0; c < 4; c++)
                        line[i + c] = std::min(line[i + c], line[i + alphaByte]);
        }
    });
    return out;
}

std::unique_ptr<QImage> ImageLib::scaled_Qt(const std::shared_ptr<const QImage>& source, QSize destSize, bool smooth) {
    if(!source)
        return std::make_unique<QImage>();
    auto dest = std::make_unique<QImage>();
    Qt::TransformationMode mode = smooth ? Qt::SmoothTransformation : Qt::FastTransformation;
    *dest = source->scaled(destSize.width(), destSize.height(), Qt::IgnoreAspectRatio, mode);
    return dest;
}

