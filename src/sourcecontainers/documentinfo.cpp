#include "documentinfo.h"
#include "utils/logging.h"
#include "utils/pathstring.h"
#include <QByteArray>
#include <QColorSpace>
#include <QDirIterator>
#include <algorithm>

DocumentInfo::DocumentInfo(const QString& path)
    : mDocumentType(DocumentType::NONE),
      mOrientation(0),
      mFormat(""),
      exifLoaded(false),
      allTagsLoaded(false),
      xmpLoaded(false)
{
    fileInfo.setFile(path);
    if(!fileInfo.isFile()) {
        qCWarning(logLoader) << "FileInfo: cannot open: " << path;
        return;
    }
    detectFormat();
}

DocumentInfo::~DocumentInfo() {
}

// ##############################################################
// ####################### PUBLIC METHODS #######################
// ##############################################################

QString DocumentInfo::directoryPath() const {
    return fileInfo.absolutePath();
}

QString DocumentInfo::filePath() const {
    return fileInfo.absoluteFilePath();
}

QString DocumentInfo::fileName() const {
    return fileInfo.fileName();
}

QString DocumentInfo::baseName() const {
    return fileInfo.baseName();
}

// bytes
qint64 DocumentInfo::fileSize() const {
    return fileInfo.size();
}

DocumentType DocumentInfo::type() const {
    return mDocumentType;
}

QMimeType DocumentInfo::mimeType() const {
    return mMimeType;
}

QString DocumentInfo::format() const {
    return mFormat;
}

QDateTime DocumentInfo::lastModified() const {
    return fileInfo.lastModified();
}

// For cases like orientation / even mimetype change we just reload
// Image from scratch, so don`t bother handling it here
void DocumentInfo::refresh() {
    fileInfo.refresh();
}

int DocumentInfo::exifOrientation() const {
    return mOrientation;
}

// ##############################################################
// ####################### PRIVATE METHODS ######################
// ##############################################################
void DocumentInfo::detectFormat() {
    if(mDocumentType != DocumentType::NONE)
        return;
    QMimeDatabase mimeDb;
    mMimeType = mimeDb.mimeTypeForFile(fileInfo.filePath(), QMimeDatabase::MatchContent);
    auto mimeName = mMimeType.name().toUtf8();
    auto suffix = fileInfo.suffix().toLower().toUtf8();
    if(mimeName == "image/jpeg") {
        mFormat = "jpg";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/png") {
        if(QImageReader::supportedImageFormats().contains("apng") && detectAPNG()) {
            mFormat = "apng";
            mDocumentType = DocumentType::ANIMATED;
        } else {
            mFormat = "png";
            mDocumentType = DocumentType::STATIC;
        }
    } else if(mimeName == "image/gif") {
        mFormat = "gif";
        mDocumentType = DocumentType::ANIMATED;
    } else if(mimeName == "image/webp" || (mimeName == "audio/x-riff" && suffix == "webp")) {
        mFormat = "webp";
        mDocumentType = detectAnimatedWebP() ? DocumentType::ANIMATED : DocumentType::STATIC;
    } else if(mimeName == "image/jxl") {
        mFormat = "jxl";
        mDocumentType = detectAnimatedJxl() ? DocumentType::ANIMATED : DocumentType::STATIC;
        if(mDocumentType == DocumentType::ANIMATED && !settings->jxlAnimation()) {
            mDocumentType = DocumentType::NONE;
            qCDebug(logLoader) << "animated jxl is off; skipping file";
        }
    } else if(mimeName == "image/avif") {
        mFormat = "avif";
        mDocumentType = detectAnimatedAvif() ? DocumentType::ANIMATED : DocumentType::STATIC;
    } else if(mimeName == "image/heif" || mimeName == "image/heic") {
        // One container, several extensions: heic/heif/hif for hevc-coded files,
        // avci for avc-coded ones. The decoder plugin registers a separate key
        // per extension, and QImageReader given an explicit format does not fall
        // back to sniffing, so hand it the file's own suffix when that is a key
        // it knows and the generic one otherwise.
        static const QSet<QByteArray> heifKeys = {"heic", "heif", "hif", "avci", "hej2"};
        mFormat = QString::fromUtf8(heifKeys.contains(suffix) ? suffix : QByteArrayLiteral("heif"));
        // Unlike the formats above, heif decoding is never built into Qt itself -
        // it always comes from an optional plugin. Without one the read yields a
        // blank image, so mark the file unviewable instead and let the grid show
        // it as a file-type icon with a clear message on activation.
        mDocumentType = QImageReader::supportedImageFormats().contains(mFormat.toUtf8())
                            ? DocumentType::STATIC : DocumentType::NONE;
    } else if(mimeName == "image/bmp") {
        mFormat = "bmp";
        mDocumentType = DocumentType::STATIC;
    } else if(settings->videoPlayback() && settings->videoFormats().contains(mimeName)) {
        mDocumentType = DocumentType::VIDEO;
        mFormat = settings->videoFormats().value(mimeName);
    } else {
        // just try to open via suffix if all of the above fails
        mFormat = suffix;
        if(mFormat.compare("jfif", Qt::CaseInsensitive) == 0)
            mFormat = "jpg";
        if(settings->videoPlayback() && settings->videoFormats().values().contains(suffix))
            mDocumentType = DocumentType::VIDEO;
        else if(mimeName.startsWith("image/") || QImageReader::supportedImageFormats().contains(suffix))
            mDocumentType = DocumentType::STATIC;
        else if(isTextDocument(mMimeType, QString::fromUtf8(suffix), fileInfo.fileName()))
            mDocumentType = DocumentType::TEXT;
        else
            mDocumentType = DocumentType::NONE; // not viewable by thumbgrid
    }
    loadExifOrientation();
}

