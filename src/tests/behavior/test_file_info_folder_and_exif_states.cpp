#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>

#include <exiv2/exiv2.hpp>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class FileInfoFolderAndExifStatesTest : public QObject {
    Q_OBJECT

private slots:
    void folderShowsCountsAndExifTabTracksTheSelectedFile();
};

namespace {

// Every General/EXIF row is an EntryInfoItem with two QLabel children (name,
// then value) added in that order by EntryInfoItem's constructor. Rows for
// both tabs are children of the dialog regardless of which tab is current, so
// searching the whole dialog and matching on the name label finds any row.
// The value label elides long text and moves the full string to its tooltip
// (EntryInfoItem::updateElidedText), so prefer the tooltip when set. An
// editable row shows its value in a QLineEdit instead, which never elides.
QString rowValueByName(QWidget *root, const QString &rowName) {
    for(EntryInfoItem *item : root->findChildren<EntryInfoItem *>()) {
        const QList<QLabel *> labels = item->findChildren<QLabel *>();
        if(labels.size() < 2 || labels.at(0)->text() != rowName)
            continue;
        if(QLineEdit *editor = item->valueEditor())
            return editor->text();
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

// A jpeg with a single Exif tag set (Exif.Image.Make), which DocumentInfo's
// getExifTags() surfaces as a "Make" row.
bool writeJpegWithExifMake(const QString &path, const QColor &color, const std::string &make) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(color);
    if(!image.save(path, "JPG"))
        return false;
    try {
        std::unique_ptr<Exiv2::Image> exivImage = Exiv2::ImageFactory::open(path.toStdString());
        if(!exivImage.get())
            return false;
        exivImage->readMetadata();
        Exiv2::ExifData &exifData = exivImage->exifData();
        exifData["Exif.Image.Make"] = make;
        exivImage->writeMetadata();
    } catch(Exiv2::Error &) {
        return false;
    }
    return true;
}

} // namespace

void FileInfoFolderAndExifStatesTest::folderShowsCountsAndExifTabTracksTheSelectedFile() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/sub/subsub"), "sub/subsub folders should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString subPath = root.filePath("gallery/sub");
    const QString taggedPath = root.filePath("gallery/tagged.jpg");
    const QString untaggedPath = root.filePath("gallery/untagged.png");

    {
        // Direct children of sub: 2 files (f1.txt, f2.txt) + 1 folder (subsub).
        QFile f1(root.filePath("gallery/sub/f1.txt"));
        QVERIFY(f1.open(QIODevice::WriteOnly));
        f1.write("one");
        QFile f2(root.filePath("gallery/sub/f2.txt"));
        QVERIFY(f2.open(QIODevice::WriteOnly));
        f2.write("two");
    }
    QVERIFY2(writeJpegWithExifMake(taggedPath, Qt::red, "TestCam"), "Tagged jpeg should be written.");
    QVERIFY2(tgtest::writeImage(untaggedPath, Qt::blue), "Untagged png should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), then dirs sorted (sub=1), then files sorted
    // (tagged.jpg=2, untagged.png=3).
    QTRY_COMPARE(grid->itemCount(), 4);
    const int subDir = 1;
    const int taggedJpg = 2;
    const int untaggedPng = 3;

    // --- Folder target: counts shown, EXIF tab hidden. ---
    grid->select(subDir);
    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable.");

    auto *dialog = window->findChild<FileInfoDialog *>();
    QTRY_VERIFY2(dialog != nullptr, "The File info popup should exist.");
    QTRY_VERIFY2(dialog->isVisible(), "The File info popup should be visible.");
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(subPath).absoluteFilePath());
    QTRY_COMPARE(rowValueByName(dialog, "Contains"), QStringLiteral("2 file(s), 1 folder(s)"));

    const int exifIndex = tabIndexByText(dialog->tabs(), "EXIF");
    const int generalIndex = tabIndexByText(dialog->tabs(), "General");
    QVERIFY2(exifIndex >= 0 && generalIndex >= 0, "Both tabs should exist.");
    QVERIFY2(!dialog->tabs()->isTabVisible(exifIndex), "EXIF tab should be hidden for a folder target.");

