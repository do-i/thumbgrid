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

    // --- XMP / ICC (read-only) ----------------------------------------------
    //
    // Both predicates answer "can this *format* carry this metadata kind",
    // which is deliberately broader than supportsMetadataEditing(): these tabs
    // never write, so the narrower "exiv2 can safely rewrite this" test would
    // hide readable data for no reason (a png carries both, and can be edited
    // by neither).
    static bool supportsXmp(const QString &filePath);
    static bool supportsIccProfile(const QString &filePath);

    // Xmp properties only, keyed by exiv2 key ("Xmp.dc.title"), values the
    // interpreted stream form - the same shape getAllTags() produces for its
    // Xmp section, just without the Exif/Iptc entries. Lazy-loaded and cached.
    QMap<QString, QString> getXmpTags();

    // Derived, human-readable summary of the embedded ICC profile - not a raw
    // dump. Empty when the file carries no profile (or the blob is one Qt
    // cannot parse). Ordered rather than a QMap because the four rows read as
    // a description that goes from "what is it" to "how big is it"; sorting
    // them by key would interleave "Primaries" before "Profile" and split
    // "Profile" from "Profile size".
    QList<QPair<QString, QString>> getIccProfileInfo();

    // --- XMP / ICC writing (docs/2026-08-01-001) ----------------------------
    //
    // Three separate write gates, because the three metadata kinds are not
    // writable in the same set of containers. tiff is outside all of them: its
    // ICC profile *is* an Exif tag (Exif.Image.InterColorProfile), and exiv2
    // must rewrite a tiff's structural Exif tags to emit a valid file at all,
    // so neither "remove all Exif" nor "remove the profile" can be scoped there.
    static bool supportsXmpEditing(const QString &filePath);
    static bool supportsIccEditing(const QString &filePath);

    // An XMP property's declared type. The editor picks a widget from it and
    // the writer picks assign-vs-erase-then-add: assignment replaces a Text or
    // LangAlt value in place, but *appends* to an array, so Seq and Bag have to
    // be erased before they are rewritten.
    enum class XmpValueKind : std::uint8_t { Text, LangAlt, Seq, Bag };

    // The curated editable set - a fixed list for the same reason
    // editableTagKeys() is one: each key's type has to be known here, and the
    // displayed value of an array is lossy (exiv2 joins bag items with ", ",
    // and an item may itself contain a comma).
    static QStringList editableXmpKeys();
    static QString editableXmpLabel(const QString &key);
    static XmpValueKind xmpValueKind(const QString &key);

    // Uniform shape across kinds: Text and LangAlt come back as a one-element
    // list (absent keys are absent from the map), Seq and Bag as their items in
    // order. There is deliberately no joined string to re-split.
    QMap<QString, QStringList> getEditableXmpTags();
    // An empty list erases the key. LangAlt is written as x-default only, and
    // cleared by assigning an empty x-default so other languages survive.
    bool setEditableXmpTags(const QMap<QString, QStringList> &values);

    // True when exiv2 knows the property's schema (XmpProperties::propertyInfo
    // returns non-null). Drives the standard/custom split in the UI: a standard
    // property carries a declared type a text box would corrupt, while a custom
    // one is always XmpText and so is safe to edit as free text.
    static bool isRegisteredXmpKey(const QString &key);

    // Properties in namespaces exiv2 does not know. Always XmpText, so a flat
    // QString is the whole value. Readable even for a namespace this process
    // never registered - the prefix binding travels in the packet itself.
    QMap<QString, QString> getCustomXmpTags();
    // An empty value keeps the key with an empty value (legal, and stable
    // across later writes); erasing is what removes the property. Keys whose
    // prefix is unknown are rejected rather than attempted.
    bool setCustomXmpTags(const QMap<QString, QString> &values);
    bool eraseXmpKey(const QString &key);
    // Registers the namespace when prefix is new, then writes. Main thread only:
    // registerNs() mutates the same global registry XmpParser::initialize() sets
    // up at startup.
    bool addCustomXmpProperty(const QString &prefix, const QString &name,
                              const QString &namespaceUri, const QString &value);
    // True when the prefix already resolves, i.e. no namespace URI is needed.
    static bool isKnownXmpPrefix(const QString &prefix);

    // Scoped removals. Each leaves the other two kinds untouched - verified by
    // probe, and the reason the three buttons can sit in three tabs.
    bool clearExifMetadata();
    bool clearXmpMetadata();
    bool clearIccProfile();

    // Cheap "is there anything here to remove", for deciding whether a
    // destructive button is shown at all.
    bool hasExifMetadata();
    bool hasXmpMetadata();
    bool hasIccProfile();

    // Defensive limits (docs/2026-08-01-001 §8). Constants rather than
    // settings: a configurable cap is an invitation to raise it and meet the
    // pathology it exists to prevent. None is reachable by intentional use -
    // keywords are items inside one Xmp.dc.subject bag, not one property each.
    static constexpr int kMaxCustomXmpProperties = 256;
    static constexpr int kMaxXmpKeyLength = 256;
    static constexpr int kMaxXmpValueLengthEdited = 4096;

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
    bool xmpLoaded;

    // guesses file type from its contents
    // and sets extension
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
