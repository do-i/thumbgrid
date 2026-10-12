#include "support/thumbgrid_test_support.h"

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "settings.h"
#include "sourcecontainers/documentinfo.h"

// A heic straight off a phone carries a full Exif block, which exiv2 reads as
// readily as a jpeg's. The EXIF tab used to be gated on *writing* (jpeg/webp),
// so a heic showed none of it - the tab has to open for reading alone, and stay
// free of anything that would try to write.
class FileInfoListsHeifExifTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void aHeifListsItsExifReadOnly();
};

namespace {

// Same lookup as the other file-info tests: the first QLabel of a row is its
// name, the second its value (full text in the tooltip once elided).
QString rowValueByName(QWidget *root, const QString &rowName) {
    for(EntryInfoItem *item : root->findChildren<EntryInfoItem *>()) {
        const QList<QLabel *> labels = item->findChildren<QLabel *>();
        if(labels.size() < 2 || labels.at(0)->text() != rowName)
            continue;
        const QString tip = labels.at(1)->toolTip();
        return tip.isEmpty() ? labels.at(1)->text() : tip;
    }
    return QString();
}

int tabIndexByText(QTabWidget *tabs, const QString &text) {
    for(int i = 0; i < tabs->count(); ++i) {
        if(tabs->tabText(i) == text)
            return i;
    }
    return -1;
}

// exiv2 cannot write into a heif, so the Exif comes from the encoder: the heif
// plugin turns QImage text into an Exif block on write.
bool writeHeifWithExif(const QString &path) {
    QImage image(160, 120, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    image.setText(QStringLiteral("Manufacturer"), QStringLiteral("HeifCam"));
    image.setText(QStringLiteral("Model"), QStringLiteral("HeifCam One"));
    image.setText(QStringLiteral("Software"), QStringLiteral("thumbgrid-test"));
    QImageWriter writer(path, "heic");
    return writer.write(image);
}

} // namespace

void FileInfoListsHeifExifTest::init() {
    if(!QImageReader::supportedImageFormats().contains("heic"))
        QSKIP("No heif image plugin available (install kimageformats + libheif).");
}

void FileInfoListsHeifExifTest::aHeifListsItsExifReadOnly() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "gallery folder should be created.");
    const QString heicPath = root.filePath("gallery/photo.heic");
    QVERIFY2(writeHeifWithExif(heicPath), "A heic fixture should be written.");

    QVERIFY2(DocumentInfo::supportsExif(heicPath), "heif Exif should be readable.");
    const QMap<QString, QString> allTags = DocumentInfo(heicPath).getAllTags();
    if(!allTags.contains(QStringLiteral("Exif.Image.Software")))
        QSKIP("This heif plugin does not embed Exif on write.");

    settings->setShowFullMetadata(false);

    Core core;
    QVERIFY2(core.loadPath(root.filePath("gallery")), "Opening the gallery should succeed.");
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");
    // Parent ".." (0), then photo.heic (1).
    QTRY_COMPARE(grid->itemCount(), 2);
    grid->select(1);
    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable.");

    auto *dialog = window->findChild<FileInfoDialog *>();
    QTRY_VERIFY2(dialog != nullptr && dialog->isVisible(), "The File info popup should be visible.");
    const int exifIndex = tabIndexByText(dialog->tabs(), "EXIF");
    QVERIFY2(exifIndex >= 0, "The EXIF tab should exist.");
    QTRY_VERIFY2(dialog->tabs()->isTabVisible(exifIndex), "A heic should get the EXIF tab.");
    dialog->tabs()->setCurrentIndex(exifIndex);

    // The short summary, as text: nothing here may be offered as a field.
    QTRY_COMPARE(rowValueByName(dialog, "Make"), QStringLiteral("HeifCam"));
    QVERIFY2(dialog->editableRow(QStringLiteral("Exif.Image.Make")) == nullptr,
             "a heic tag must not be editable - exiv2 cannot write it back");
    QVERIFY2(!dialog->removeExifButton()->isEnabled(),
             "Exif removal should stay disabled for a heic.");
    QVERIFY2(rowValueByName(dialog, "Exif.Image.Software").isEmpty(),
             "the summary should not list the full dump");

    // One click from the tab itself reaches every tag the file holds.
    auto *showAll = dialog->findChild<QCheckBox *>(QStringLiteral("showAllTagsCheckBox"));
    QVERIFY2(showAll != nullptr && showAll->isVisibleTo(dialog),
             "the EXIF tab should offer to show every tag");
    QVERIFY(!showAll->isChecked());
    showAll->setChecked(true);
    QVERIFY2(settings->showFullMetadata(), "the checkbox should drive the existing setting");
    QTRY_COMPARE(rowValueByName(dialog, "Exif.Image.Software"), QStringLiteral("thumbgrid-test"));
    QCOMPARE(rowValueByName(dialog, "Exif.Image.Model"), QStringLiteral("HeifCam One"));
    QVERIFY2(dialog->editableRow(QStringLiteral("Exif.Image.Make")) == nullptr,
             "the full list stays read-only for a heic too");

    showAll->setChecked(false);
    QVERIFY(!settings->showFullMetadata());
    QTRY_VERIFY(rowValueByName(dialog, "Exif.Image.Software").isEmpty());
}

TG_BEHAVIOR_TEST_MAIN(FileInfoListsHeifExifTest)

#include "test_file_info_lists_heif_exif.moc"
