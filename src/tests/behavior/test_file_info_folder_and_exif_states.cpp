#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
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
// (EntryInfoItem::updateElidedText), so prefer the tooltip when set.
QString rowValueByName(QWidget *root, const QString &rowName) {
    for(EntryInfoItem *item : root->findChildren<EntryInfoItem *>()) {
        const QList<QLabel *> labels = item->findChildren<QLabel *>();
        if(labels.size() >= 2 && labels.at(0)->text() == rowName) {
            const QString tip = labels.at(1)->toolTip();
            return tip.isEmpty() ? labels.at(1)->text() : tip;
        }
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

    // --- Folder target: counts shown, EXIF tab disabled. ---
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
    QVERIFY2(!dialog->tabs()->isTabEnabled(exifIndex), "EXIF tab should be disabled for a folder target.");

    // Strip metadata lives here rather than in the context menu. It must be
    // present but refuse a folder - disabled, not hidden, so its absence can
    // never be misread as "this file carries no metadata".
    QPushButton *stripButton = dialog->stripMetadataButton();
    QVERIFY2(stripButton != nullptr, "The File info window should offer a strip metadata button.");
    QVERIFY2(stripButton->isVisibleTo(dialog), "The strip button should be visible in the dialog.");
    QVERIFY2(!stripButton->isEnabled(), "Strip metadata should be disabled for a folder target.");
    QVERIFY2(!stripButton->isDefault() && !stripButton->autoDefault(),
             "a destructive button must never be the dialog's default (Enter) button");

    // Edit is gated more narrowly than Strip: it needs a format exiv2 can
    // rewrite, so a folder disables both.
    QPushButton *editButton = dialog->editMetadataButton();
    QVERIFY2(editButton != nullptr, "The File info window should offer an edit metadata button.");
    QVERIFY2(!editButton->isEnabled(), "Edit metadata should be disabled for a folder target.");

    // --- Live-follow: selecting a tagged jpeg (no re-invoking the action)
    // updates the path and enables the EXIF tab. ---
    grid->select(taggedJpg);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(taggedPath).absoluteFilePath());
    QTRY_VERIFY2(dialog->tabs()->isTabEnabled(exifIndex), "EXIF tab should be enabled for a tagged jpeg.");
    QTRY_COMPARE(rowValueByName(dialog, "Make"), QStringLiteral("TestCam"));
    QTRY_VERIFY2(stripButton->isEnabled(), "Strip metadata should be enabled for a writable jpeg.");
    QTRY_VERIFY2(editButton->isEnabled(), "Edit metadata should be enabled for a jpeg.");

    // Switch to the EXIF tab so the next case can prove it snaps back.
    dialog->tabs()->setCurrentIndex(exifIndex);
    QCOMPARE(dialog->tabs()->currentIndex(), exifIndex);

    // --- Live-follow: selecting a tagless png disables the EXIF tab again
    // and snaps the current tab back to General. ---
    grid->select(untaggedPng);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(untaggedPath).absoluteFilePath());
    QTRY_VERIFY2(!dialog->tabs()->isTabEnabled(exifIndex), "EXIF tab should be disabled for a tagless png.");
    QTRY_COMPARE(dialog->tabs()->currentIndex(), generalIndex);
    // Still enabled: a png with no *Exif* tags can carry XMP or text chunks, so
    // the button gates on the file being a strippable image, not on the EXIF tab.
    QTRY_VERIFY2(stripButton->isEnabled(), "Strip metadata should stay enabled for a tagless png.");
    // ...but a png is not rewritable by tier 1, so editing stays off even
    // though stripping is available. The two buttons gate on different rules.
    QTRY_VERIFY2(!editButton->isEnabled(), "Edit metadata should be disabled for a png.");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoFolderAndExifStatesTest)

#include "test_file_info_folder_and_exif_states.moc"