// Anything we are willing to open in the text viewer. Content-based mime detection
// covers most cases (anything derived from text/plain); the suffix and file name
// sets catch text-like files that ship with unregistered or generic mime types.
bool DocumentInfo::isTextDocument(const QMimeType &mimeType, const QString &suffix, const QString &fileName) {
    if(mimeType.inherits("text/plain"))
        return true;
    static const QSet<QString> textSuffixes = {
        "txt", "md", "markdown", "rst", "adoc", "org",
        "xml", "json", "yaml", "yml", "toml", "ini", "conf", "cfg", "properties",
        "csv", "tsv", "log",
        "py", "java", "c", "h", "cpp", "hpp", "cc", "hh", "cxx", "hxx",
        "js", "mjs", "ts", "jsx", "tsx", "css", "scss", "html", "htm", "xhtml", "svg",
        "sh", "bash", "zsh", "fish", "bat", "cmd", "ps1",
        "rs", "go", "rb", "php", "pl", "pm", "lua", "sql", "kt", "kts", "swift",
        "cmake", "gradle", "tex", "srt", "vtt", "desktop", "service", "patch", "diff"
    };
    if(textSuffixes.contains(suffix.toLower()))
        return true;
    static const QSet<QString> textFileNames = {
        "makefile", "gnumakefile", "dockerfile", "cmakelists.txt", "kconfig",
        "license", "copying", "notice", "readme", "changelog", "authors", "todo", "install", "news"
    };
    return textFileNames.contains(fileName.toLower());
}

// Extension-based best-effort predictor used to gate "Convert to...". gif and
// apng are practically always animated (conversion only accepts STATIC images),
// and pdf is excluded app-wide, so they are rejected here even though Qt may
// list them. The authoritative STATIC check stays in convertToFormat.
bool DocumentInfo::isConvertibleImageFile(const QString &filePath) {
    QString suffix = QFileInfo(filePath).suffix().toLower();
    if(suffix.isEmpty())
        return false;
    if(suffix == "jfif")
        return true;
    if(suffix == "gif" || suffix == "apng" || suffix == "pdf")
        return false;
    return QImageReader::supportedImageFormats().contains(suffix.toUtf8());
}

// Shallow (non-recursive) scan for the menu gate; the app's directory scan
// includes hidden files, so we match that. Capped at maxEntries files so
// opening a context menu on a huge directory never stalls.
bool DocumentInfo::dirContainsConvertibleImage(const QString &dirPath, int maxEntries) {
    QDirIterator it(dirPath, QDir::Files | QDir::Hidden);
    int examined = 0;
    while(it.hasNext() && examined < maxEntries) {
        if(isConvertibleImageFile(it.next()))
            return true;
        examined++;
    }
    return false;
}

inline
// dumb apng detector
bool DocumentInfo::detectAPNG() {
    QFile f(fileInfo.filePath());
    if(f.open(QFile::ReadOnly)) {
        QDataStream in(&f);
        const int len = 120;
        QByteArray qbuf("\0", len);
        if (in.readRawData(qbuf.data(), len) > 0) {
            return qbuf.contains("acTL");
        }
    }
    return false;
}

bool DocumentInfo::detectAnimatedWebP() {
    QFile f(fileInfo.filePath());
    bool result = false;
    if(f.open(QFile::ReadOnly)) {
        QDataStream in(&f);
        in.skipRawData(12);
        char buf[5] = {0};
        if(in.readRawData(buf, 4) == 4 && strcmp(buf, "VP8X") == 0) {
            in.skipRawData(4);
            char flags = 0;
            if(in.readRawData(&flags, 1) == 1 && (flags & (1 << 1)))
                result = true;
        }
    }
    return result;
}

// TODO avoid creating multiple QImageReader instances
bool DocumentInfo::detectAnimatedJxl() {
    QImageReader r(fileInfo.filePath(), "jxl");
    return r.supportsAnimation();
}

bool DocumentInfo::detectAnimatedAvif() {
    QFile f(fileInfo.filePath());
    bool result = false;
    if(f.open(QFile::ReadOnly)) {
        QDataStream in(&f);
        in.skipRawData(4); // skip box size
        char buf[9] = {0};
        if(in.readRawData(buf, 8) == 8 && strcmp(buf, "ftypavis") == 0)
            result = true;
    }
    return result;
}

