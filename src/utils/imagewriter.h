// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

// Everything one image write needs, captured up front on the GUI thread so
// that ImageWriter::write() touches no shared state and can run on a worker.
struct ImageWriteJob {
    QImage image;           // pixels to encode; an implicitly shared snapshot
    QString destPath;
    QByteArray format;      // writer format, from the destination suffix
    int quality = -1;
    QString metadataSource; // file whose Exif/Iptc/Xmp is carried over; empty for none
};

namespace ImageWriter {

// Encoder quality used when writing a file with this suffix. Reads settings,
// so it belongs to the GUI thread.
int qualityFor(const QString &suffix);

// Encodes job.image and attaches the source metadata in memory, then replaces
// job.destPath atomically (QSaveFile: a temporary sibling, synced and renamed
// over it). Any failure leaves an existing destination untouched. Thread-safe.
bool write(const ImageWriteJob &job);

}
