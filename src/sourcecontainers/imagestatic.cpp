#include "imagestatic.h"
#include <time.h>

#include <utility>
#include "utils/logging.h"

ImageStatic::ImageStatic(QString _path)
    : Image(std::move(_path))
{
    load();
}

ImageStatic::ImageStatic(std::unique_ptr<DocumentInfo> _info)
    : Image(std::move(_info))
{
    load();
}

ImageStatic::~ImageStatic() {
}

//load image data from disk
void ImageStatic::load() {
    if(isLoaded()) {
        return;
    }
    if(mDocInfo->mimeType().name() == "image/vnd.microsoft.icon")
        loadICO();
    else
        loadGeneric();
}


void ImageStatic::loadGeneric() {
    /* QImageReader::read() seems more reliable than just reading via QImage.
     * For example: "Invalid JPEG file structure: two SOF markers"
     * QImageReader::read() returns false, but still reads an image. Meanwhile QImage just fails.
     * I havent checked qimage's code, but it seems like it sees an exception
     * from libjpeg or whatever and just gives up on reading the file.
     *
     * tldr: qimage bad
     */
    QImageReader r(mPath, mDocInfo->format().toStdString().c_str());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    r.setAllocationLimit(settings->memoryAllocationLimit());
#endif
    QImage *tmp = new QImage();
    r.read(tmp);
    std::unique_ptr<const QImage> img(tmp);
    img = ImageLib::exifRotated(std::move(img), mDocInfo.get()->exifOrientation());
    // scaling this format via qt results in transparent background
    // it rare enough so lets just convert it to the closest working thing
    if(img->format() == QImage::Format_Mono) {
        QImage *imgConverted = new QImage();
        *imgConverted = img->convertToFormat(QImage::Format_Grayscale8);
        image.reset(imgConverted);
    } else {
        // set image
        image = std::move(img);
    }
    mLoaded = true;
}

// TODO: move this out somewhere to use in other places
void ImageStatic::loadICO() {
    // Big brain code. It's mostly for small ico files so whatever. I'm not patching Qt for this.
    QIcon icon(mPath);
    QList<QSize> sizes = icon.availableSizes();
    QSize maxSize(0, 0);
    for(auto sz : sizes)
        if(maxSize.width() < sz.width())
            maxSize = sz;
    QPixmap iconPix = icon.pixmap(maxSize);
    std::unique_ptr<const QImage> img(new QImage(iconPix.toImage()));
    image = std::move(img);
    mLoaded = true;
}

std::optional<ImageWriteJob> ImageStatic::writeJob(const QString &destPath) {
    const QString ext = QFileInfo(destPath).suffix();
    // The single choke point for every write route (save, save-as, convert), so
    // a format that cannot faithfully encode this image is refused here once
    // rather than in each caller.
    if(!DocumentInfo::canEncodeToFormat(ext, size())) {
        qCWarning(logLoader) << "ImageStatic::writeJob() -" << ext
                             << "cannot encode an image this small:" << destPath;
        return std::nullopt;
    }
    ImageWriteJob job;
    job.image = *getImage();
    job.destPath = destPath;
    job.format = ext.toLower().toLatin1();
    job.quality = ImageWriter::qualityFor(ext);
    job.metadataSource = mPath;
    return job;
}

void ImageStatic::onWritten(const ImageWriteJob &job) {
    // QImage copies share their data, so the cache key tells whether the edit
    // that was written is still the current one.
    if(isEdited() && imageEdited->cacheKey() == job.image.cacheKey()) {
        // the edit is on disk now: it becomes the clean image
        image = imageEdited;
        discardEditedImage();
    } else if(job.destPath == mPath && image->cacheKey() != job.image.cacheKey()) {
        // edited again while the write ran: what is on disk is the older
        // edit, so it becomes the clean image under the newer one
        image = std::make_shared<const QImage>(job.image);
    }
    if(job.destPath == mPath)
        mDocInfo->refresh();
}

bool ImageStatic::save(QString destPath) {
    auto job = writeJob(destPath);
    if(!job || !ImageWriter::write(*job))
        return false;
    onWritten(*job);
    return true;
}

bool ImageStatic::save() {
    return save(mPath);
}

std::unique_ptr<QPixmap> ImageStatic::getPixmap() {
    std::unique_ptr<QPixmap> pix(new QPixmap());
    isEdited()?pix->convertFromImage(*imageEdited):pix->convertFromImage(*image, Qt::NoFormatConversion);
    return pix;
}

std::shared_ptr<const QImage> ImageStatic::getSourceImage() {
    return image;
}

std::shared_ptr<const QImage> ImageStatic::getImage() {
    return isEdited()?imageEdited:image;
}

int ImageStatic::height() {
    return isEdited()?imageEdited->height():image->height();
}

int ImageStatic::width() {
    return isEdited()?imageEdited->width():image->width();
}

QSize ImageStatic::size() {
    return isEdited()?imageEdited->size():image->size();
}

bool ImageStatic::setEditedImage(std::unique_ptr<const QImage> imageEditedNew) {
    if(imageEditedNew && imageEditedNew->width() != 0) {
        discardEditedImage();
        imageEdited = std::move(imageEditedNew);
        mEdited = true;
        return true;
    }
    return false;
}

bool ImageStatic::discardEditedImage() {
    if(imageEdited) {
        imageEdited.reset();
        mEdited = false;
        return true;
    }
    return false;
}