void DocumentInfo::loadExifTags() {
    if(exifLoaded)
        return;
    exifLoaded = true;
    exifTags.clear();
#ifdef USE_EXIV2
    try {
        std::unique_ptr<Exiv2::Image> image;

        image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));

        assert(image.get() != 0);
        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();
        if(exifData.empty())
            return;

        Exiv2::ExifKey make("Exif.Image.Make");
        Exiv2::ExifKey model("Exif.Image.Model");
        Exiv2::ExifKey dateTime("Exif.Image.DateTime");
        Exiv2::ExifKey exposureTime("Exif.Photo.ExposureTime");
        Exiv2::ExifKey fnumber("Exif.Photo.FNumber");
        Exiv2::ExifKey isoSpeedRatings("Exif.Photo.ISOSpeedRatings");
        Exiv2::ExifKey flash("Exif.Photo.Flash");
        Exiv2::ExifKey focalLength("Exif.Photo.FocalLength");
        Exiv2::ExifKey userComment("Exif.Photo.UserComment");

        Exiv2::ExifData::const_iterator it;

        it = exifData.findKey(make);
        if(it != exifData.end() /* && it->count() */)
            exifTags.insert(QObject::tr("Make"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(model);
        if(it != exifData.end())
            exifTags.insert(QObject::tr("Model"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(dateTime);
        if(it != exifData.end())
            exifTags.insert(QObject::tr("Date/Time"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(exposureTime);
        if(it != exifData.end()) {
            Exiv2::Rational r = it->toRational();
            if(r.first < r.second) {
                qreal exp = round(static_cast<qreal>(r.second) / r.first);
                exifTags.insert(QObject::tr("ExposureTime"), "1/" + QString::number(exp) + QObject::tr(" sec"));
            } else {
                qreal exp = round(static_cast<qreal>(r.first) / r.second);
                exifTags.insert(QObject::tr("ExposureTime"), QString::number(exp) + QObject::tr(" sec"));
            }
        }

        it = exifData.findKey(fnumber);
        if(it != exifData.end()) {
            Exiv2::Rational r = it->toRational();
            qreal fn = static_cast<qreal>(r.first) / r.second;
            exifTags.insert(QObject::tr("F Number"), "f/" + QString::number(fn, 'g', 3));
        }

        it = exifData.findKey(isoSpeedRatings);
        if(it != exifData.end())
            exifTags.insert(QObject::tr("ISO Speed ratings"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(flash);
        if(it != exifData.end())
            exifTags.insert(QObject::tr("Flash"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(focalLength);
        if(it != exifData.end()) {
            Exiv2::Rational r = it->toRational();
            qreal fn = static_cast<qreal>(r.first) / r.second;
            exifTags.insert(QObject::tr("Focal Length"), QString::number(fn, 'g', 3) + QObject::tr(" mm"));
        }

        it = exifData.findKey(userComment);
        if(it != exifData.end()) {
            // crop out 'charset=ascii' etc"
            auto comment = QString::fromStdString(it->value().toString());
            if(comment.startsWith("charset="))
                comment.remove(0, comment.indexOf(" ") + 1);
            exifTags.insert(QObject::tr("UserComment"), comment);
        }
    }

// this should work with both 0.28 and <0.28
#if not EXIV2_TEST_VERSION(0, 28, 0)
#ifdef __WIN32
    catch (Exiv2::BasicError<wchar_t>& e) {
        qCWarning(logLoader) << "Caught Exiv2::BasicError exception:\n" << e.what() << "\n";
        return;
    }
#else
    catch (Exiv2::BasicError<char>& e) {
        qCWarning(logLoader) << "Caught Exiv2::BasicError exception:\n" << e.what() << "\n";
        return;
    }
#endif
#endif

    catch (Exiv2::Error& e) {
        qCWarning(logLoader) << "Caught Exiv2 exception:\n" << e.what() << "\n";
        return;
    }
#endif
}

QMap<QString, QString> DocumentInfo::getExifTags() {
    if(!exifLoaded)
        loadExifTags();
    return exifTags;
}

// Dumps every Exif/Iptc/Xmp tag exiv2 can read, keyed by its full family.key
// name (e.g. "Exif.Image.Make"). Values are the human-readable interpreted
// strings (same as `exiv2 -pa`). Keys sort into Exif/Iptc/Xmp groups for free.
void DocumentInfo::loadAllTags() {
    if(allTagsLoaded)
        return;
    allTagsLoaded = true;
    allTags.clear();
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return;
        image->readMetadata();

        const Exiv2::ExifData &exifData = image->exifData();
        for(auto it = exifData.begin(); it != exifData.end(); ++it) {
            std::ostringstream os;
            os << *it;
            allTags.insert(QString::fromStdString(it->key()), QString::fromStdString(os.str()));
        }
        const Exiv2::IptcData &iptcData = image->iptcData();
        for(auto it = iptcData.begin(); it != iptcData.end(); ++it) {
            std::ostringstream os;
            os << *it;
            allTags.insert(QString::fromStdString(it->key()), QString::fromStdString(os.str()));
        }
        const Exiv2::XmpData &xmpData = image->xmpData();
        for(auto it = xmpData.begin(); it != xmpData.end(); ++it) {
            std::ostringstream os;
            os << *it;
            allTags.insert(QString::fromStdString(it->key()), QString::fromStdString(os.str()));
        }
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::loadAllTags() - exiv2 failed to read" << fileInfo.filePath();
    }
#endif
}

QMap<QString, QString> DocumentInfo::getAllTags() {
    if(!allTagsLoaded)
        loadAllTags();
    return allTags;
}

// The Xmp half of loadAllTags(), on its own: same keys ("Xmp.dc.title"), same
// interpreted stream values, cached under its own flag so opening the Xmp tab
// does not pay for a full Exif+Iptc dump it will not show.
void DocumentInfo::loadXmpTags() {
    if(xmpLoaded)
        return;
    xmpLoaded = true;
    xmpTags.clear();
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return;
        image->readMetadata();
        const Exiv2::XmpData &xmpData = image->xmpData();
        for(auto it = xmpData.begin(); it != xmpData.end(); ++it) {
            std::ostringstream os;
            os << *it;
            xmpTags.insert(QString::fromStdString(it->key()), QString::fromStdString(os.str()));
        }
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::loadXmpTags() - exiv2 failed to read" << fileInfo.filePath();
    }
#endif
}

QMap<QString, QString> DocumentInfo::getXmpTags() {
    if(!xmpLoaded)
        loadXmpTags();
    return xmpTags;
}

#ifdef USE_EXIV2
namespace {

QString primariesName(QColorSpace::Primaries primaries) {
    switch(primaries) {
    case QColorSpace::Primaries::SRgb:       return QStringLiteral("SRgb");
    case QColorSpace::Primaries::AdobeRgb:   return QStringLiteral("AdobeRgb");
    case QColorSpace::Primaries::DciP3D65:   return QStringLiteral("DciP3D65");
    case QColorSpace::Primaries::ProPhotoRgb:return QStringLiteral("ProPhotoRgb");
    case QColorSpace::Primaries::Custom:     break;
    default:                                 break;
    }
    return QStringLiteral("Custom");
}

QString transferFunctionName(const QColorSpace &space) {
    switch(space.transferFunction()) {
    case QColorSpace::TransferFunction::Linear: return QStringLiteral("Linear");
    case QColorSpace::TransferFunction::Gamma:
        // The name alone says nothing useful here - gamma 1.8 and gamma 2.2 are
        // both "Gamma", and the number is the part someone is looking for.
        return QStringLiteral("Gamma (%1)").arg(space.gamma(), 0, 'g', 3);
    case QColorSpace::TransferFunction::SRgb:   return QStringLiteral("SRgb");
    case QColorSpace::TransferFunction::St2084: return QStringLiteral("St2084");
    case QColorSpace::TransferFunction::Hlg:    return QStringLiteral("Hlg");
    case QColorSpace::TransferFunction::Custom: break;
    default:                                    break;
    }
    return QStringLiteral("Custom");
}

} // namespace
#endif

// Summarises the embedded profile instead of dumping it: the blob is binary and
// several hundred bytes even for plain sRGB, so what is worth showing is what Qt
// managed to make of it.
QList<QPair<QString, QString>> DocumentInfo::getIccProfileInfo() {
    QList<QPair<QString, QString>> info;
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return info;
        image->readMetadata();
        if(!image->iccProfileDefined())
            return info;
        const Exiv2::DataBuf &profile = image->iccProfile();
        if(profile.size() == 0)
            return info;
        const QByteArray raw(reinterpret_cast<const char *>(profile.c_data()),
                             static_cast<qsizetype>(profile.size()));
        const QColorSpace space = QColorSpace::fromIccProfile(raw);
        // A profile Qt cannot parse is reported as "none" rather than as four
        // empty rows: there is nothing truthful to put in them.
        if(!space.isValid())
            return info;
        info.append({QObject::tr("Profile"), space.description()});
        info.append({QObject::tr("Primaries"), primariesName(space.primaries())});
        info.append({QObject::tr("Transfer function"), transferFunctionName(space)});
        info.append({QObject::tr("Profile size"),
                     QObject::tr("%1 bytes").arg(raw.size())});
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::getIccProfileInfo() - exiv2 failed to read"
                             << fileInfo.filePath();
    }
#endif
    return info;
}

bool DocumentInfo::stripMetadata() {
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        image->clearMetadata();
        image->writeMetadata();
        // invalidate caches so a refresh re-reads from disk
        exifLoaded = false;
        allTagsLoaded = false;
        xmpLoaded = false;
        exifTags.clear();
        allTags.clear();
        xmpTags.clear();
        return true;
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::stripMetadata() - exiv2 failed to write" << fileInfo.filePath();
        return false;
    }
#else
    return false;
#endif
}

// --- editable metadata -------------------------------------------------------

QStringList DocumentInfo::editableTagKeys() {
    return {QStringLiteral("Exif.Image.Make"),
            QStringLiteral("Exif.Image.Model"),
            QStringLiteral("Exif.Image.DateTime"),
            QStringLiteral("Exif.Photo.UserComment")};
}

QString DocumentInfo::editableTagLabel(const QString &key) {
    if(key == QLatin1String("Exif.Image.Make"))
        return QObject::tr("Make");
    if(key == QLatin1String("Exif.Image.Model"))
        return QObject::tr("Model");
    if(key == QLatin1String("Exif.Image.DateTime"))
        return QObject::tr("Date/Time");
    if(key == QLatin1String("Exif.Photo.UserComment"))
        return QObject::tr("Comment");
    return key;
}

// Deliberately a allow-list of two rather than "anything exiv2 might open".
// exiv2 can read metadata from far more formats than it can safely rewrite, and
// offering an editor that fails on save is worse than not offering one.
bool DocumentInfo::supportsMetadataEditing(const QString &filePath) {
#ifdef USE_EXIV2
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") ||
           suffix == QLatin1String("webp");
#else
    Q_UNUSED(filePath)
    return false;
#endif
}

// libheif hands the picture to x265, whose default 64x64 coding tree unit
// cannot describe a smaller frame. It does not report this: the encode
// "succeeds" and the file reads back transposed (64x48 -> 48x64) or plainly
// garbled (100x50 -> 86x64). Measured 2026-08-31 against libheif 1.23.1 with
// kimageformats 6.29; sizes at or above the floor round-tripped exactly.
// Refused up front rather than left to fail, because a silently wrong image on
// disk is worse than a conversion that declines to run.
bool DocumentInfo::canEncodeToFormat(const QString &format, const QSize &size) {
    static const int heifMinDimension = 64;
    static const QSet<QString> heifFormats = {QStringLiteral("heic"), QStringLiteral("heif"),
                                              QStringLiteral("hif"),  QStringLiteral("avci"),
                                              QStringLiteral("hej2")};
    if(!heifFormats.contains(format.toLower()))
        return true;
    return size.width() >= heifMinDimension && size.height() >= heifMinDimension;
}

// exiv2 reads metadata out of every container thumbgrid opens, but it writes
// back into only a subset. Probed with exiv2 0.28 on 2026-08-31: BMFF-based
// files (heic/heif/avif/cr3) are refused outright with "Writing to BMFF images
// is not supported", and jxl is not recognised as a writable type at all. An
// allowlist rather than a BMFF denylist, so a format nobody probed cannot reach
// a rewrite path by default - the cost of a wrong "no" is a greyed-out action,
// the cost of a wrong "yes" is an irreversible-sounding confirmation for an
// operation that always fails.
bool DocumentInfo::supportsMetadataWriting(const QString &filePath) {
#ifdef USE_EXIV2
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QLatin1String("jpg")  || suffix == QLatin1String("jpeg") ||
           suffix == QLatin1String("jfif") || suffix == QLatin1String("png")  ||
           suffix == QLatin1String("webp") || suffix == QLatin1String("tif")  ||
           suffix == QLatin1String("tiff");
#else
    Q_UNUSED(filePath)
    return false;
#endif
}

// Wider than supportsMetadataEditing() on purpose: the Xmp tab only reads, so
// the gate is "can this container hold an XMP packet at all", which png and tiff
// can even though exiv2 will not rewrite Exif into them.
bool DocumentInfo::supportsXmp(const QString &filePath) {
#ifdef USE_EXIV2
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    // heif is read-only here for the same reason it is absent from the editing
    // gates - exiv2 parses its XMP packet fine but cannot write one back.
    return suffix == QLatin1String("jpg")  || suffix == QLatin1String("jpeg") ||
           suffix == QLatin1String("png")  || suffix == QLatin1String("webp") ||
           suffix == QLatin1String("tif")  || suffix == QLatin1String("tiff") ||
           suffix == QLatin1String("heic") || suffix == QLatin1String("heif") ||
           suffix == QLatin1String("hif");
#else
    Q_UNUSED(filePath)
    return false;
#endif
}

// Same set, and for the same reason: these are the formats with a defined place
// to embed an ICC profile that exiv2 will hand back via iccProfile().
bool DocumentInfo::supportsIccProfile(const QString &filePath) {
#ifdef USE_EXIV2
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") ||
           suffix == QLatin1String("png") || suffix == QLatin1String("webp") ||
           suffix == QLatin1String("tif") || suffix == QLatin1String("tiff");
#else
    Q_UNUSED(filePath)
    return false;
#endif
}

// --- XMP / ICC writing -------------------------------------------------------

// Wider than supportsMetadataEditing(): probed 2026-08-01 that exiv2 writes XMP
// into png as cleanly as into jpeg, which it will not do for Exif. tiff is left
// out on purpose - exiv2 rewrites a tiff's whole structure, and its Exif and ICC
// scopes overlap, so no removal there could be scoped to one kind.
bool DocumentInfo::supportsXmpEditing(const QString &filePath) {
#ifdef USE_EXIV2
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") ||
           suffix == QLatin1String("png") || suffix == QLatin1String("webp");
#else
    Q_UNUSED(filePath)
    return false;
#endif
}

// Same set, same reason. In a tiff the profile is stored as an Exif tag, so
// clearing it there is not a scoped operation at all.
bool DocumentInfo::supportsIccEditing(const QString &filePath) {
    return supportsXmpEditing(filePath);
}

QStringList DocumentInfo::editableXmpKeys() {
    return {QStringLiteral("Xmp.dc.title"),
            QStringLiteral("Xmp.dc.description"),
            QStringLiteral("Xmp.dc.creator"),
            QStringLiteral("Xmp.dc.rights"),
            QStringLiteral("Xmp.dc.subject"),
            QStringLiteral("Xmp.xmp.Rating"),
            QStringLiteral("Xmp.xmp.Label")};
}

QString DocumentInfo::editableXmpLabel(const QString &key) {
    if(key == QLatin1String("Xmp.dc.title"))       return QObject::tr("Title");
    if(key == QLatin1String("Xmp.dc.description")) return QObject::tr("Description");
    if(key == QLatin1String("Xmp.dc.creator"))     return QObject::tr("Author");
    if(key == QLatin1String("Xmp.dc.rights"))      return QObject::tr("Copyright");
    if(key == QLatin1String("Xmp.dc.subject"))     return QObject::tr("Keywords");
    if(key == QLatin1String("Xmp.xmp.Rating"))     return QObject::tr("Rating");
    if(key == QLatin1String("Xmp.xmp.Label"))      return QObject::tr("Label");
    return key;
}

DocumentInfo::XmpValueKind DocumentInfo::xmpValueKind(const QString &key) {
    if(key == QLatin1String("Xmp.dc.title") || key == QLatin1String("Xmp.dc.description") ||
       key == QLatin1String("Xmp.dc.rights"))
        return XmpValueKind::LangAlt;
    if(key == QLatin1String("Xmp.dc.creator"))
        return XmpValueKind::Seq;
    if(key == QLatin1String("Xmp.dc.subject"))
        return XmpValueKind::Bag;
    return XmpValueKind::Text;
}

#ifdef USE_EXIV2
namespace {

const char *kDefaultLang = "x-default";

// A LangAlt value's x-default entry, or empty. Read through LangAltValue rather
// than the stream form, which prefixes every entry with its lang and joins them
// with commas - unusable as an edit-box value.
QString langAltDefault(const Exiv2::Xmpdatum &datum) {
    const auto *la = dynamic_cast<const Exiv2::LangAltValue *>(&datum.value());
    if(!la)
        return QString::fromStdString(datum.value().toString());
    for(const auto &entry : la->value_) {
        if(entry.first == kDefaultLang)
            return QString::fromStdString(entry.second);
    }
    return QString();
}

// Array items, in order. count() is the item count for XmpArrayValue; toString(n)
// gives one item, which is the whole point of not using the joined stream form.
QStringList arrayItems(const Exiv2::Xmpdatum &datum) {
    QStringList items;
    const long n = static_cast<long>(datum.count());
    for(long i = 0; i < n; ++i)
        items.append(QString::fromStdString(datum.value().toString(i)));
    return items;
}

// Erases every datum under key - an array can hold more than one, and assignment
// appends rather than replaces, so a rewrite has to start from nothing.
void eraseAll(Exiv2::XmpData &xmp, const std::string &key) {
    for(;;) {
        auto it = xmp.findKey(Exiv2::XmpKey(key));
        if(it == xmp.end())
            return;
        xmp.erase(it);
    }
}

// Xmp.<prefix>.<Name> -> prefix. Empty when the key is not in that shape.
QString xmpPrefixOf(const QString &key) {
    const QStringList parts = key.split(QLatin1Char('.'));
    return parts.size() == 3 && parts.at(0) == QLatin1String("Xmp") ? parts.at(1) : QString();
}

} // namespace
#endif

bool DocumentInfo::isRegisteredXmpKey(const QString &key) {
#ifdef USE_EXIV2
    try {
        return Exiv2::XmpProperties::propertyInfo(Exiv2::XmpKey(key.toStdString())) != nullptr;
    } catch(...) {
        // An unparseable key is not a schema exiv2 knows, which is the question
        // being asked.
        return false;
    }
#else
    Q_UNUSED(key)
    return false;
#endif
}

bool DocumentInfo::isKnownXmpPrefix(const QString &prefix) {
#ifdef USE_EXIV2
    try {
        return !Exiv2::XmpProperties::ns(prefix.toStdString()).empty();
    } catch(...) {
        return false;
    }
#else
    Q_UNUSED(prefix)
    return false;
#endif
}

QMap<QString, QStringList> DocumentInfo::getEditableXmpTags() {
    QMap<QString, QStringList> values;
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return values;
        image->readMetadata();
        const Exiv2::XmpData &xmp = image->xmpData();
        for(const QString &key : editableXmpKeys()) {
            auto it = xmp.findKey(Exiv2::XmpKey(key.toStdString()));
            if(it == xmp.end())
                continue;
            switch(xmpValueKind(key)) {
            case XmpValueKind::LangAlt: {
                const QString text = langAltDefault(*it);
                values.insert(key, text.isEmpty() ? QStringList() : QStringList{text});
                break;
            }
            case XmpValueKind::Seq:
            case XmpValueKind::Bag:
                values.insert(key, arrayItems(*it));
                break;
            case XmpValueKind::Text:
                values.insert(key, QStringList{QString::fromStdString(it->value().toString())});
                break;
            }
        }
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::getEditableXmpTags() - exiv2 failed to read"
                             << fileInfo.filePath();
    }
#endif
    return values;
}

bool DocumentInfo::setEditableXmpTags(const QMap<QString, QStringList> &values) {
#ifdef USE_EXIV2
    // Validate before opening the file: a half-applied edit is worse than a
    // rejected one, the same rule setEditableTags() follows.
    const QStringList allowed = editableXmpKeys();
    for(auto it = values.cbegin(); it != values.cend(); ++it) {
        if(!allowed.contains(it.key()))
            continue;
        for(const QString &item : it.value()) {
            if(item.length() > kMaxXmpValueLengthEdited)
                return false;
        }
        if(it.key() == QLatin1String("Xmp.xmp.Rating") && !it.value().isEmpty() &&
           !it.value().first().isEmpty()) {
            bool ok = false;
            const int rating = it.value().first().toInt(&ok);
            // XMP spec: -1 means rejected, then 0-5.
            if(!ok || rating < -1 || rating > 5)
                return false;
        }
    }
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        Exiv2::XmpData &xmp = image->xmpData();
        for(auto it = values.cbegin(); it != values.cend(); ++it) {
            if(!allowed.contains(it.key()))
                continue;
            const std::string key = it.key().toStdString();
            const QStringList items = it.value();
            const bool empty = items.isEmpty() || (items.size() == 1 && items.first().isEmpty());
            switch(xmpValueKind(it.key())) {
            case XmpValueKind::LangAlt:
                // Assigning an empty x-default drops exactly that entry and
                // leaves translations in other languages alone - clearing a
                // title must not destroy a de-DE one.
                xmp[key] = empty ? std::string("lang=x-default ")
                                 : ("lang=x-default " + items.first().toStdString());
                break;
            case XmpValueKind::Seq:
            case XmpValueKind::Bag: {
                eraseAll(xmp, key);
                if(empty)
                    break;
                auto value = Exiv2::Value::create(xmpValueKind(it.key()) == XmpValueKind::Seq
                                                      ? Exiv2::xmpSeq
                                                      : Exiv2::xmpBag);
                for(const QString &item : items) {
                    if(!item.isEmpty())
                        value->read(item.toStdString());
                }
                xmp.add(Exiv2::XmpKey(key), value.get());
                break;
            }
            case XmpValueKind::Text:
                // Assignment would leave a present-but-blank property, which
                // shows up as an empty row everywhere else; a curated row has
                // only its value to delete by, so empty means erase.
                if(empty)
                    eraseAll(xmp, key);
                else
                    xmp[key] = items.first().toStdString();
                break;
            }
        }
        image->writeMetadata();
        xmpLoaded = false;
        allTagsLoaded = false;
        xmpTags.clear();
        allTags.clear();
        return true;
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::setEditableXmpTags() - exiv2 failed to write"
                             << fileInfo.filePath();
        return false;
    }
#else
    Q_UNUSED(values)
    return false;
#endif
}

QMap<QString, QString> DocumentInfo::getCustomXmpTags() {
    QMap<QString, QString> values;
#ifdef USE_EXIV2
    const QMap<QString, QString> all = getXmpTags();
    for(auto it = all.cbegin(); it != all.cend(); ++it) {
        if(!isRegisteredXmpKey(it.key()))
            values.insert(it.key(), it.value());
    }
#endif
    return values;
}

bool DocumentInfo::setCustomXmpTags(const QMap<QString, QString> &values) {
#ifdef USE_EXIV2
    for(auto it = values.cbegin(); it != values.cend(); ++it) {
        if(it.key().length() > kMaxXmpKeyLength ||
           it.value().length() > kMaxXmpValueLengthEdited)
            return false;
        if(isRegisteredXmpKey(it.key()))
            return false;
        if(!isKnownXmpPrefix(xmpPrefixOf(it.key())))
            return false;
    }
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        Exiv2::XmpData &xmp = image->xmpData();
        // A custom property is always XmpText, so plain assignment replaces it.
        // An empty value is kept rather than erased: in the grid the key is the
        // row's identity, so an empty value is a legal state and erasing is what
        // eraseXmpKey() is for.
        for(auto it = values.cbegin(); it != values.cend(); ++it)
            xmp[it.key().toStdString()] = it.value().toStdString();
        image->writeMetadata();
        xmpLoaded = false;
        allTagsLoaded = false;
        xmpTags.clear();
        allTags.clear();
        return true;
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::setCustomXmpTags() - exiv2 failed to write"
                             << fileInfo.filePath();
        return false;
    }
#else
    Q_UNUSED(values)
    return false;
#endif
}

bool DocumentInfo::eraseXmpKey(const QString &key) {
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        eraseAll(image->xmpData(), key.toStdString());
        image->writeMetadata();
        xmpLoaded = false;
        allTagsLoaded = false;
        xmpTags.clear();
        allTags.clear();
        return true;
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::eraseXmpKey() - exiv2 failed to write"
                             << fileInfo.filePath();
        return false;
    }
#else
    Q_UNUSED(key)
    return false;
#endif
}

