// XMP writing: the curated typed set, the custom-property grid's backend, and
// the three scoped removals.
//
// These drive DocumentInfo rather than the UI for the same reason
// test_exif_editing_round_trips.cpp does - the risk is in exiv2's value
// handling, which no UI assertion would catch. The specific traps, each with a
// case below:
//
//   * assignment replaces a Text or LangAlt value but *appends* to an array, so
//     a rewritten keyword list has to be erased first or it grows;
//   * an empty assignment leaves a present-but-blank property rather than
//     deleting, so "empty means remove" needs an explicit erase;
//   * the displayed form of an array joins items with ", ", so a keyword that
//     itself contains a comma is indistinguishable from two keywords unless the
//     items are kept as items;
//   * clearing a LangAlt property must not take other languages with it;
//   * each scoped removal has to leave the other two kinds alone, which is what
//     lets the three buttons live in three tabs.
//
// See docs/2026-08-01-001.

#include "support/thumbgrid_test_support.h"

#include <QColorSpace>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>

#include <exiv2/exiv2.hpp>

#include "sourcecontainers/documentinfo.h"

class XmpEditingRoundTripsTest : public QObject {
    Q_OBJECT

private slots:
    void curatedKeysRoundTripByType();
    void aKeywordContainingACommaSurvivesAnUnrelatedEdit();
    void clearingATitleKeepsItsOtherLanguages();
    void anEmptyCuratedValueRemovesTheProperty();
    void customPropertiesReadAndWriteWithoutRegisteringTheirNamespace();
    void aKeyWithNoValueIsLegalAndStable();
    void anUnknownPrefixNeedsANamespaceUri();
    void eachScopedRemovalLeavesTheOtherKindsIntact();
    void onlyWritableFormatsAreOffered();
    void oversizedKeysAndValuesAreRejectedBeforeTheFileIsOpened();
};

namespace {

const QString kTitle    = QStringLiteral("Xmp.dc.title");
const QString kSubject  = QStringLiteral("Xmp.dc.subject");
const QString kCreator  = QStringLiteral("Xmp.dc.creator");
const QString kRating   = QStringLiteral("Xmp.xmp.Rating");

// A jpeg carrying an sRGB ICC profile, so the removal cases have all three
// kinds present to prove isolation against.
bool writeJpeg(const QString &path) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    image.setColorSpace(QColorSpace::SRgb);
    return image.save(path, "jpeg");
}

bool writeImageAs(const QString &path, const char *format) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    image.setColorSpace(QColorSpace::SRgb);
    return image.save(path, format);
}

// Reads a property straight through exiv2, bypassing DocumentInfo's caches, so
// an assertion is about the file rather than about what the object remembers.
QString rawOnDisk(const QString &path, const QString &key) {
    try {
        auto image = Exiv2::ImageFactory::open(path.toStdString());
        image->readMetadata();
        auto it = image->xmpData().findKey(Exiv2::XmpKey(key.toStdString()));
        if(it == image->xmpData().end())
            return QString();
        std::ostringstream os;
        os << *it;
        return QString::fromStdString(os.str());
    } catch(...) {
        return QStringLiteral("<exiv2 threw>");
    }
}

int itemCountOnDisk(const QString &path, const QString &key) {
    try {
        auto image = Exiv2::ImageFactory::open(path.toStdString());
        image->readMetadata();
        auto it = image->xmpData().findKey(Exiv2::XmpKey(key.toStdString()));
        return it == image->xmpData().end() ? -1 : static_cast<int>(it->count());
    } catch(...) {
        return -2;
    }
}

} // namespace

