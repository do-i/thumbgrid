#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QPair>
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

    // --- XMP / ICC (read-only) ---
    // Can the format carry this kind at all; broader than supportsMetadataEditing() (png has both).
    static bool supportsXmp(const QString &filePath);
    static bool supportsIccProfile(const QString &filePath);

    // Xmp properties keyed by exiv2 key, interpreted values. Lazy, cached.
    QMap<QString, QString> getXmpTags();

    // Human-readable summary of the embedded ICC profile, in descriptive order; empty without one.
    QList<QPair<QString, QString>> getIccProfileInfo();

    // --- XMP / ICC writing (verified against exiv2 0.28.8) ---
    // Separate gates per kind; tiff is excluded (its ICC profile is an Exif tag exiv2 must rewrite).
    static bool supportsXmpEditing(const QString &filePath);
    static bool supportsIccEditing(const QString &filePath);

    // Declared type: picks the widget, and Seq/Bag must be erased before rewriting (assignment appends).
    enum class XmpValueKind : std::uint8_t { Text, LangAlt, Seq, Bag };

    // Curated editable set: types must be known, and joined array values are lossy.
    static QStringList editableXmpKeys();
    static QString editableXmpLabel(const QString &key);
    static XmpValueKind xmpValueKind(const QString &key);

    // Text/LangAlt as one-element lists, Seq/Bag as items in order; absent keys are absent.
    QMap<QString, QStringList> getEditableXmpTags();
    // Empty list erases. LangAlt writes x-default only; clearing it keeps other languages.
    bool setEditableXmpTags(const QMap<QString, QStringList> &values);

    // exiv2 knows the schema; such properties have declared types a text box would corrupt.
    static bool isRegisteredXmpKey(const QString &key);

    // Properties in unknown namespaces (always XmpText); readable without registering the prefix.
    QMap<QString, QString> getCustomXmpTags();
    // Empty value keeps the key; unknown prefixes are rejected.
    bool setCustomXmpTags(const QMap<QString, QString> &values);
    bool eraseXmpKey(const QString &key);
    // Registers a new namespace, then writes. Main thread only (registerNs() mutates a global registry).
    bool addCustomXmpProperty(const QString &prefix, const QString &name,
                              const QString &namespaceUri, const QString &value);
    // True when the prefix already resolves, i.e. no namespace URI is needed.
    static bool isKnownXmpPrefix(const QString &prefix);

    // Scoped removals; each leaves the other kinds untouched.
    bool clearExifMetadata();
    bool clearXmpMetadata();
    bool clearIccProfile();

    // Cheap check whether a destructive button has anything to remove.
    bool hasExifMetadata();
    bool hasXmpMetadata();
    bool hasIccProfile();

    // Defensive limits, deliberately not settings; unreachable by intentional use.
    static constexpr int kMaxCustomXmpProperties = 256;
    static constexpr int kMaxXmpKeyLength = 256;
    static constexpr int kMaxXmpValueLengthEdited = 4096;

    // --- editable metadata (text-valued Exif only) ---
    // A fixed list: interpreted strings ("F2.8") and translated labels cannot be written back.
    static QStringList editableTagKeys();
    // Human-readable label for an editable key, for the edit form.
    static QString editableTagLabel(const QString &key);
    // Formats exiv2 can safely rewrite; gates the tag editor.
    static bool supportsMetadataEditing(const QString &filePath);
    // Formats exiv2 can read Exif from: adds the heif family (read-only).
    static bool supportsExif(const QString &filePath);
    // Can exiv2 rewrite this container at all; gates the removal actions.
    static bool supportsMetadataWriting(const QString &filePath);
    // False for heif below the size the encoder silently garbles; true for other formats.
    static bool canEncodeToFormat(const QString &format, const QSize &size);

    // Raw values for editableTagKeys(); missing tags absent; UserComment without "charset=".
    QMap<QString, QString> getEditableTags();
    // One open/write cycle. Empty deletes; unknown keys ignored; false (nothing written) on any rejection.
    bool setEditableTags(const QMap<QString, QString> &values);
    // "YYYY:MM:DD HH:MM:SS", the only form Exif.Image.DateTime may hold.
    static bool isValidExifDateTime(const QString &value);

    static bool isTextDocument(const QMimeType &mimeType, const QString &suffix, const QString &fileName);

    // Extension-based menu gate; the conversion flow checks img->type() == STATIC.
    static bool isConvertibleImageFile(const QString &filePath);
    static bool dirContainsConvertibleImage(const QString &dirPath, int maxEntries = 1000);

private:
    QFileInfo fileInfo;
    DocumentType mDocumentType;
    int mOrientation;
    QString mFormat;
    bool exifLoaded;
    bool allTagsLoaded;
    bool xmpLoaded;

    void detectFormat();
    void loadExifOrientation();
#ifdef USE_EXIV2
    // reads Exif.Image.Orientation directly; returns false if unavailable
    bool loadExifOrientationExiv2();
#endif
    void loadAllTags();
    void loadXmpTags();
    bool detectAPNG();
    bool detectAnimatedWebP();
    bool detectAnimatedJxl();
    bool detectAnimatedAvif();
    QMap<QString, QString> exifTags;
    QMap<QString, QString> allTags;
    QMap<QString, QString> xmpTags;
    QMimeType mMimeType;
};