bool DocumentInfo::addCustomXmpProperty(const QString &prefix, const QString &name,
                                        const QString &namespaceUri, const QString &value) {
#ifdef USE_EXIV2
    if(prefix.isEmpty() || name.isEmpty())
        return false;
    const QString key = QStringLiteral("Xmp.%1.%2").arg(prefix, name);
    if(key.length() > kMaxXmpKeyLength || value.length() > kMaxXmpValueLengthEdited)
        return false;
    if(getCustomXmpTags().size() >= kMaxCustomXmpProperties)
        return false;
    try {
        // Only for a genuinely new prefix: writing one the file does not already
        // declare throws "No namespace info available for XMP prefix".
        if(!isKnownXmpPrefix(prefix)) {
            if(namespaceUri.isEmpty())
                return false;
            Exiv2::XmpProperties::registerNs(namespaceUri.toStdString(), prefix.toStdString());
        }
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        image->xmpData()[key.toStdString()] = value.toStdString();
        image->writeMetadata();
        xmpLoaded = false;
        allTagsLoaded = false;
        xmpTags.clear();
        allTags.clear();
        return true;
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::addCustomXmpProperty() - exiv2 failed to write"
                             << fileInfo.filePath();
        return false;
    }
#else
    Q_UNUSED(prefix) Q_UNUSED(name) Q_UNUSED(namespaceUri) Q_UNUSED(value)
    return false;
#endif
}