// Each curated key written and read back in the shape its declared type calls
// for: a LangAlt as one string, an array as its items.
void XmpEditingRoundTripsTest::curatedKeysRoundTripByType() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("photo.jpg");
    QVERIFY(writeJpeg(path));

    DocumentInfo doc(path);
    QVERIFY2(doc.setEditableXmpTags({
                 {kTitle, {QStringLiteral("Sunrise over the bay")}},
                 {QStringLiteral("Xmp.dc.description"), {QStringLiteral("From the pier")}},
                 {kCreator, {QStringLiteral("Jane Doe"), QStringLiteral("John Roe")}},
                 {kSubject, {QStringLiteral("beach"), QStringLiteral("golden hour")}},
                 {kRating, {QStringLiteral("4")}},
             }),
             "writing the curated set should succeed on a jpeg");

    DocumentInfo reread(path);
    const QMap<QString, QStringList> back = reread.getEditableXmpTags();
    QCOMPARE(back.value(kTitle), QStringList{QStringLiteral("Sunrise over the bay")});
    QCOMPARE(back.value(QStringLiteral("Xmp.dc.description")),
             QStringList{QStringLiteral("From the pier")});
    QCOMPARE(back.value(kCreator),
             (QStringList{QStringLiteral("Jane Doe"), QStringLiteral("John Roe")}));
    QCOMPARE(back.value(kSubject),
             (QStringList{QStringLiteral("beach"), QStringLiteral("golden hour")}));
    QCOMPARE(back.value(kRating), QStringList{QStringLiteral("4")});

    // A LangAlt must be stored as one, not as a plain text property: other
    // readers rely on the alt-array to find their language.
    QVERIFY2(rawOnDisk(path, kTitle).contains(QStringLiteral("x-default")),
             "dc:title must be written as a LangAlt carrying x-default");

    // Rewriting an array must replace it, not append to it - exiv2's assignment
    // appends, which would silently grow the list on every edit.
    QVERIFY(doc.setEditableXmpTags({{kSubject, {QStringLiteral("beach")}}}));
    QCOMPARE(itemCountOnDisk(path, kSubject), 1);

    // Out-of-range ratings are refused before the file is touched.
    QVERIFY2(!doc.setEditableXmpTags({{kRating, {QStringLiteral("9")}}}),
             "a rating outside -1..5 should be rejected");
    QCOMPARE(DocumentInfo(path).getEditableXmpTags().value(kRating),
             QStringList{QStringLiteral("4")});
}

// The reason the editable API carries QStringList rather than a joined string:
// the displayed form is ambiguous, so a round trip through it would split one
// keyword into two.
void XmpEditingRoundTripsTest::aKeywordContainingACommaSurvivesAnUnrelatedEdit() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("photo.jpg");
    QVERIFY(writeJpeg(path));

    DocumentInfo doc(path);
    QVERIFY(doc.setEditableXmpTags(
        {{kSubject, {QStringLiteral("beach"), QStringLiteral("vacation, 2026")}}}));
    QCOMPARE(itemCountOnDisk(path, kSubject), 2);

    // Editing a different property must not disturb the keyword list.
    QVERIFY(doc.setEditableXmpTags({{kTitle, {QStringLiteral("A title")}}}));
    const QStringList keywords = DocumentInfo(path).getEditableXmpTags().value(kSubject);
    QCOMPARE(keywords.size(), 2);
    QCOMPARE(keywords.at(1), QStringLiteral("vacation, 2026"));
}

// Clearing the field a person can see must not silently destroy translations
// they cannot.
void XmpEditingRoundTripsTest::clearingATitleKeepsItsOtherLanguages() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("photo.jpg");
    QVERIFY(writeJpeg(path));

    {
        auto image = Exiv2::ImageFactory::open(path.toStdString());
        image->readMetadata();
        image->xmpData()["Xmp.dc.title"] = "lang=x-default English title";
        image->xmpData()["Xmp.dc.title"] = "lang=de-DE Deutscher Titel";
        image->writeMetadata();
    }
    QCOMPARE(itemCountOnDisk(path, kTitle), 2);

    DocumentInfo doc(path);
    QVERIFY(doc.setEditableXmpTags({{kTitle, {}}}));

    QCOMPARE(itemCountOnDisk(path, kTitle), 1);
    QVERIFY2(rawOnDisk(path, kTitle).contains(QStringLiteral("Deutscher Titel")),
             "clearing the x-default title must leave the de-DE one alone");
    QVERIFY2(DocumentInfo(path).getEditableXmpTags().value(kTitle).isEmpty(),
             "and the field itself should read back empty");
}

// Empty means remove, not "store an empty value" - a present-but-blank property
// would show up as an empty row everywhere else.
void XmpEditingRoundTripsTest::anEmptyCuratedValueRemovesTheProperty() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("photo.jpg");
    QVERIFY(writeJpeg(path));

    DocumentInfo doc(path);
    QVERIFY(doc.setEditableXmpTags({{kRating, {QStringLiteral("3")}},
                                    {kSubject, {QStringLiteral("beach")}}}));
    QVERIFY(doc.setEditableXmpTags({{kRating, {}}, {kSubject, {}}}));

    QCOMPARE(itemCountOnDisk(path, kRating), -1);
    QCOMPARE(itemCountOnDisk(path, kSubject), -1);
}

