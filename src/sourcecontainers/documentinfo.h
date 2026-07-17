#pragma once

#include <QString>
#include <QSize>
#include <QUrl>
#include <QMimeDatabase>
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <cmath>
#include <cstring>
#include "settings.h"

#ifdef USE_EXIV2

#include <exiv2/exiv2.hpp>
#include <iostream>
#include <iomanip>
#include <cassert>

#endif

#include <QImageReader>

enum DocumentType { NONE, STATIC, ANIMATED, VIDEO, TEXT };

class DocumentInfo {
public:
    DocumentInfo(const QString& path);
    ~DocumentInfo();
    
    QString directoryPath() const;
    QString filePath() const;
    QString fileName() const;
    QString baseName() const;
    qint64 fileSize() const;
    DocumentType type() const;
    QMimeType mimeType() const;

    // file extension (guessed from mime-type)
    QString format() const;
    int exifOrientation() const;

    QDateTime lastModified() const;
    void refresh();
    void loadExifTags();
    QMap<QString, QString> getExifTags();
    // full Exif/Iptc/Xmp dump (lazy-loaded)
    QMap<QString, QString> getAllTags();
    // remove all Exif/Iptc/Xmp metadata from the file on disk
    bool stripMetadata();

    static bool isTextDocument(const QMimeType &mimeType, const QString &suffix, const QString &fileName);

    // Fast, extension-based predictor for "can be converted to another image
    // format". The authoritative check stays in the conversion flow
    // (img->type() == STATIC); this only gates the menu without loading files.
    static bool isConvertibleImageFile(const QString &filePath);
    static bool dirContainsConvertibleImage(const QString &dirPath, int maxEntries = 1000);

private:
    QFileInfo fileInfo;
    DocumentType mDocumentType;
    int mOrientation;
    QString mFormat;
    bool exifLoaded;
    bool allTagsLoaded;

    // guesses file type from its contents
    // and sets extension
    void detectFormat();
    void loadExifOrientation();
#ifdef USE_EXIV2
    // reads Exif.Image.Orientation directly; returns false if unavailable
    bool loadExifOrientationExiv2();
#endif
    void loadAllTags();
    bool detectAPNG();
    bool detectAnimatedWebP();
    bool detectAnimatedJxl();
    bool detectAnimatedAvif();
    QMap<QString, QString> exifTags;
    QMap<QString, QString> allTags;
    QMimeType mMimeType;
};
