#include "imagestatic.h"
#include "utils/safesave.h"
#include <time.h>

#include <utility>

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

// TODO: move saving to directorymodel
bool ImageStatic::save(QString destPath) {
    QFileInfo fi(destPath);
    QString ext = fi.suffix();
    // png compression note from libpng
    // Note that tests have shown that zlib compression levels 3-6 usually perform as well
    // as level 9 for PNG images, and do considerably fewer caclulations
    int quality = 95;
    if(ext.compare("png", Qt::CaseInsensitive) == 0)
        quality = 30;
    else if(ext.compare("jpg", Qt::CaseInsensitive) == 0 || ext.compare("jpeg", Qt::CaseInsensitive) == 0)
        quality = settings->JPEGSaveQuality();

#ifdef USE_EXIV2
    // Qt's QImage::save() drops all metadata, so grab the source file's
    // Exif/Iptc/Xmp now (before mPath may be overwritten) to re-attach later.
    Exiv2::ExifData srcExif;
    Exiv2::IptcData srcIptc;
    Exiv2::XmpData  srcXmp;
    bool haveSrcMeta = false;
    try {
        auto srcImage = Exiv2::ImageFactory::open(toStdString(mPath));
        if(srcImage.get()) {
            srcImage->readMetadata();
            srcExif = srcImage->exifData();
            srcIptc = srcImage->iptcData();
            srcXmp  = srcImage->xmpData();
            haveSrcMeta = !srcExif.empty() || !srcIptc.empty() || !srcXmp.empty();
        }
    } catch(...) {
        haveSrcMeta = false;
    }
#endif

    // save file
    bool success = SafeSave::withBackup(destPath, mDocInfo->filePath(), [&]() {
        if(isEdited()) {
            bool ok = imageEdited->save(destPath, ext.toStdString().c_str(), quality);
            image.swap(imageEdited);
            discardEditedImage();
            return ok;
        }
        return image->save(destPath, ext.toStdString().c_str(), quality);
    });

#ifdef USE_EXIV2
    // Re-attach the source metadata that QImage::save() stripped. Best-effort:
    // any failure (unsupported format, etc.) leaves the pixel save intact.
    if(success && haveSrcMeta) {
        try {
            auto dstImage = Exiv2::ImageFactory::open(toStdString(destPath));
            if(dstImage.get()) {
                // thumbgrid bakes exif rotation into pixels on load, so the saved
                // pixels are always upright - normalize orientation/dimensions
                // to avoid viewers rotating an already-upright image.
                srcExif["Exif.Image.Orientation"]     = uint16_t(1);
                srcExif["Exif.Photo.PixelXDimension"] = uint32_t(image->width());
                srcExif["Exif.Photo.PixelYDimension"] = uint32_t(image->height());
                dstImage->setExifData(srcExif);
                dstImage->setIptcData(srcIptc);
                dstImage->setXmpData(srcXmp);
                dstImage->writeMetadata();
            }
        } catch(...) {
            qDebug() << "ImageStatic::save() - could not write metadata to" << destPath;
        }
    }
#endif

    if(destPath == mPath && success)
        mDocInfo->refresh();
    return success;
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