    // Clear metadata lives here rather than in the context menu, and it belongs
    // to the EXIF tab: General is a read-only view and must offer no action at
    // all. A folder cannot even reach the EXIF tab, so the button is away.
    QPushButton *clearButton = dialog->stripMetadataButton();
    QVERIFY2(clearButton != nullptr, "The File info window should offer a clear metadata button.");
    QCOMPARE(dialog->tabs()->currentIndex(), generalIndex);
    QVERIFY2(!clearButton->isVisibleTo(dialog),
             "the read-only General tab should show no action buttons");
    QVERIFY2(!clearButton->isEnabled(), "Clear metadata should be disabled for a folder target.");
    QVERIFY2(!clearButton->isDefault() && !clearButton->autoDefault(),
             "a destructive button must never be the dialog's default (Enter) button");

    // --- Live-follow: selecting a tagged jpeg (no re-invoking the action)
    // updates the path and shows the EXIF tab. ---
    grid->select(taggedJpg);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(taggedPath).absoluteFilePath());
    QTRY_VERIFY2(dialog->tabs()->isTabVisible(exifIndex), "EXIF tab should be shown for a tagged jpeg.");
    QTRY_COMPARE(rowValueByName(dialog, "Make"), QStringLiteral("TestCam"));
    QTRY_VERIFY2(clearButton->isEnabled(), "Clear metadata should be enabled for a writable jpeg.");

    // Switch to the EXIF tab so the next case can prove it snaps back.
    dialog->tabs()->setCurrentIndex(exifIndex);
    QCOMPARE(dialog->tabs()->currentIndex(), exifIndex);

    // A writable jpeg's four tier-1 tags are input fields, not text - the tab
    // is where this file gets written, so its action button rides with it.
    QVERIFY2(clearButton->isVisibleTo(dialog),
             "Clear metadata should appear with the EXIF tab.");
    EntryInfoItem *makeRow = dialog->editableRow(QStringLiteral("Exif.Image.Make"));
    QVERIFY2(makeRow != nullptr && makeRow->valueEditor() != nullptr,
             "Make should be editable in place on a writable jpeg.");
    QCOMPARE(makeRow->valueEditor()->text(), QStringLiteral("TestCam"));
    QVERIFY2(!makeRow->valueEditor()->isReadOnly(), "the field should accept typing directly");
    // ...and listed once: the read-only dump must not repeat a tag that already
    // has a field.
    int makeRows = 0;
    for(EntryInfoItem *item : dialog->findChildren<EntryInfoItem *>()) {
        const QList<QLabel *> labels = item->findChildren<QLabel *>();
        if(!labels.isEmpty() && labels.at(0)->text() == QLatin1String("Make"))
            ++makeRows;
    }
    QCOMPARE(makeRows, 1);

    // --- Live-follow: a png has nowhere for Exiv2 to write Exif, so the tab
    // closes for it just as it does for a folder - the gate is the format's
    // capability to store Exif, not merely the file being an image. ---
    grid->select(untaggedPng);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(untaggedPath).absoluteFilePath());
    QTRY_VERIFY2(!dialog->tabs()->isTabVisible(exifIndex),
                 "EXIF tab should be hidden for a png, which Exiv2 cannot write Exif into.");
    QCOMPARE(dialog->tabs()->currentIndex(), generalIndex);
    QVERIFY2(!clearButton->isVisibleTo(dialog),
             "with the EXIF tab hidden, its button stays off General too");
    QVERIFY2(dialog->editableRow(QStringLiteral("Exif.Image.Make")) == nullptr,
             "a png should offer no editable metadata fields");

    // --- ...but a folder has no EXIF to speak of at all: the tab closes and
    // takes the current tab back to General with it. ---
    grid->select(subDir);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(subPath).absoluteFilePath());
    QTRY_VERIFY2(!dialog->tabs()->isTabVisible(exifIndex), "EXIF tab should be hidden for a folder.");
    QTRY_COMPARE(dialog->tabs()->currentIndex(), generalIndex);
    QVERIFY2(!clearButton->isVisibleTo(dialog),
             "back on General, the button row should be gone again");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoFolderAndExifStatesTest)

#include "test_file_info_folder_and_exif_states.moc"
