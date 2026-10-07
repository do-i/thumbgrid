#include "resizecopy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cstdlib>
#include "sourcecontainers/documentinfo.h"
#include "sourcecontainers/imagestatic.h"
#include "utils/imagelib.h"

namespace ResizeCopy {

QSize targetSize(const QSize &source, const ResizeSpec &spec) {
    if(source.isEmpty())
        return QSize();
    QSize target;
    switch(spec.mode) {
        case ResizeSpec::Percent: {
            // truncate like ResizeDialog does, so the size the dialog showed
            // for the seed image is exactly the size that gets written
            double scale = spec.percent / 100.0;
            target = QSize(qMax(1, static_cast<int>(source.width() * scale)),
                           qMax(1, static_cast<int>(source.height() * scale)));
            break;
        }
        case ResizeSpec::FitWithin:
            if(spec.size.isEmpty())
                return QSize();
            target = source.scaled(spec.size, Qt::KeepAspectRatio);
            // The dialog derives the other side of the box from the seed image
            // with truncation, so fitting the seed back into it can land a
            // pixel short of what the user typed. Snap to the box then.
            if(std::abs(target.width() - spec.size.width()) <= 1 &&
               std::abs(target.height() - spec.size.height()) <= 1)
                target = spec.size;
            target = target.expandedTo(QSize(1, 1));
            break;
        case ResizeSpec::Exact:
            if(spec.size.isEmpty())
                return QSize();
            target = spec.size;
            break;
        case ResizeSpec::LongEdge: {
            if(spec.longEdge <= 0)
                return QSize();
            bool landscape = source.width() >= source.height();
            int longSide = landscape ? source.width() : source.height();
            int shortSide = landscape ? source.height() : source.width();
            // the long side is exact by definition; round only the short one
            int scaledShort = qMax(1, qRound(static_cast<double>(shortSide) * spec.longEdge / longSide));
            target = landscape ? QSize(spec.longEdge, scaledShort) : QSize(scaledShort, spec.longEdge);
            break;
        }
    }
    if(target == source)
        return QSize();
    if(spec.shrinkOnly && (target.width() > source.width() || target.height() > source.height()))
        return QSize();
    return target;
}

QString copyPath(const QString &sourcePath, const QSize &target, const QSet<QString> &reserved) {
    QFileInfo fi(sourcePath);
    QString stem = fi.absolutePath() + "/" + fi.completeBaseName() + "_" +
                   QString::number(target.width()) + "x" + QString::number(target.height());
    QString ext = fi.suffix().isEmpty() ? QString() : "." + fi.suffix();
    QString candidate = stem + ext;
    for(int n = 1; QFileInfo::exists(candidate) || reserved.contains(candidate); ++n)
        candidate = stem + "-" + QString::number(n) + ext;
    return candidate;
}

Result resizeFile(const QString &sourcePath, const ResizeSpec &spec, QSet<QString> *reserved) {
    Result result;
    auto info = std::make_unique<DocumentInfo>(sourcePath);
    // video, animation and anything else without a single still frame
    if(info->type() != STATIC) {
        result.outcome = Outcome::Skipped;
        return result;
    }
    ImageStatic image(std::move(info));
    QSize target = targetSize(image.size(), spec);
    QString ext = QFileInfo(sourcePath).suffix();
    if(!target.isValid() || !DocumentInfo::canEncodeToFormat(ext, target)) {
        result.outcome = Outcome::Skipped;
        return result;
    }
    result.destPath = copyPath(sourcePath, target, reserved ? *reserved : QSet<QString>());
    if(reserved)
        reserved->insert(result.destPath);

    std::unique_ptr<QImage> scaled = ImageLib::scaled(image.getImage(), target, spec.filter);
    if(!scaled || scaled->isNull()) {
        result.outcome = Outcome::Failed;
        return result;
    }
    image.setEditedImage(std::unique_ptr<const QImage>(scaled.release()));
    // ImageStatic::save carries the source's Exif/IPTC/XMP over and resets the
    // orientation/dimension tags to match the already-upright scaled pixels
    if(image.save(result.destPath)) {
        result.outcome = Outcome::Resized;
    } else {
        // the path was free before (copyPath), so anything there now is our
        // own partial write - SafeSave only restores files that existed
        QFile::remove(result.destPath);
        result.outcome = Outcome::Failed;
    }
    return result;
}

} // namespace ResizeCopy
