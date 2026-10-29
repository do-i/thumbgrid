// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#include "imagewriter.h"

#include <QBuffer>
#include <QImageWriter>
#include <QSaveFile>
#include "settings.h"
#include "utils/logging.h"

#ifdef USE_EXIV2
#include <exiv2/exiv2.hpp>
#endif

namespace {

#ifdef USE_EXIV2
// Qt's encoders write no metadata: copy the source's Exif/Iptc/Xmp, best-effort.
void attachMetadata(QByteArray &bytes, const ImageWriteJob &job) {
    try {
        auto src = Exiv2::ImageFactory::open(job.metadataSource.toStdString());
        if(!src.get())
            return;
        src->readMetadata();
        Exiv2::ExifData exif = src->exifData();
        if(exif.empty() && src->iptcData().empty() && src->xmpData().empty())
            return;
        auto dst = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte *>(bytes.constData()),
                                             static_cast<size_t>(bytes.size()));
        if(!dst.get())
            return;
        // pixels are already upright (rotation baked on load)
        exif["Exif.Image.Orientation"]     = uint16_t(1);
        exif["Exif.Photo.PixelXDimension"] = uint32_t(job.image.width());
        exif["Exif.Photo.PixelYDimension"] = uint32_t(job.image.height());
        dst->setExifData(exif);
        dst->setIptcData(src->iptcData());
        dst->setXmpData(src->xmpData());
        dst->writeMetadata();
        Exiv2::BasicIo &io = dst->io();
        if(io.open() != 0)
            return;
        const Exiv2::DataBuf out = io.read(io.size());
        io.close();
        if(out.size() != 0)
            bytes = QByteArray(reinterpret_cast<const char *>(out.c_data()), static_cast<qsizetype>(out.size()));
    } catch(...) {
        qCWarning(logLoader) << "ImageWriter - could not carry metadata over to" << job.destPath;
    }
}
#endif

} // namespace

int ImageWriter::qualityFor(const QString &suffix) {
    // zlib levels 3-6 compress PNG about as well as 9 for far less work; Qt maps quality 66 to level 3
    // (~3.5x faster than 6 on screenshots, 2-5% larger)
    if(suffix.compare("png", Qt::CaseInsensitive) == 0)
        return 66;
    if(suffix.compare("jpg", Qt::CaseInsensitive) == 0 || suffix.compare("jpeg", Qt::CaseInsensitive) == 0)
        return settings->JPEGSaveQuality();
    return 95;
}

bool ImageWriter::write(const ImageWriteJob &job) {
    QByteArray bytes;
    {
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        QImageWriter writer(&buffer, job.format);
        writer.setQuality(job.quality);
        if(!writer.write(job.image)) {
            qCWarning(logLoader) << "ImageWriter - could not encode" << job.destPath << writer.errorString();
            return false;
        }
    }
#ifdef USE_EXIV2
    if(!job.metadataSource.isEmpty())
        attachMetadata(bytes, job);
#endif
    QSaveFile file(job.destPath);
    if(!file.open(QIODevice::WriteOnly)) {
        qCWarning(logLoader) << "ImageWriter - could not open" << job.destPath << file.errorString();
        return false;
    }
    if(file.write(bytes) != bytes.size()) {
        qCWarning(logLoader) << "ImageWriter - could not write" << job.destPath << file.errorString();
        file.cancelWriting();
        return false;
    }
    if(!file.commit()) {
        qCWarning(logLoader) << "ImageWriter - could not replace" << job.destPath << file.errorString();
        return false;
    }
    return true;
}