#ifdef USE_EXIV2
// The three scoped removals share everything but one line, and each was probed
// to leave the other two kinds intact - that isolation is what lets the three
// buttons live in three tabs.
bool DocumentInfo::clearExifMetadata() {
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        image->exifData().clear();
        image->writeMetadata();
        exifLoaded = false;
        allTagsLoaded = false;
        exifTags.clear();
        allTags.clear();
        return true;
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::clearExifMetadata() - exiv2 failed to write"
                             << fileInfo.filePath();
        return false;
    }
}

bool DocumentInfo::clearXmpMetadata() {
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        image->xmpData().clear();
        image->writeMetadata();
        xmpLoaded = false;
        allTagsLoaded = false;
        xmpTags.clear();
        allTags.clear();
        return true;
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::clearXmpMetadata() - exiv2 failed to write"
                             << fileInfo.filePath();
        return false;
    }
}

bool DocumentInfo::clearIccProfile() {
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        image->clearIccProfile();
        image->writeMetadata();
        return true;
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::clearIccProfile() - exiv2 failed to write"
                             << fileInfo.filePath();
        return false;
    }
}

bool DocumentInfo::hasExifMetadata() {
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        return !image->exifData().empty();
    } catch(...) {
        return false;
    }
}

bool DocumentInfo::hasXmpMetadata() {
    return !getXmpTags().isEmpty();
}