// The prefix binding travels in the packet, so a namespace this process never
// registered is still readable and still writable.
void XmpEditingRoundTripsTest::customPropertiesReadAndWriteWithoutRegisteringTheirNamespace() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("photo.jpg");
    QVERIFY(writeJpeg(path));

    DocumentInfo doc(path);
    QVERIFY2(doc.addCustomXmpProperty(QStringLiteral("tgtest"), QStringLiteral("ProjectCode"),
                                      QStringLiteral("http://thumbgrid.example/test/1.0/"),
                                      QStringLiteral("ALPHA-7")),
             "a new prefix with a namespace URI should be writable");
    QVERIFY(doc.setEditableXmpTags({{kTitle, {QStringLiteral("A standard property")}}}));

    DocumentInfo reread(path);
    const QMap<QString, QString> custom = reread.getCustomXmpTags();
    QCOMPARE(custom.size(), 1);
    QCOMPARE(custom.value(QStringLiteral("Xmp.tgtest.ProjectCode")), QStringLiteral("ALPHA-7"));
    // The split is what drives the two groups in the UI: a schema exiv2 knows is
    // standard, anything else is custom.
    QVERIFY(DocumentInfo::isRegisteredXmpKey(kTitle));
    QVERIFY(!DocumentInfo::isRegisteredXmpKey(QStringLiteral("Xmp.tgtest.ProjectCode")));

    // Editing a custom property in place, then erasing it, must leave the
    // standard one untouched.
    QVERIFY(reread.setCustomXmpTags({{QStringLiteral("Xmp.tgtest.ProjectCode"),
                                      QStringLiteral("BETA-9")}}));
    QCOMPARE(DocumentInfo(path).getCustomXmpTags().value(QStringLiteral("Xmp.tgtest.ProjectCode")),
             QStringLiteral("BETA-9"));
    QVERIFY(DocumentInfo(path).eraseXmpKey(QStringLiteral("Xmp.tgtest.ProjectCode")));
    QVERIFY(DocumentInfo(path).getCustomXmpTags().isEmpty());
    QCOMPARE(DocumentInfo(path).getEditableXmpTags().value(kTitle),
             QStringList{QStringLiteral("A standard property")});
}

// A grid row's key is its identity, so a key with no value yet is a legal state
// - and it has to survive later edits to other rows, or a half-filled row would
// vanish while the user was still working.
void XmpEditingRoundTripsTest::aKeyWithNoValueIsLegalAndStable() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("photo.jpg");
    QVERIFY(writeJpeg(path));

    DocumentInfo doc(path);
    QVERIFY(doc.addCustomXmpProperty(QStringLiteral("tgtest"), QStringLiteral("KeyOnly"),
                                     QStringLiteral("http://thumbgrid.example/test/1.0/"),
                                     QString()));
    QVERIFY(DocumentInfo(path).getCustomXmpTags().contains(QStringLiteral("Xmp.tgtest.KeyOnly")));

    // An unrelated write must not drop it.
    QVERIFY(DocumentInfo(path).setEditableXmpTags({{kTitle, {QStringLiteral("elsewhere")}}}));
    QVERIFY2(DocumentInfo(path).getCustomXmpTags().contains(QStringLiteral("Xmp.tgtest.KeyOnly")),
             "a key-only property must survive an edit to a different property");

    // Blanking a custom value keeps the key; only an erase removes it. This is
    // the opposite of the curated rows, and deliberately so: there the value is
    // the row's only handle, here the key is.
    QVERIFY(DocumentInfo(path).setCustomXmpTags(
        {{QStringLiteral("Xmp.tgtest.KeyOnly"), QString()}}));
    QVERIFY(DocumentInfo(path).getCustomXmpTags().contains(QStringLiteral("Xmp.tgtest.KeyOnly")));
    QVERIFY(DocumentInfo(path).eraseXmpKey(QStringLiteral("Xmp.tgtest.KeyOnly")));
    QVERIFY(!DocumentInfo(path).getCustomXmpTags().contains(QStringLiteral("Xmp.tgtest.KeyOnly")));
}

// Writing a prefix the file does not declare throws inside exiv2; the write path
// has to refuse it first so the failure is a message rather than an exception
// escaping into the dialog.
void XmpEditingRoundTripsTest::anUnknownPrefixNeedsANamespaceUri() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("photo.jpg");
    QVERIFY(writeJpeg(path));

    DocumentInfo doc(path);
    QVERIFY2(!DocumentInfo::isKnownXmpPrefix(QStringLiteral("tgunknown")),
             "the fixture prefix should not be known to exiv2");
    QVERIFY2(!doc.addCustomXmpProperty(QStringLiteral("tgunknown"), QStringLiteral("Thing"),
                                       QString(), QStringLiteral("value")),
             "a new prefix with no namespace URI must be refused");
    QVERIFY(DocumentInfo(path).getCustomXmpTags().isEmpty());

    // A prefix exiv2 already knows needs no URI at all.
    QVERIFY(DocumentInfo::isKnownXmpPrefix(QStringLiteral("dc")));
}

