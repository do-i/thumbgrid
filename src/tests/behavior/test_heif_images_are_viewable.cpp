#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QImageWriter>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "sourcecontainers/documentinfo.h"
#include "sourcecontainers/imagestatic.h"

// heif/heic decoding always comes from an optional Qt image plugin
// (kimageformats + libheif on Linux), never from Qt itself. The packages
// declare it as a hard dependency, but a developer build on a machine without
// it is legitimate - so every check here is skipped rather than failed when the
// plugin is absent.
class HeifImagesAreViewableTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void aHeifFileIsDetectedAsAStaticImage();
    void aHeifFileAppearsInTheGrid();
    void heifMetadataIsReadOnly();
    void tinyImagesAreNotEncodedAsHeif();

private:
    bool writeHeif(const QString &path);
};

void HeifImagesAreViewableTest::init() {
    if(!QImageReader::supportedImageFormats().contains("heic"))
        QSKIP("No heif image plugin available (install kimageformats + libheif).");
}

// Deliberately larger than the encoder floor checked in
// tinyImagesAreNotEncodedAsHeif() - the point of this fixture is a file that
// round-trips exactly.
bool HeifImagesAreViewableTest::writeHeif(const QString &path) {
    QImage image(160, 120, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    QImageWriter writer(path, "heic");
    return writer.write(image);
}

void HeifImagesAreViewableTest::aHeifFileIsDetectedAsAStaticImage() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    const QString heicPath = QDir(fixture.path()).filePath("photo.heic");
    QVERIFY2(writeHeif(heicPath), "A heic fixture should be written.");

    DocumentInfo info(heicPath);
    QCOMPARE(info.type(), DocumentType::STATIC);
    // The reader is given this as an explicit format hint and does not fall back
    // to sniffing, so it has to be a key the plugin actually registered.
    QCOMPARE(info.format(), QString("heic"));
    QVERIFY2(QImageReader::supportedImageFormats().contains(info.format().toUtf8()),
             "The detected format should be a key the image plugin registered.");

    // Detection is only half of it - the format above is handed to QImageReader
    // as an explicit hint, and a hint the plugin does not know decodes to a
    // blank image rather than failing loudly.
    ImageStatic loaded(heicPath);
    QVERIFY2(loaded.isLoaded(), "The heic should load.");
    QCOMPARE(loaded.width(), 160);
    QCOMPARE(loaded.height(), 120);
}

void HeifImagesAreViewableTest::aHeifFileAppearsInTheGrid() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY2(writeHeif(root.filePath("photo.heic")), "A heic fixture should be written.");

    Core core;
    QVERIFY2(core.loadPath(fixture.path()), "Opening the fixture folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    auto grid = tgtest::mainWindow()->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");
    // Parent ".." plus the heic file.
    QTRY_COMPARE(grid->itemCount(), 2);
}

void HeifImagesAreViewableTest::heifMetadataIsReadOnly() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    const QString heicPath = QDir(fixture.path()).filePath("photo.heic");
    QVERIFY2(writeHeif(heicPath), "A heic fixture should be written.");

    // exiv2 refuses to write into BMFF containers, so every path that would
    // rewrite the file has to stay closed - otherwise the user is offered an
    // irreversible-sounding action that can only fail.
    QVERIFY2(!DocumentInfo::supportsMetadataWriting(heicPath),
             "heif containers should not be offered for metadata rewriting.");
    QVERIFY2(!DocumentInfo::supportsMetadataEditing(heicPath),
             "The tag editor should stay disabled for heif containers.");
    QVERIFY2(!DocumentInfo::supportsXmpEditing(heicPath),
             "XMP editing should stay disabled for heif containers.");
    // Reading is a different question: exiv2 parses heif metadata fine.
    QVERIFY2(DocumentInfo::supportsXmp(heicPath),
             "The read-only XMP tab should still be offered for heif containers.");
}

// libheif's x265 backend cannot describe a frame smaller than one 64x64 coding
// tree unit and does not say so - it writes a transposed or garbled picture and
// reports success. Nothing may reach the encoder below that floor.
void HeifImagesAreViewableTest::tinyImagesAreNotEncodedAsHeif() {
    QVERIFY2(!DocumentInfo::canEncodeToFormat("heic", QSize(64, 48)),
             "A picture shorter than the encoder floor should be refused.");
    QVERIFY2(!DocumentInfo::canEncodeToFormat("heif", QSize(32, 32)),
             "A picture smaller than the encoder floor should be refused.");
    QVERIFY2(DocumentInfo::canEncodeToFormat("heic", QSize(160, 120)),
             "An ordinary picture should still be encodable as heic.");
    QVERIFY2(DocumentInfo::canEncodeToFormat("png", QSize(1, 1)),
             "The floor is a heif encoder limit and must not touch other formats.");

    // And the refusal has to hold at the write itself, not only in the gate the
    // conversion menu consults.
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    const QString pngPath = root.filePath("tiny.png");
    QVERIFY2(tgtest::writeImage(pngPath, Qt::red), "A small png fixture should be written.");

    ImageStatic tiny(pngPath);
    QVERIFY2(tiny.isLoaded(), "The png fixture should load.");
    const QString heicPath = root.filePath("tiny.heic");
    QVERIFY2(!tiny.save(heicPath), "Saving a sub-floor image as heic should fail.");
    QVERIFY2(!QFile::exists(heicPath), "No heic file should be left behind.");
}

TG_BEHAVIOR_TEST_MAIN(HeifImagesAreViewableTest)

#include "test_heif_images_are_viewable.moc"