bool DocumentInfo::hasIccProfile() {
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        return image->iccProfileDefined();
    } catch(...) {
        return false;
    }
}
#else
bool DocumentInfo::clearExifMetadata() { return false; }
bool DocumentInfo::clearXmpMetadata()  { return false; }
bool DocumentInfo::clearIccProfile()   { return false; }
bool DocumentInfo::hasExifMetadata()   { return false; }
bool DocumentInfo::hasXmpMetadata()    { return false; }
bool DocumentInfo::hasIccProfile()     { return false; }
#endif

bool DocumentInfo::isValidExifDateTime(const QString &value) {
    // Exif 2.3 §4.6.4: exactly "YYYY:MM:DD HH:MM:SS", zero-padded.
    return QDateTime::fromString(value, QStringLiteral("yyyy:MM:dd HH:mm:ss")).isValid();
}

#ifdef USE_EXIV2
namespace {

// UserComment is an Undefined-typed value carrying an 8-byte charset header.
// exiv2's string form keeps that header ("charset=Ascii hello"), which is an
// encoding detail rather than something to show in an edit box.
QString stripCharsetPrefix(const QString &raw) {
    if(!raw.startsWith(QLatin1String("charset=")))
        return raw;
    const int space = raw.indexOf(QLatin1Char(' '));
    return space < 0 ? QString() : raw.mid(space + 1);
}

// ...and put it back on the way in. Non-ASCII text must be labelled Unicode:
// exiv2 will happily store UTF-8 bytes under charset=Ascii, but other readers
// then mangle them.
std::string withCharsetPrefix(const QString &text) {
    const bool ascii = std::all_of(text.cbegin(), text.cend(),
                                   [](QChar c) { return c.unicode() < 128; });
    const QString prefix = ascii ? QStringLiteral("charset=Ascii ")
                                 : QStringLiteral("charset=Unicode ");
    return (prefix + text).toStdString();
}

} // namespace
#endif

