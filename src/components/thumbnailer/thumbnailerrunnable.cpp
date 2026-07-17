#include "thumbnailerrunnable.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <utility>

namespace {
    const QString directoryThumbnailIconVersion = "folder-icon-v2";

    QSet<QString> supportedPreviewImageSuffixes() {
        QSet<QString> suffixes;
        for(const QByteArray &format : QImageReader::supportedImageFormats())
            suffixes.insert(QString::fromLatin1(format).toLower());
        suffixes.insert("jfif");
        suffixes.remove("pdf");
        return suffixes;
    }

    bool hasSupportedPreviewImageSuffix(const QFileInfo &fileInfo) {
        static const QSet<QString> suffixes = supportedPreviewImageSuffixes();
        return suffixes.contains(fileInfo.suffix().toLower());
    }

    // Audio files are opened through the mpv video player but have no video
    // track to grab a frame from, so they get a file-type icon instead.
    bool isAudioSuffix(const QString &suffix) {
        static const QSet<QString> suffixes = { "mp3" };
        return suffixes.contains(suffix.toLower());
    }
}

ThumbnailerRunnable::ThumbnailerRunnable(ThumbnailCache* _cache, QString _path, int _size, bool _crop, bool _force) :
    path(std::move(_path)),
    size(_size),
    crop(_crop),
    force(_force),
    cache(_cache)
{
}

ThumbnailerRunnable::ThumbnailerRunnable(ThumbnailCache* _cache, QString _path, int _size, bool _crop, bool _force,
                                         bool _previewFit, bool _showHidden, QImage _iconBase, QString _colorId) :
    path(std::move(_path)),
    size(_size),
    crop(_crop),
    force(_force),
    cache(_cache),
    isDir(true),
    previewFit(_previewFit),
    showHidden(_showHidden),
    iconBase(std::move(_iconBase)),
    colorId(std::move(_colorId))
{
}

void ThumbnailerRunnable::run() {
    if(isDir) {
        std::shared_ptr<Thumbnail> thumbnail = generateDir(cache, path, size, crop, force, previewFit, showHidden, iconBase, colorId);
        emit dirTaskEnd(thumbnail, path);
    } else {
        std::shared_ptr<Thumbnail> thumbnail = generate(cache, path, size, crop, force);
        emit taskEnd(thumbnail, path);
    }
}

QString ThumbnailerRunnable::generateIdString(const QString& path, int size, bool crop) {
    QString queryStr = path + QString::number(size);
    if(crop)
        queryStr.append("s");
    queryStr = QString("%1").arg(QString(QCryptographicHash::hash(queryStr.toUtf8(),QCryptographicHash::Md5).toHex()));
    return queryStr;
}

std::shared_ptr<Thumbnail> ThumbnailerRunnable::generate(ThumbnailCache* cache, const QString& path, int size, bool crop, bool force) {
    DocumentInfo imgInfo(path);
    QString thumbnailId = generateIdString(path, size, crop);
    std::unique_ptr<QImage> image;

    QString time = QString::number(imgInfo.lastModified().toMSecsSinceEpoch());

    if(!force && cache) {
        image = cache->readThumbnail(thumbnailId);
        if(image && image->text("lastModified") != time)
            image.reset(nullptr);
    }

    if(!image) {
        // non-media entries get a rendered file-type icon instead of a decoded picture;
        // it is cheap to draw, so it skips the disk cache entirely
        if(imgInfo.type() == DocumentType::TEXT || imgInfo.type() == DocumentType::NONE ||
           (imgInfo.type() == DocumentType::VIDEO && isAudioSuffix(QFileInfo(imgInfo.fileName()).suffix())))
            return generateFileTypeIcon(imgInfo, size);
        std::pair<std::unique_ptr<QImage>, QSize> pair;
        if(imgInfo.type() == VIDEO)
            pair = createVideoThumbnail(path, size, crop);
        else
            pair = createThumbnail(imgInfo.filePath(), imgInfo.format().toStdString().c_str(), size, crop);
        image = std::move(pair.first);
        QSize originalSize = pair.second;

        image = ImageLib::exifRotated(std::move(image), imgInfo.exifOrientation());

        // put in image info
        image->setText("originalWidth", QString::number(originalSize.width()));
        image->setText("originalHeight", QString::number(originalSize.height()));
        image->setText("lastModified", time);

        if(imgInfo.type() == ANIMATED)
            image->setText("label", " [a]");
        else if(imgInfo.type() == VIDEO)
            image->setText("label", " [v]");

        if(cache) {
            // save thumbnail if it makes sense
            // FIXME: avoid too much i/o
            if(originalSize.width() > size || originalSize.height() > size)
                cache->saveThumbnail(image.get(), thumbnailId);
        }
    }
    auto && tmpPixmap = new QPixmap(image->size());
    *tmpPixmap = QPixmap::fromImage(*image);
    tmpPixmap->setDevicePixelRatio(qApp->devicePixelRatio());

    QString label;
    if(tmpPixmap->width() == 0) {
        label = "error";
    } else  {
        // put info into Thumbnail object
        label = image->text("originalWidth") +
                "x" +
                image->text("originalHeight") +
                image->text("label");
    }
    std::shared_ptr<QPixmap> pixmapPtr(tmpPixmap);
    std::shared_ptr<Thumbnail> thumbnail(new Thumbnail(imgInfo.fileName(), label, size, pixmapPtr));
    return thumbnail;
}

