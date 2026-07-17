#include "imagelib.h"

void ImageLib::recolor(QPixmap &pixmap, QColor color) {
    if(pixmap.isNull())
        return;

    QPainter p(&pixmap);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.setBrush(color);
    p.setPen(color);
    p.drawRect(pixmap.rect());
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
std::unique_ptr<const QImage> ImageLib::exifRotated(std::unique_ptr<const QImage> src, int orientation) {
    switch(orientation) {
    case 1: {
        src = ImageLib::flippedHRaw(src.get());
    } break;
    case 2: {
        src = ImageLib::flippedVRaw(src.get());
    } break;
    case 3: {
        src = ImageLib::flippedHRaw(src.get());
        src = ImageLib::flippedVRaw(src.get());
    } break;
    case 4: {
        src = ImageLib::rotatedRaw(src.get(), 90);
    } break;
    case 5: {
        src = ImageLib::flippedHRaw(src.get());
        src = ImageLib::rotatedRaw(src.get(), 90);
    } break;
    case 6: {
        src = ImageLib::flippedVRaw(src.get());
        src = ImageLib::rotatedRaw(src.get(), 90);
    } break;
    case 7: {
        src = ImageLib::rotatedRaw(src.get(), -90);
    } break;
    default: {
    } break;
    }
    return src;
}
//------------------------------------------------------------------------------
std::unique_ptr<QImage> ImageLib::exifRotated(std::unique_ptr<QImage> src, int orientation) {
    switch(orientation) {
    case 1: {
        src = ImageLib::flippedHRaw(src.get());
    } break;
    case 2: {
        src = ImageLib::flippedVRaw(src.get());
    } break;
    case 3: {
        src = ImageLib::flippedHRaw(src.get());
        src = ImageLib::flippedVRaw(src.get());
    } break;
    case 4: {
        src = ImageLib::rotatedRaw(src.get(), 90);
    } break;
    case 5: {
        src = ImageLib::flippedHRaw(src.get());
        src = ImageLib::rotatedRaw(src.get(), 90);
    } break;
    case 6: {
        src = ImageLib::flippedVRaw(src.get());
        src = ImageLib::rotatedRaw(src.get(), 90);
    } break;
    case 7: {
        src = ImageLib::rotatedRaw(src.get(), -90);
    } break;
    default: {
    } break;
    }
    return src;
}
//------------------------------------------------------------------------------
/*

QImage *ImageLib::cropped(QRect newRect, QRect targetRes, bool upscaled) {
    QImage *cropped = new QImage(targetRes.size(), image->format());
    if(upscaled) {
        QImage temp = image->copy(newRect);
        *cropped = temp.scaled(targetRes.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QRect target(QPoint(0, 0), targetRes.size());
        target.moveCenter(cropped->rect().center());
        *cropped = cropped->copy(target);
    } else {
        newRect.moveCenter(image->rect().center());
        *cropped = image->copy(newRect);
    }
    return cropped;
}
*/

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
    if(filter > 1 && !QtOcv::isSupported(scaleTarget->format()))
        filter = QI_FILTER_BILINEAR;
#endif
    switch (filter) {
        case QI_FILTER_NEAREST:
            return scaled_Qt(scaleTarget, destSize, false);
        case QI_FILTER_BILINEAR:
            return scaled_Qt(scaleTarget, destSize, true);
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
        //result.reset(new StaticImageContainer(std::make_shared<cv::Mat>(srcMat)));
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
    //qDebug() << "Filter:" << filter << " sharpen=" << sharpen << " source size:" << source->size() << "->" << (float)destSize.width() / source->width() << ": " << t.elapsed() << " ms.";
    return dest;
}
#endif
