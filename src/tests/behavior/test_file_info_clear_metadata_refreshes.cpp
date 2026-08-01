// Clearing metadata from the File info window has to update the window that
// raised it. Nothing else does: the selection has not changed, so the grid's
// statusTextChanged - the signal Core live-follows the popup on - never fires,
// and the EXIF tab would keep listing tags the file no longer has until the
// window was closed and reopened.

#include "support/thumbgrid_test_support.h"

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <exiv2/exiv2.hpp>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "sourcecontainers/documentinfo.h"

class FileInfoClearMetadataRefreshesTest : public QObject {
    Q_OBJECT

private slots:
    void clearingMetadataUpdatesTheOpenExifTab();
};

namespace {

int tabIndexByText(QTabWidget *tabs, const QString &text) {
    for(int i = 0; i < tabs->count(); ++i) {
        if(tabs->tabText(i) == text)
            return i;
    }
    return -1;
}

// Read-only EXIF rows keep their value in a QLabel; editable ones in a
// QLineEdit. Returns an empty string when there is no such row at all.
QString rowValueByName(QWidget *root, const QString &rowName) {
    for(EntryInfoItem *item : root->findChildren<EntryInfoItem *>()) {
        const QList<QLabel *> labels = item->findChildren<QLabel *>();
        if(labels.size() < 2 || labels.at(0)->text() != rowName)
            continue;
        if(QLineEdit *editor = item->valueEditor())
            return editor->text();
        return labels.at(1)->text();
    }
    return QString();
}

bool writeTaggedJpeg(const QString &path) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::magenta);
    if(!image.save(path, "JPG"))
        return false;
    try {
        std::unique_ptr<Exiv2::Image> exivImage = Exiv2::ImageFactory::open(path.toStdString());
        if(!exivImage.get())
            return false;
        exivImage->readMetadata();
        Exiv2::ExifData &exifData = exivImage->exifData();
        // One editable tag and one that is only ever read, so the refresh is
        // proven for both kinds of row.
        exifData["Exif.Image.Make"] = "TestCam";
        exifData["Exif.Photo.FNumber"] = Exiv2::Rational(28, 10);
        exivImage->writeMetadata();
    } catch(Exiv2::Error &) {
        return false;
    }
    return true;
}

} // namespace

void FileInfoClearMetadataRefreshesTest::clearingMetadataUpdatesTheOpenExifTab() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString photoPath = root.filePath("gallery/photo.jpg");
    QVERIFY2(writeTaggedJpeg(photoPath), "Tagged jpeg should be written.");

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
    QTRY_VERIFY2(dialog->tabs()->isTabEnabled(exifIndex), "EXIF tab should be enabled for a tagged jpeg.");
    dialog->tabs()->setCurrentIndex(exifIndex);
    QTRY_COMPARE(rowValueByName(dialog, "Make"), QStringLiteral("TestCam"));
    QCOMPARE(rowValueByName(dialog, "F Number"), QStringLiteral("f/2.8"));

    QPushButton *clearButton = dialog->stripMetadataButton();
    QVERIFY(clearButton != nullptr);
    QVERIFY2(clearButton->isEnabled(), "Clear metadata should be enabled for a writable jpeg.");

    // The confirmation runs a nested exec() loop, so it can only be answered
    // from a timer: poll for the active modal and press its danger button.
    bool confirmed = false;
    QTimer poll;
    poll.setInterval(50);
    QObject::connect(&poll, &QTimer::timeout, [&] {
        auto *box = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if(!box)
            return;
        poll.stop();
        confirmed = box->findChild<QPushButton *>("dangerButton") != nullptr;
        box->accept();
    });
    poll.start();
    clearButton->click();
    QVERIFY2(confirmed, "Clearing metadata must confirm first, with a danger-styled accept.");

    // On disk...
    QTRY_VERIFY2(DocumentInfo(photoPath).getEditableTags().isEmpty(),
                 "the tags should be gone from the file");
    // ...and in the window that is still open on it, with no reopen needed.
    QTRY_VERIFY2(rowValueByName(dialog, "F Number").isEmpty(),
                 "a read-only row for a deleted tag must not survive the clear");
    QVERIFY2(rowValueByName(dialog, "Make").isEmpty(),
             "the Make field must come back empty, not still reading TestCam");
    // The jpeg is still writable, so its four fields stay - empty ones to type
    // into, which is why the tab does not go away with the tags.
    QVERIFY2(dialog->editableRow(QStringLiteral("Exif.Image.Make")) != nullptr,
             "a writable jpeg keeps its input fields after a clear");
    QVERIFY(dialog->tabs()->isTabEnabled(exifIndex));

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoClearMetadataRefreshesTest)

#include "test_file_info_clear_metadata_refreshes.moc"