ThumbnailerRunnable::~ThumbnailerRunnable() {
}

QString ThumbnailerRunnable::taskPath() const {
    return path;
}

int ThumbnailerRunnable::taskSize() const {
    return size;
}

bool ThumbnailerRunnable::taskCrop() const {
    return crop;
}

bool ThumbnailerRunnable::taskPreviewFit() const {
    return previewFit;
}

QString ThumbnailerRunnable::taskColorId() const {
    return colorId;
}

std::pair<std::unique_ptr<QImage>, QSize> ThumbnailerRunnable::createThumbnail(const QString& path, const char *format, int size, bool squared) {
    auto reader = std::make_unique<QImageReader>(path, format);
    Qt::AspectRatioMode ARMode = squared?
                (Qt::KeepAspectRatioByExpanding):(Qt::KeepAspectRatio);
    std::unique_ptr<QImage> result;
    QSize originalSize;
    bool indexed = (reader->imageFormat() == QImage::Format_Indexed8);
    bool manualResize = indexed || !reader->supportsOption(QImageIOHandler::Size);
    if(!manualResize) { // resize during read via QImageReader (faster)
        QSize scaledSize = reader->size().scaled(size, size, ARMode);
        reader->setScaledSize(scaledSize);
        if(squared) {
            QRect clip(0, 0, size, size);
            QRect scaledRect(QPoint(0,0), scaledSize);
            clip.moveCenter(scaledRect.center());
            reader->setScaledClipRect(clip);
        }
        originalSize = reader->size();
        result = std::make_unique<QImage>();
        if(!reader->read(result.get())) {
            // If read() returns false there's no guarantee that size conversion worked properly.
            // So we fallback to manual.
            // Se far I've seen this happen only on some weird (corrupted?) jpeg saved from camera
            manualResize = true;
            result.reset();
            // Force reset reader because it is really finicky
            // and can fail on the second read attempt (yeah wtf)
            reader->setFileName("");
            reader = std::make_unique<QImageReader>(path, format);
        }
    }
    if(manualResize) { // manual resize & crop. slower but should just work
        auto fullSize = std::make_unique<QImage>();
        reader->read(fullSize.get());
        if(indexed) {
            auto newFmt = QImage::Format_RGB32;
            if(fullSize->hasAlphaChannel())
                newFmt = QImage::Format_ARGB32;
            fullSize = std::make_unique<QImage>(fullSize->convertToFormat(newFmt));
        }
        originalSize = fullSize->size();
        QSize scaledSize = fullSize->size().scaled(size, size, ARMode);
        if(squared) {
            QRect clip(0, 0, size, size);
            QRect scaledRect(QPoint(0,0), scaledSize);
            clip.moveCenter(scaledRect.center());
            QImage scaled = fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            result = ImageLib::croppedRaw(&scaled, clip);
        } else {
            result = std::make_unique<QImage>(fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
    }
    // force reader to close file so it can be deleted later
    reader->setFileName("");
    return std::make_pair(std::move(result), originalSize);
}

std::shared_ptr<Thumbnail> ThumbnailerRunnable::generateDir(ThumbnailCache *cache, const QString& path, int size, bool crop, bool force,
                                                            bool previewFit, bool showHidden, const QImage &iconBase, const QString &colorId) {
    QString name = QFileInfo(path).fileName();
    QString thumbnailId = generateDirIdString(path, size, previewFit, colorId);
    // directory mtime changes when direct children are added/removed/renamed,
    // which is exactly what changes the preview set
    QString time = QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());

    std::unique_ptr<QImage> composite;
    if(!force && cache) {
        composite = cache->readThumbnail(thumbnailId);
        if(composite && composite->text("lastModified") != time)
            composite.reset(nullptr);
    }

    if(!composite) {
        // iconBase is implicitly shared; the copy becomes deep on first paint (copy-on-write)
        QImage base = iconBase;
        drawDirPreview(base, dirPreviewImages(cache, path, size, crop, showHidden), previewFit);
        base.setText("lastModified", time);
        composite.reset(new QImage(base));
        if(cache)
            cache->saveThumbnail(composite.get(), thumbnailId);
    }

    auto pixmap = new QPixmap(QPixmap::fromImage(*composite));
    pixmap->setDevicePixelRatio(qApp->devicePixelRatio());
    return std::shared_ptr<Thumbnail>(new Thumbnail(name, "Folder", size, std::shared_ptr<QPixmap>(pixmap), false));
}

// Cache key includes the preview-fit mode, scheme icon color, and folder icon
// generation version, so changes regenerate rather than serving stale composites.
QString ThumbnailerRunnable::generateDirIdString(const QString& path, int size, bool previewFit, const QString &colorId) {
    QString queryStr = path + QString::number(size) + "dir";
    if(previewFit)
        queryStr.append("f");
    queryStr.append(colorId);
    queryStr.append(directoryThumbnailIconVersion);
    return QString(QCryptographicHash::hash(queryStr.toUtf8(), QCryptographicHash::Md5).toHex());
}

QList<QImage> ThumbnailerRunnable::dirPreviewImages(ThumbnailCache *cache, const QString &path, int targetSize, bool crop, bool showHidden) {
    QList<QImage> images;
    QDir dir(path);
    if(!dir.exists() || !dir.isReadable())
        return images;

    QDir::Filters filters = QDir::Files | QDir::Readable | QDir::NoDotAndDotDot;
    if(showHidden)
        filters |= QDir::Hidden;

    const int maxScannedEntries = 80;
    int scanned = 0;
    QFileInfoList entries = dir.entryInfoList(filters, QDir::Name | QDir::IgnoreCase);
    for(const QFileInfo &entry : entries) {
        if(scanned++ >= maxScannedEntries || images.count() >= 4)
            break;
        if(!hasSupportedPreviewImageSuffix(entry))
            continue;

        // best-effort: reuse an already-cached per-file thumbnail (same size+crop as
        // the grid) as the preview tile, avoiding a fresh decode. Falls back below.
        if(cache) {
            QString childId = generateIdString(entry.absoluteFilePath(), targetSize, crop);
            std::unique_ptr<QImage> cached = cache->readThumbnail(childId);
            if(cached && !cached->isNull() &&
               cached->text("lastModified") == QString::number(entry.lastModified().toMSecsSinceEpoch())) {
                images << *cached;
                continue;
            }
        }

        QImageReader reader(entry.absoluteFilePath());
        if(!reader.canRead())
            continue;

        QSize originalSize = reader.size();
        if(originalSize.isValid()) {
            QSize scaledSize = originalSize.scaled(targetSize, targetSize, Qt::KeepAspectRatioByExpanding);
            reader.setScaledSize(scaledSize);
        }

        QImage image = reader.read();
        if(!image.isNull())
            images << image;
    }
    return images;
}

void ThumbnailerRunnable::drawDirPreview(QImage &base, const QList<QImage> &images, bool previewFit) {
    if(images.isEmpty())
        return;

    int gutter = qMax(2, base.width() / 30);
    int bodyTop = qRound(base.height() * 0.125f);
    QRect previewRect(gutter,
                      bodyTop + gutter,
                      base.width() - gutter * 2,
                      base.height() - bodyTop - gutter * 2);

    // false = "Cover" (mode A): crop each preview to fill its cell.
    // true  = "Contain" (mode B): fit the whole image, no cropping, with a
    //         layout that adapts to image orientation for the 1-2 image cases.
    bool fit = previewFit;

    int columns, rows;
    if(images.count() == 1) {
        columns = 1;
        rows = 1;
    } else if(images.count() == 2 && fit) {
        // Two images: stack wide images in 2 rows, place tall images in 2 cols.
        // (Mixed orientations fall back to side-by-side columns.)
        bool bothWide = true;
        for(const QImage &img : images)
            if(img.width() <= img.height())
                bothWide = false;
        columns = bothWide ? 1 : 2;
        rows    = bothWide ? 2 : 1;
    } else {
        columns = 2;
        rows = images.count() <= 2 ? 1 : 2;
    }

    int cellWidth = (previewRect.width() - gutter * (columns - 1)) / columns;
    int cellHeight = (previewRect.height() - gutter * (rows - 1)) / rows;
    if(cellWidth <= 0 || cellHeight <= 0)
        return;

    QPainter painter(&base);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    for(int i = 0; i < images.count(); i++) {
        int row = i / columns;
        int column = i % columns;
        QRect cell(previewRect.left() + column * (cellWidth + gutter),
                   previewRect.top() + row * (cellHeight + gutter),
                   cellWidth,
                   cellHeight);

        if(fit) {
            // Contain: show the entire image, centered within the cell.
            QSize scaledSize = images.at(i).size().scaled(cell.size(), Qt::KeepAspectRatio);
            QRect target(QPoint(0, 0), scaledSize);
            target.moveCenter(cell.center());
            painter.drawImage(target, images.at(i));
        } else {
            // Cover: fill the cell, cropping the overflow.
            QImage scaled = images.at(i).scaled(cell.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QRect source((scaled.width() - cell.width()) / 2,
                         (scaled.height() - cell.height()) / 2,
                         cell.width(),
                         cell.height());
            painter.drawImage(cell, scaled, source);
        }
    }
}

std::shared_ptr<Thumbnail> ThumbnailerRunnable::generateFileTypeIcon(const DocumentInfo &imgInfo, int size) {
    QString suffix = QFileInfo(imgInfo.fileName()).suffix().toLower();
    bool viewable = (imgInfo.type() == DocumentType::TEXT) || isAudioSuffix(suffix);
    QImage icon = renderFileTypeIcon(suffix, viewable, size);
    auto pixmap = new QPixmap(QPixmap::fromImage(icon));
    pixmap->setDevicePixelRatio(qApp->devicePixelRatio());
    QString label;
    if(viewable)
        label = suffix.isEmpty() ? QObject::tr("Text file") : suffix.toUpper() + QObject::tr(" file");
    else
        label = QObject::tr("Unknown format");
    return std::shared_ptr<Thumbnail>(
        new Thumbnail(imgInfo.fileName(), label, size, std::shared_ptr<QPixmap>(pixmap), false));
}

// Generic "document page" icon with a folded corner. Text files get faux text
// lines plus an extension badge (e.g. [TXT]); files thumbgrid cannot render get
// a dimmed page with a big question mark instead. Uses only QImage + QPainter,
// so it is safe to run on thumbnailer worker threads.
QImage ThumbnailerRunnable::renderFileTypeIcon(const QString &suffix, bool viewable, int size) {
    const ColorScheme &scheme = settings->colorScheme();
    QImage icon(size, size, QImage::Format_ARGB32_Premultiplied);
    icon.fill(Qt::transparent);

    QPainter p(&icon);
    p.setRenderHint(QPainter::Antialiasing);

    // page geometry
    qreal pageW = size * 0.54;
    qreal pageH = size * 0.72;
    QRectF page((size - pageW) / 2.0, (size - pageH) / 2.0, pageW, pageH);
    qreal fold = pageW * 0.26;
    qreal radius = pageW * 0.07;

    // nudge the page away from the grid background so the glyph reads clearly
    // on both dark (lighten) and light (darken) themes
    QColor pageFill = scheme.widget;
    qreal pageLum = 0.299 * pageFill.redF() + 0.587 * pageFill.greenF() + 0.114 * pageFill.blueF();
    pageFill = (pageLum < 0.5) ? pageFill.lighter(160) : pageFill.darker(108);
    QColor pageOutline = scheme.widget_border;
    QColor lineColor = scheme.text_lc2.isValid() ? scheme.text_lc2 : scheme.text;
    if(!viewable)
        pageFill.setAlphaF(pageFill.alphaF() * 0.6);

    // page body with the top-right corner cut off
    QPainterPath pagePath;
    pagePath.moveTo(page.left() + radius, page.top());
    pagePath.lineTo(page.right() - fold, page.top());
    pagePath.lineTo(page.right(), page.top() + fold);
    pagePath.lineTo(page.right(), page.bottom() - radius);
    pagePath.quadTo(page.right(), page.bottom(), page.right() - radius, page.bottom());
    pagePath.lineTo(page.left() + radius, page.bottom());
    pagePath.quadTo(page.left(), page.bottom(), page.left(), page.bottom() - radius);
    pagePath.lineTo(page.left(), page.top() + radius);
    pagePath.quadTo(page.left(), page.top(), page.left() + radius, page.top());
    p.setPen(QPen(pageOutline, qMax(1.0, size / 96.0)));
    p.setBrush(pageFill);
    p.drawPath(pagePath);

    // folded corner triangle
    QPainterPath foldPath;
    foldPath.moveTo(page.right() - fold, page.top());
    foldPath.lineTo(page.right() - fold, page.top() + fold);
    foldPath.lineTo(page.right(), page.top() + fold);
    foldPath.closeSubpath();
    QColor foldFill = pageOutline;
    foldFill.setAlphaF(0.65);
    p.setPen(Qt::NoPen);
    p.setBrush(foldFill);
    p.drawPath(foldPath);

    if(viewable) {
        // faux text lines
        QColor faint = lineColor;
        faint.setAlphaF(0.35);
        p.setBrush(faint);
        qreal lineH = qMax(2.0, pageH * 0.045);
        qreal lineGap = pageH * 0.115;
        qreal lineLeft = page.left() + pageW * 0.14;
        qreal top = page.top() + fold + lineGap * 0.4;
        qreal maxRight = page.right() - pageW * 0.14;
        const qreal widths[5] = {0.95, 0.75, 0.9, 0.6, 0.82};
        for(int i = 0; i < 5; i++) {
            qreal y = top + i * lineGap;
            if(y + lineH > page.bottom() - pageH * 0.2)
                break;
            QRectF line(lineLeft, y, (maxRight - lineLeft) * widths[i], lineH);
            p.drawRoundedRect(line, lineH / 2.0, lineH / 2.0);
        }
    } else {
        // big question mark: this file cannot be rendered / viewed
        QFont markFont;
        markFont.setBold(true);
        markFont.setPixelSize(qRound(pageH * 0.42));
        p.setFont(markFont);
        p.setPen(lineColor);
        QRectF markRect = page.adjusted(0, fold * 0.5, 0, 0);
        p.drawText(markRect, Qt::AlignCenter, "?");
    }

    // extension badge pill, slightly overlapping the page bottom
    QString ext = suffix.left(5).toUpper();
    if(!viewable && ext.isEmpty())
        ext = "?";
    if(!ext.isEmpty()) {
        QFont badgeFont;
        badgeFont.setBold(true);
        badgeFont.setPixelSize(qMax(8, qRound(size * 0.11)));
        p.setFont(badgeFont);
        QFontMetricsF fm(badgeFont);
        qreal badgeH = fm.height() * 1.35;
        qreal badgeW = qMax(fm.horizontalAdvance(ext) + badgeH * 0.9, badgeH * 1.6);
        QRectF badge((size - badgeW) / 2.0, page.bottom() - badgeH * 0.62, badgeW, badgeH);
        QColor badgeBg = viewable ? scheme.accent : lineColor;
        p.setPen(Qt::NoPen);
        p.setBrush(badgeBg);
        p.drawRoundedRect(badge, badgeH / 2.0, badgeH / 2.0);
        // pick badge text color by background luminance
        qreal lum = 0.299 * badgeBg.redF() + 0.587 * badgeBg.greenF() + 0.114 * badgeBg.blueF();
        p.setPen(lum > 0.55 ? QColor(25, 25, 25) : QColor(245, 245, 245));
        p.drawText(badge, Qt::AlignCenter, ext);
    }
    p.end();
    return icon;
}

std::pair<std::unique_ptr<QImage>, QSize> ThumbnailerRunnable::createVideoThumbnail(const QString& path, int size, bool squared) {
    QFileInfo fi(path);
    QImageReader reader;
    QString tmpFilePath = settings->tmpDir() + fi.fileName() + ".png";
    QString tmpFilePathEsc = tmpFilePath;
    tmpFilePathEsc.replace("%", "%%");
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(settings->mpvBinary(),
                  QStringList() << "--start=30%"
                                << "--frames=1"
                                << "--aid=no"
                                << "--sid=no"
                                << "--no-config"
                                << "--load-scripts=no"
                                << "--no-terminal"
                                << "--o=" + tmpFilePathEsc
                                << path
                  );
    process.waitForFinished(8000);
    process.close();

    reader.setFileName(tmpFilePath);
    reader.setFormat("png");
    Qt::AspectRatioMode ARMode = squared?
                (Qt::KeepAspectRatioByExpanding):(Qt::KeepAspectRatio);

    // scale & crop
    QSize scaledSize = reader.size().scaled(size, size, ARMode);
    reader.setScaledSize(scaledSize);
    if(squared) {
        QRect clip(0, 0, size, size);
        QRect scaledRect(QPoint(0,0), scaledSize);
        clip.moveCenter(scaledRect.center());
        reader.setScaledClipRect(clip);
    }
    QSize originalSize = reader.size();
    auto result = std::make_unique<QImage>(reader.read());

    // force reader to close file so it can be deleted later
    reader.setFileName("");

    // remove temporary file
    QFile tmpFile(tmpFilePath);
    tmpFile.remove();

    return std::make_pair(std::move(result), originalSize);
}
