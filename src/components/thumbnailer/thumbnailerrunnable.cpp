#include "thumbnailerrunnable.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>

ThumbnailerRunnable::ThumbnailerRunnable(ThumbnailCache* _cache, QString _path, int _size, bool _crop, bool _force) :
    path(_path),
    size(_size),
    crop(_crop),
    force(_force),
    cache(_cache)
{
}

ThumbnailerRunnable::ThumbnailerRunnable(ThumbnailCache* _cache, QString _path, int _size, bool _crop, bool _force,
                                         bool _previewFit, bool _showHidden, QImage _iconBase, QString _colorId) :
    path(_path),
    size(_size),
    crop(_crop),
    force(_force),
    cache(_cache),
    isDir(true),
    previewFit(_previewFit),
    showHidden(_showHidden),
    iconBase(_iconBase),
    colorId(_colorId)
{
}

void ThumbnailerRunnable::run() {
    emit taskStart(path, size);
    if(isDir) {
        std::shared_ptr<Thumbnail> thumbnail = generateDir(cache, path, size, crop, force, previewFit, showHidden, iconBase, colorId);
        emit dirTaskEnd(thumbnail, path);
    } else {
        std::shared_ptr<Thumbnail> thumbnail = generate(cache, path, size, crop, force);
        emit taskEnd(thumbnail, path);
    }
}

QString ThumbnailerRunnable::generateIdString(QString path, int size, bool crop) {
    QString queryStr = path + QString::number(size);
    if(crop)
        queryStr.append("s");
    queryStr = QString("%1").arg(QString(QCryptographicHash::hash(queryStr.toUtf8(),QCryptographicHash::Md5).toHex()));
    return queryStr;
}

std::shared_ptr<Thumbnail> ThumbnailerRunnable::generate(ThumbnailCache* cache, QString path, int size, bool crop, bool force) {
    DocumentInfo imgInfo(path);
    QString thumbnailId = generateIdString(path, size, crop);
    std::unique_ptr<QImage> image;

    QString time = QString::number(imgInfo.lastModified().toMSecsSinceEpoch());

    if(!force && cache) {
        image.reset(cache->readThumbnail(thumbnailId));
        if(image && image->text("lastModified") != time)
            image.reset(nullptr);
    }

    if(!image) {
        if(imgInfo.type() == DocumentType::NONE) {
            std::shared_ptr<Thumbnail> thumbnail(new Thumbnail(imgInfo.fileName(), "", size, nullptr));
            return thumbnail;
        }
        std::pair<QImage*, QSize> pair;
        if(imgInfo.type() == VIDEO)
            pair = createVideoThumbnail(path, size, crop);
        else
            pair = createThumbnail(imgInfo.filePath(), imgInfo.format().toStdString().c_str(), size, crop);
        image.reset(pair.first);
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

std::pair<QImage*, QSize> ThumbnailerRunnable::createThumbnail(QString path, const char *format, int size, bool squared) {
    QImageReader *reader = new QImageReader(path, format);
    Qt::AspectRatioMode ARMode = squared?
                (Qt::KeepAspectRatioByExpanding):(Qt::KeepAspectRatio);
    QImage *result = nullptr;
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
        result = new QImage();
        if(!reader->read(result)) {
            // If read() returns false there's no guarantee that size conversion worked properly.
            // So we fallback to manual.
            // Se far I've seen this happen only on some weird (corrupted?) jpeg saved from camera
            manualResize = true;
            delete result;
            result = nullptr;
            // Force reset reader because it is really finicky
            // and can fail on the second read attempt (yeah wtf)
            reader->setFileName("");
            delete reader;
            reader = new QImageReader(path, format);
        }
    }
    if(manualResize) { // manual resize & crop. slower but should just work
        QImage *fullSize = new QImage();
        reader->read(fullSize);
        if(indexed) {
            auto newFmt = QImage::Format_RGB32;
            if(fullSize->hasAlphaChannel())
                newFmt = QImage::Format_ARGB32;
            auto tmp = new QImage(fullSize->convertToFormat(newFmt));
            delete fullSize;
            fullSize = tmp;
        }
        originalSize = fullSize->size();
        QSize scaledSize = fullSize->size().scaled(size, size, ARMode);
        if(squared) {
            QRect clip(0, 0, size, size);
            QRect scaledRect(QPoint(0,0), scaledSize);
            clip.moveCenter(scaledRect.center());
            QImage scaled = QImage(fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            result = ImageLib::croppedRaw(&scaled, clip);
        } else {
            result = new QImage(fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
        delete fullSize;
    }
    // force reader to close file so it can be deleted later
    reader->setFileName("");
    delete reader;
    return std::make_pair(result, originalSize);
}

std::shared_ptr<Thumbnail> ThumbnailerRunnable::generateDir(ThumbnailCache *cache, QString path, int size, bool crop, bool force,
                                                            bool previewFit, bool showHidden, const QImage &iconBase, const QString &colorId) {
    QString name = QFileInfo(path).fileName();
    QString thumbnailId = generateDirIdString(path, size, previewFit, colorId);
    // directory mtime changes when direct children are added/removed/renamed,
    // which is exactly what changes the preview set
    QString time = QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());

    std::unique_ptr<QImage> composite;
    if(!force && cache) {
        composite.reset(cache->readThumbnail(thumbnailId));
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

// Cache key includes the preview-fit mode and the scheme icon color, so toggling
// either regenerates rather than serving a stale composite.
QString ThumbnailerRunnable::generateDirIdString(QString path, int size, bool previewFit, const QString &colorId) {
    QString queryStr = path + QString::number(size) + "dir";
    if(previewFit)
        queryStr.append("f");
    queryStr.append(colorId);
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

        // best-effort: reuse an already-cached per-file thumbnail (same size+crop as
        // the grid) as the preview tile, avoiding a fresh decode. Falls back below.
        if(cache) {
            QString childId = generateIdString(entry.absoluteFilePath(), targetSize, crop);
            std::unique_ptr<QImage> cached(cache->readThumbnail(childId));
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

std::pair<QImage*, QSize> ThumbnailerRunnable::createVideoThumbnail(QString path, int size, bool squared) {
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
    QImage *result = nullptr;

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
    result = new QImage(reader.read());

    // force reader to close file so it can be deleted later
    reader.setFileName("");

    // remove temporary file
    QFile tmpFile(tmpFilePath);
    tmpFile.remove();

    return std::make_pair(result, originalSize);
}