QMap<QString, QString> DocumentInfo::getEditableTags() {
    QMap<QString, QString> values;
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return values;
        image->readMetadata();
        const Exiv2::ExifData &exifData = image->exifData();
        for(const QString &key : editableTagKeys()) {
            auto it = exifData.findKey(Exiv2::ExifKey(key.toStdString()));
            if(it == exifData.end())
                continue;
            // value().toString(), not the stream operator: the stream form is
            // the interpreted one, which is exactly what must not round-trip.
            QString raw = QString::fromStdString(it->value().toString());
            if(key == QLatin1String("Exif.Photo.UserComment"))
                raw = stripCharsetPrefix(raw);
            values.insert(key, raw);
        }
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::getEditableTags() - exiv2 failed to read"
                             << fileInfo.filePath();
    }
#endif
    return values;
}

bool DocumentInfo::setEditableTags(const QMap<QString, QString> &values) {
#ifdef USE_EXIV2
    // Validate everything before opening the file: a half-applied edit is worse
    // than a rejected one.
    const QStringList allowed = editableTagKeys();
    for(auto it = values.cbegin(); it != values.cend(); ++it) {
        if(!allowed.contains(it.key()))
            continue;
        if(it.key() == QLatin1String("Exif.Image.DateTime") && !it.value().isEmpty() &&
           !isValidExifDateTime(it.value()))
            return false;
    }
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();
        for(auto it = values.cbegin(); it != values.cend(); ++it) {
            if(!allowed.contains(it.key()))
                continue;
            const std::string key = it.key().toStdString();
            // Empty means "remove": leaving an empty Ascii tag behind would show
            // up as a present-but-blank row everywhere else.
            if(it.value().isEmpty()) {
                auto existing = exifData.findKey(Exiv2::ExifKey(key));
                if(existing != exifData.end())
                    exifData.erase(existing);
                continue;
            }
            if(it.key() == QLatin1String("Exif.Photo.UserComment"))
                exifData[key] = withCharsetPrefix(it.value());
            else
                exifData[key] = it.value().toStdString();
        }
        image->writeMetadata();
        // Same cache invalidation stripMetadata() does - both tag maps are lazy
        // and would otherwise keep serving pre-edit values.
        exifLoaded = false;
        allTagsLoaded = false;
        exifTags.clear();
        allTags.clear();
        return true;
    } catch(...) {
        qCWarning(logLoader) << "DocumentInfo::setEditableTags() - exiv2 failed to write"
                             << fileInfo.filePath();
        return false;
    }
