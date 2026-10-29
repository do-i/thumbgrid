#include "documentinfo.h"
#include "utils/logging.h"
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

// orientation or mimetype changes reload the Image from scratch
void DocumentInfo::refresh() {
    fileInfo.refresh();
}

int DocumentInfo::exifOrientation() const {
    return mOrientation;
}

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
        // one container, several extensions; the plugin needs the file's own key (no sniffing with an explicit format)
        static const QSet<QByteArray> heifKeys = {"heic", "heif", "hif", "avci", "hej2"};
        mFormat = QString::fromUtf8(heifKeys.contains(suffix) ? suffix : QByteArrayLiteral("heif"));
        // heif always needs an optional plugin; without one mark the file unviewable instead of showing blank
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

// text/plain-derived mime types, plus suffixes and names that ship with generic mime types
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

// Menu gate: gif/apng are practically always animated and pdf is excluded app-wide.
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

// shallow scan including hidden files, capped so a huge folder never stalls the menu
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

        image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());

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
    catch (Exiv2::BasicError<char>& e) {
        qCWarning(logLoader) << "Caught Exiv2::BasicError exception:\n" << e.what() << "\n";
        return;
    }
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

// every Exif/Iptc/Xmp tag keyed "Family.Group.Tag", interpreted values (like `exiv2 -pa`)
void DocumentInfo::loadAllTags() {
    if(allTagsLoaded)
        return;
    allTagsLoaded = true;
    allTags.clear();
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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

// the Xmp part of loadAllTags(), cached separately so the Xmp tab skips the full dump
void DocumentInfo::loadXmpTags() {
    if(xmpLoaded)
        return;
    xmpLoaded = true;
    xmpTags.clear();
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
        // the gamma value is what distinguishes them
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

// summarizes what Qt parsed from the binary profile
QList<QPair<QString, QString>> DocumentInfo::getIccProfileInfo() {
    QList<QPair<QString, QString>> info;
#ifdef USE_EXIV2
    try {
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
        // an unparseable profile is reported as none
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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

// allowlist: offering an editor that fails on save is worse than none
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

bool DocumentInfo::supportsExif(const QString &filePath) {
#ifdef USE_EXIV2
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return supportsMetadataEditing(filePath) || suffix == QLatin1String("heic") ||
           suffix == QLatin1String("heif") || suffix == QLatin1String("hif");
#else
    Q_UNUSED(filePath)
    return false;
#endif
}

// libheif/x265 silently transposes or garbles frames below the 64x64 CTU (measured with libheif 1.23.1),
// so refuse up front.
bool DocumentInfo::canEncodeToFormat(const QString &format, const QSize &size) {
    static const int heifMinDimension = 64;
    static const QSet<QString> heifFormats = {QStringLiteral("heic"), QStringLiteral("heif"),
                                              QStringLiteral("hif"),  QStringLiteral("avci"),
                                              QStringLiteral("hej2")};
    if(!heifFormats.contains(format.toLower()))
        return true;
    return size.width() >= heifMinDimension && size.height() >= heifMinDimension;
}

// Allowlist of containers exiv2 0.28 rewrites (BMFF refused, jxl unsupported), so unprobed formats
// never reach a rewrite path.
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

// read-only tab: can the container hold an XMP packet (png, tiff can)
bool DocumentInfo::supportsXmp(const QString &filePath) {
#ifdef USE_EXIV2
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    // exiv2 reads heif XMP but cannot write it
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

// formats with a defined ICC slot that exiv2 returns via iccProfile()
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

// exiv2 writes XMP into png cleanly; tiff excluded because its Exif and ICC scopes overlap
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

// in a tiff the profile is an Exif tag, so clearing it is not scoped
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

// x-default entry via LangAltValue; the stream form prefixes langs and joins with commas
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

// items in order (toString(n)), not the joined stream form
QStringList arrayItems(const Exiv2::Xmpdatum &datum) {
    QStringList items;
    const long n = static_cast<long>(datum.count());
    for(long i = 0; i < n; ++i)
        items.append(QString::fromStdString(datum.value().toString(i)));
    return items;
}

// arrays can hold several datums and assignment appends, so erase all first
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
    // validate before opening: a half-applied edit is worse than a rejected one
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
                // empty x-default drops only that entry, keeping other languages
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
                // empty means erase: a blank property would show as an empty row
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
        if(!image.get())
            return false;
        image->readMetadata();
        Exiv2::XmpData &xmp = image->xmpData();
        // plain assignment replaces XmpText; empty values are kept (eraseXmpKey() removes)
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
        // writing an undeclared prefix throws "No namespace info available"
        if(!isKnownXmpPrefix(prefix)) {
            if(namespaceUri.isEmpty())
                return false;
            Exiv2::XmpProperties::registerNs(namespaceUri.toStdString(), prefix.toStdString());
        }
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
// each scoped removal leaves the other two kinds intact
bool DocumentInfo::clearExifMetadata() {
    try {
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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

// UserComment's string form keeps the 8-byte charset header; hide it in the edit box
QString stripCharsetPrefix(const QString &raw) {
    if(!raw.startsWith(QLatin1String("charset=")))
        return raw;
    const int space = raw.indexOf(QLatin1Char(' '));
    return space < 0 ? QString() : raw.mid(space + 1);
}

// label non-ASCII as Unicode: other readers mangle UTF-8 stored as Ascii
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
        if(!image.get())
            return values;
        image->readMetadata();
        const Exiv2::ExifData &exifData = image->exifData();
        for(const QString &key : editableTagKeys()) {
            auto it = exifData.findKey(Exiv2::ExifKey(key.toStdString()));
            if(it == exifData.end())
                continue;
            // raw value, not the interpreted stream form
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
    // validate before opening: a half-applied edit is worse than a rejected one
    const QStringList allowed = editableTagKeys();
    for(auto it = values.cbegin(); it != values.cend(); ++it) {
        if(!allowed.contains(it.key()))
            continue;
        if(it.key() == QLatin1String("Exif.Image.DateTime") && !it.value().isEmpty() &&
           !isValidExifDateTime(it.value()))
            return false;
    }
    try {
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
        if(!image.get())
            return false;
        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();
        for(auto it = values.cbegin(); it != values.cend(); ++it) {
            if(!allowed.contains(it.key()))
                continue;
            const std::string key = it.key().toStdString();
            // empty removes the tag rather than leaving a blank one
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
        // both tag maps are lazy caches
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
        auto image = Exiv2::ImageFactory::open(fileInfo.filePath().toStdString());
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
        // EXIF orientation (1-8) -> QImageIOHandler transform values, Qt's exif2Qt() mapping
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
