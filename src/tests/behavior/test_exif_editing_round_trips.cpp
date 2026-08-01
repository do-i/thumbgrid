// Tier-1 metadata editing: the four text-valued Exif tags a person actually
// retypes, on the two formats exiv2 can safely rewrite.
//
// These drive DocumentInfo rather than the UI on purpose - the risk here is
// entirely in exiv2's value handling, not in the rows. In particular
// UserComment is an Undefined-typed value carrying a "charset=" header that
// exiv2 includes in its string form; getting that wrong silently stores the
// header as part of the comment text, which no UI test would catch. The
// in-place fields that reach these calls are covered by
// test_file_info_inline_metadata_editing.cpp.

#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <exiv2/exiv2.hpp>

#include "sourcecontainers/documentinfo.h"

class ExifEditingRoundTripsTest : public QObject {
    Q_OBJECT

private slots:
    void allFourTagsRoundTripOnJpegAndWebp();
    void anEmptyValueRemovesTheTag();
    void anInvalidDateTimeIsRejectedAndNothingIsWritten();
    void onlyRewritableFormatsAreOffered();
};

namespace {

const QString kMake    = QStringLiteral("Exif.Image.Make");
const QString kModel   = QStringLiteral("Exif.Image.Model");
const QString kDate    = QStringLiteral("Exif.Image.DateTime");
const QString kComment = QStringLiteral("Exif.Photo.UserComment");

bool writeImageAs(const QString &path, const char *format) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    return image.save(path, format);
}

} // namespace

void ExifEditingRoundTripsTest::allFourTagsRoundTripOnJpegAndWebp() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());

    // Non-ASCII in the comment on purpose. Round-tripping it through our own
    // reader is NOT enough to prove the encoding is right: exiv2 hands back
    // whatever bytes it stored, so a wrongly-labelled charset=Ascii comment
    // still compares equal here. The stored label is asserted separately below,
    // because that is what every *other* reader goes by.
    const QString comment = QStringLiteral("Café — naïve ☕");

    struct Case { const char *format; const char *file; };
    const QList<Case> cases = {{"JPG", "photo.jpg"}, {"WEBP", "photo.webp"}};

    for(const Case &c : cases) {
        const QString path = root.filePath(QString::fromLatin1(c.file));
        QVERIFY2(writeImageAs(path, c.format), qPrintable(QStringLiteral("write %1").arg(path)));
        QVERIFY2(DocumentInfo::supportsMetadataEditing(path),
                 qPrintable(QStringLiteral("%1 should be editable").arg(c.file)));

        QMap<QString, QString> values;
        values.insert(kMake, QStringLiteral("TestCam"));
        values.insert(kModel, QStringLiteral("Model X"));
        values.insert(kDate, QStringLiteral("2026:07:26 10:30:00"));
        values.insert(kComment, comment);

        DocumentInfo writer(path);
        QVERIFY2(writer.setEditableTags(values),
                 qPrintable(QStringLiteral("writing tags to %1 should succeed").arg(c.file)));

        // Fresh DocumentInfo: proves the values are on disk, not just cached.
        DocumentInfo reader(path);
        const QMap<QString, QString> read = reader.getEditableTags();
        QCOMPARE(read.value(kMake), QStringLiteral("TestCam"));
        QCOMPARE(read.value(kModel), QStringLiteral("Model X"));
        QCOMPARE(read.value(kDate), QStringLiteral("2026:07:26 10:30:00"));
        // The charset header is an encoding detail and must never reach the UI.
        QVERIFY2(!read.value(kComment).startsWith(QLatin1String("charset=")),
                 "the charset= prefix must be stripped from the comment");
        QCOMPARE(read.value(kComment), comment);

        // The label as actually written to the file. exiv2 will happily store
        // UTF-8 under charset=Ascii, and other tools then mangle it.
        std::unique_ptr<Exiv2::Image> raw = Exiv2::ImageFactory::open(path.toStdString());
        QVERIFY(raw.get() != nullptr);
        raw->readMetadata();
        auto it = raw->exifData().findKey(Exiv2::ExifKey(kComment.toStdString()));
        QVERIFY(it != raw->exifData().end());
        const QString stored = QString::fromStdString(it->value().toString());
        QVERIFY2(stored.startsWith(QLatin1String("charset=Unicode")),
                 qPrintable(QStringLiteral("non-ASCII comment must be labelled Unicode, got: %1")
                                .arg(stored.left(24))));
    }
}

void ExifEditingRoundTripsTest::anEmptyValueRemovesTheTag() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath(QStringLiteral("p.jpg"));
    QVERIFY(writeImageAs(path, "JPG"));

    DocumentInfo writer(path);
    QVERIFY(writer.setEditableTags({{kMake, QStringLiteral("TestCam")}}));
    QCOMPARE(DocumentInfo(path).getEditableTags().value(kMake), QStringLiteral("TestCam"));

    // Blanking a field must delete the tag, not leave a present-but-empty one -
    // otherwise every other view shows a blank row where there is no data.
    QVERIFY(writer.setEditableTags({{kMake, QString()}}));
    QVERIFY2(!DocumentInfo(path).getEditableTags().contains(kMake),
             "an emptied field must remove the tag rather than store an empty string");
}

void ExifEditingRoundTripsTest::anInvalidDateTimeIsRejectedAndNothingIsWritten() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath(QStringLiteral("p.jpg"));
    QVERIFY(writeImageAs(path, "JPG"));

    QVERIFY(DocumentInfo(path).setEditableTags({{kMake, QStringLiteral("Before")}}));

    // Exif.Image.DateTime has exactly one legal form. A rejected edit must be
    // all-or-nothing: the valid Make in the same batch must not land either.
    DocumentInfo writer(path);
    QVERIFY2(!writer.setEditableTags({{kMake, QStringLiteral("After")},
                                      {kDate, QStringLiteral("26/07/2026 10:30")}}),
             "an invalid Date/Time must be rejected");

    const QMap<QString, QString> read = DocumentInfo(path).getEditableTags();
    QCOMPARE(read.value(kMake), QStringLiteral("Before"));
    QVERIFY2(!read.contains(kDate), "the rejected batch must not have written a date");

    QVERIFY(DocumentInfo::isValidExifDateTime(QStringLiteral("2026:07:26 10:30:00")));
    QVERIFY(!DocumentInfo::isValidExifDateTime(QStringLiteral("2026-07-26 10:30:00")));
    QVERIFY(!DocumentInfo::isValidExifDateTime(QStringLiteral("2026:13:99 10:30:00")));
}

void ExifEditingRoundTripsTest::onlyRewritableFormatsAreOffered() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());

    const QString png = root.filePath(QStringLiteral("a.png"));
    const QString bmp = root.filePath(QStringLiteral("a.bmp"));
    QVERIFY(writeImageAs(png, "PNG"));
    QVERIFY(writeImageAs(bmp, "BMP"));

    // exiv2 can *read* more formats than tier 1 offers to rewrite. Offering an
    // editor that fails on save would be worse than not offering one.
    QVERIFY(!DocumentInfo::supportsMetadataEditing(png));
    QVERIFY(!DocumentInfo::supportsMetadataEditing(bmp));
    QVERIFY(!DocumentInfo::supportsMetadataEditing(root.filePath(QStringLiteral("a.mp4"))));
    QVERIFY(DocumentInfo::supportsMetadataEditing(root.filePath(QStringLiteral("a.JPEG"))));
}

TG_BEHAVIOR_TEST_MAIN(ExifEditingRoundTripsTest)

#include "test_exif_editing_round_trips.moc"