#else
    Q_UNUSED(values)
    return false;
#endif
}

#ifdef USE_EXIV2
bool DocumentInfo::loadExifOrientationExiv2() {
    try {
        auto image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image.get())
            return false;
        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();
        if(exifData.empty())
            return false;
        auto it = exifData.findKey(Exiv2::ExifKey("Exif.Image.Orientation"));
        if(it == exifData.end())
            return false;
        int exifOri = QString::fromStdString(it->value().toString()).toInt();
        if(exifOri < 1 || exifOri > 8)
            return false;
        // Map standard EXIF orientation (1-8) onto the QImageIOHandler
        // transformation values consumed by ImageLib::exifRotated(). This is
        // exactly Qt's exif2Qt() mapping, so behaviour matches QImageReader for
        // formats Qt handles, and now also covers formats exiv2 reads but Qt
        // does not expose orientation for.
        static const int exif2qt[9] = {0, 0, 1, 3, 2, 6, 4, 5, 7};
        mOrientation = exif2qt[exifOri];
        return true;
    } catch(...) {
        return false;
    }
}
#endif

void DocumentInfo::loadExifOrientation() {
    if(mDocumentType != DocumentType::STATIC && mDocumentType != DocumentType::ANIMATED)
        return;

#ifdef USE_EXIV2
    if(loadExifOrientationExiv2())
        return;
#endif

    QString path = filePath();
    QImageReader *reader = nullptr;
    if(!mFormat.isEmpty())
        reader = new QImageReader(path, mFormat.toStdString().c_str());
    else
        reader = new QImageReader(path);

    if(reader->canRead())
        mOrientation = static_cast<int>(reader->transformation());
    delete reader;
}