// The isolation that lets three removal buttons sit in three tabs.
void XmpEditingRoundTripsTest::eachScopedRemovalLeavesTheOtherKindsIntact() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());

    auto seed = [](const QString &path) {
        QVERIFY(writeJpeg(path));
        auto image = Exiv2::ImageFactory::open(path.toStdString());
        image->readMetadata();
        image->exifData()["Exif.Image.Make"] = "TestCam";
        image->xmpData()["Xmp.dc.title"] = "lang=x-default keep or go";
        image->writeMetadata();
    };

    // Exif only.
    const QString exifPath = root.filePath("exif.jpg");
    seed(exifPath);
    QVERIFY(DocumentInfo(exifPath).clearExifMetadata());
    QVERIFY2(!DocumentInfo(exifPath).hasExifMetadata(), "Exif should be gone");
    QVERIFY2(DocumentInfo(exifPath).hasXmpMetadata(), "XMP must survive an Exif removal");
    QVERIFY2(DocumentInfo(exifPath).hasIccProfile(), "the profile must survive an Exif removal");

    // XMP only.
    const QString xmpPath = root.filePath("xmp.jpg");
    seed(xmpPath);
    QVERIFY(DocumentInfo(xmpPath).clearXmpMetadata());
    QVERIFY2(!DocumentInfo(xmpPath).hasXmpMetadata(), "XMP should be gone");
    QVERIFY2(DocumentInfo(xmpPath).hasExifMetadata(), "Exif must survive an XMP removal");
    QVERIFY2(DocumentInfo(xmpPath).hasIccProfile(), "the profile must survive an XMP removal");

    // Profile only.
    const QString iccPath = root.filePath("icc.jpg");
    seed(iccPath);
    QVERIFY(DocumentInfo(iccPath).hasIccProfile());
    QVERIFY(DocumentInfo(iccPath).clearIccProfile());
    QVERIFY2(!DocumentInfo(iccPath).hasIccProfile(), "the profile should be gone");
    QVERIFY2(DocumentInfo(iccPath).hasExifMetadata(), "Exif must survive a profile removal");
    QVERIFY2(DocumentInfo(iccPath).hasXmpMetadata(), "XMP must survive a profile removal");
}

// The write gate is wider than Exif's (png is writable here) and narrower than
// the read gate (tiff is not), so it is worth pinning rather than inferring.
void XmpEditingRoundTripsTest::onlyWritableFormatsAreOffered() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());

    const QString pngPath = root.filePath("a.png");
    QVERIFY(writeImageAs(pngPath, "png"));
    QVERIFY2(DocumentInfo::supportsXmpEditing(pngPath),
             "a png takes XMP even though exiv2 will not write Exif into it");
    QVERIFY(DocumentInfo(pngPath).setEditableXmpTags({{kTitle, {QStringLiteral("on a png")}}}));
    QCOMPARE(DocumentInfo(pngPath).getEditableXmpTags().value(kTitle),
             QStringList{QStringLiteral("on a png")});
    QVERIFY2(DocumentInfo(pngPath).hasIccProfile(),
             "the colour profile must survive an XMP write on a png");

    // A tiff can hold XMP and is deliberately not writable: exiv2 rewrites its
    // whole structure, and its Exif and ICC scopes overlap, so no removal there
    // could be scoped to one kind.
    const QString tiffPath = root.filePath("a.tiff");
    QVERIFY(writeImageAs(tiffPath, "tiff"));
    QVERIFY(DocumentInfo::supportsXmp(tiffPath));
    QVERIFY2(!DocumentInfo::supportsXmpEditing(tiffPath), "tiff is read-only for XMP");
    QVERIFY2(!DocumentInfo::supportsIccEditing(tiffPath), "tiff is read-only for its profile");
}

// The caps exist to keep a maliciously large packet from being writable one
// property at a time; they are checked before the file is opened.
void XmpEditingRoundTripsTest::oversizedKeysAndValuesAreRejectedBeforeTheFileIsOpened() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("photo.jpg");
    QVERIFY(writeJpeg(path));

    DocumentInfo doc(path);
    const QString hugeValue(DocumentInfo::kMaxXmpValueLengthEdited + 1, QLatin1Char('x'));
    QVERIFY2(!doc.setEditableXmpTags({{kTitle, {hugeValue}}}),
             "a value past the edit cap should be refused");
    QVERIFY(DocumentInfo(path).getEditableXmpTags().value(kTitle).isEmpty());

    const QString hugeName(DocumentInfo::kMaxXmpKeyLength, QLatin1Char('n'));
    QVERIFY2(!doc.addCustomXmpProperty(QStringLiteral("dc"), hugeName, QString(),
                                       QStringLiteral("v")),
             "a key past the length cap should be refused");
}

TG_BEHAVIOR_TEST_MAIN(XmpEditingRoundTripsTest)

#include "test_xmp_editing_round_trips.moc"
