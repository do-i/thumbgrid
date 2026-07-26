#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QSize>
#include <QUrl>
#include <QMimeDatabase>
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <cmath>
#include <cstring>
#include <cstdint>
#include "settings.h"

#ifdef USE_EXIV2

#include <exiv2/exiv2.hpp>
#include <iostream>
#include <iomanip>
#include <cassert>

#endif

#include <QImageReader>

enum DocumentType : std::uint8_t { NONE, STATIC, ANIMATED, VIDEO, TEXT };

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

    // --- editable metadata (docs: tier 1 - text-valued Exif only) ------------
    //
    // The four keys below are the ones a person realistically retypes, and all
    // four are text: no Rational, no enum, no byte array. That is the whole
    // reason the editable set is a fixed list rather than "whatever the EXIF tab
    // is showing" - getAllTags() returns exiv2's *interpreted* strings ("F2.8",
    // "top, left"), which cannot be written back, and getExifTags() keys its map
    // by translated display labels, losing the exiv2 key entirely.
    static QStringList editableTagKeys();
    // Human-readable label for an editable key, for the edit form.
    static QString editableTagLabel(const QString &key);
    // Exiv2 keys can only be written to formats exiv2 can safely rewrite; this
    // gates the UI so the editor is never offered for a file it cannot save.
    static bool supportsMetadataEditing(const QString &filePath);

    // Raw (uninterpreted) current values for editableTagKeys(), keyed by exiv2
    // key. Missing tags are absent from the map rather than empty-valued.
    // UserComment is returned without its "charset=" prefix.
    QMap<QString, QString> getEditableTags();
    // Writes values back in one open/write cycle. An empty value deletes the
    // tag. Keys outside editableTagKeys() are ignored. Returns false and writes
    // nothing if the file cannot be opened or a value is rejected.
    bool setEditableTags(const QMap<QString, QString> &values);
    // "YYYY:MM:DD HH:MM:SS", the only form Exif.Image.DateTime may hold.
    static bool isValidExifDateTime(const QString &value);

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
