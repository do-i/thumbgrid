// The File info window's EXIF tab is where a file gets written: its four
// writable Exif tags are in-place fields, committed on Enter or focus-out, with
// no Save button and no separate edit dialog.
//
// Driven through Core rather than the dialog alone, because the interesting
// part is the round trip - the commit hands off to Core, which writes, reloads
// and retargets the window, which rebuilds the very row that emitted the
// signal.

#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTemporaryDir>

#include <exiv2/exiv2.hpp>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "sourcecontainers/documentinfo.h"

class FileInfoInlineMetadataEditingTest : public QObject {
    Q_OBJECT

private slots:
    void exifFieldsWriteOnCommitAndRefuseAMalformedDate();
};

namespace {

const QString kMake = QStringLiteral("Exif.Image.Make");
const QString kDate = QStringLiteral("Exif.Image.DateTime");

int tabIndexByText(QTabWidget *tabs, const QString &text) {
    for(int i = 0; i < tabs->count(); ++i) {
        if(tabs->tabText(i) == text)
            return i;
    }
    return -1;
}

bool writeJpegWithExifMake(const QString &path, const std::string &make) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::darkGreen);
    if(!image.save(path, "JPG"))
        return false;
    try {
        std::unique_ptr<Exiv2::Image> exivImage = Exiv2::ImageFactory::open(path.toStdString());
        if(!exivImage.get())
            return false;
        exivImage->readMetadata();
        exivImage->exifData()["Exif.Image.Make"] = make;
        exivImage->writeMetadata();
    } catch(Exiv2::Error &) {
        return false;
    }
    return true;
}

// Types into a row's field and commits it the way a user does. Enter is used
// rather than a focus change so the test does not depend on another focusable
// widget being there to receive it.
void commitField(EntryInfoItem *row, const QString &text) {
    QLineEdit *editor = row->valueEditor();
    QVERIFY(editor != nullptr);
    editor->setText(text);
    QTest::keyClick(editor, Qt::Key_Return);
}

} // namespace

void FileInfoInlineMetadataEditingTest::exifFieldsWriteOnCommitAndRefuseAMalformedDate() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString photoPath = root.filePath("gallery/photo.jpg");
    QVERIFY2(writeJpegWithExifMake(photoPath, "OldCam"), "Tagged jpeg should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");
    // Parent ".." (0), then photo.jpg (1).
    QTRY_COMPARE(grid->itemCount(), 2);
    grid->select(1);

    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable.");
    auto *dialog = window->findChild<FileInfoDialog *>();
    QTRY_VERIFY2(dialog != nullptr, "The File info popup should exist.");
    QTRY_VERIFY2(dialog->isVisible(), "The File info popup should be visible.");

    const int exifIndex = tabIndexByText(dialog->tabs(), "EXIF");
    QVERIFY(exifIndex >= 0);
    QTRY_VERIFY2(dialog->tabs()->isTabEnabled(exifIndex), "EXIF tab should be enabled for a jpeg.");
    dialog->tabs()->setCurrentIndex(exifIndex);

    // --- Committing a field writes the tag to the file. ---
    EntryInfoItem *makeRow = dialog->editableRow(kMake);
    QVERIFY2(makeRow != nullptr, "Make should have an editable row.");
    QCOMPARE(makeRow->valueEditor()->text(), QStringLiteral("OldCam"));
    commitField(makeRow, QStringLiteral("NewCam"));

    // Queued through Core (see Core::showFileInfoDialog), so the write lands on
    // a later event-loop turn - and it must land on disk, not just in the row.
    QTRY_COMPARE(DocumentInfo(photoPath).getEditableTags().value(kMake), QStringLiteral("NewCam"));
    // The window was retargeted afterwards, so the rebuilt row shows the file's
    // current value rather than a stale one.
    QTRY_VERIFY(dialog->editableRow(kMake) != nullptr);
    QTRY_COMPARE(dialog->editableRow(kMake)->valueEditor()->text(), QStringLiteral("NewCam"));

    // --- A malformed date is refused, explained, and reverted. ---
    EntryInfoItem *dateRow = dialog->editableRow(kDate);
    QVERIFY2(dateRow != nullptr, "Date/Time should have an editable row.");
    QVERIFY2(dateRow->valueEditor()->text().isEmpty(), "the file has no date yet");
    commitField(dateRow, QStringLiteral("26/07/2026"));

    auto *error = dialog->findChild<QLabel *>(QStringLiteral("metadataEditError"));
    QVERIFY(error != nullptr);
    QVERIFY2(error->isVisibleTo(dialog), "the reason must be shown, not just the refusal");
    QVERIFY2(dialog->editableRow(kDate)->valueEditor()->text().isEmpty(),
             "a rejected value must not stay in a row that reports what the file contains");
    // Nothing was written, and the valid tag from before is untouched.
    const QMap<QString, QString> afterBadDate = DocumentInfo(photoPath).getEditableTags();
    QVERIFY2(!afterBadDate.contains(kDate), "a malformed date must never reach the file");
    QCOMPARE(afterBadDate.value(kMake), QStringLiteral("NewCam"));

    // A well-formed one goes through, and clears the error.
    commitField(dialog->editableRow(kDate), QStringLiteral("2026:07:26 10:30:00"));
    QTRY_COMPARE(DocumentInfo(photoPath).getEditableTags().value(kDate),
                 QStringLiteral("2026:07:26 10:30:00"));
    QVERIFY2(!error->isVisibleTo(dialog), "the error should clear once a value is accepted");

    // --- Emptying a field removes the tag rather than storing a blank one. ---
    commitField(dialog->editableRow(kMake), QString());
    QTRY_VERIFY2(!DocumentInfo(photoPath).getEditableTags().contains(kMake),
                 "an emptied field must remove the tag");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoInlineMetadataEditingTest)

#include "test_file_info_inline_metadata_editing.moc"
