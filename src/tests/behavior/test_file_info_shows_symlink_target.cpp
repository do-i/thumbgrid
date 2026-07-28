#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLocale>
#include <QTemporaryDir>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// A symlink is invisible in the File info window unless it says so: Path shows
// the link, while Size, Permissions and the timestamps are all read *through*
// the link and describe the target. Without a row naming the target, the window
// reads as a plain file that happens to live at that path. The dangling case is
// worse - the link resolves to nothing, so the window used to fall through to
// its "No selection" placeholder and say nothing at all about an entry the user
// had selected.
class FileInfoSymlinkTargetTest : public QObject {
    Q_OBJECT

private slots:
    void symlinkRowNamesTheTargetAndFlagsBrokenLinks();
};

namespace {

// Every General/EXIF row is an EntryInfoItem with two QLabel children (name,
// then value) added in that order by EntryInfoItem's constructor. The value
// label elides long text into its tooltip (EntryInfoItem::updateElidedText), so
// the tooltip is the full string when it is set.
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

bool hasRow(QWidget *root, const QString &rowName) {
    for(EntryInfoItem *item : root->findChildren<EntryInfoItem *>()) {
        const QList<QLabel *> labels = item->findChildren<QLabel *>();
        if(labels.size() >= 2 && labels.at(0)->text() == rowName)
            return true;
    }
    return false;
}

} // namespace

void FileInfoSymlinkTargetTest::symlinkRowNamesTheTargetAndFlagsBrokenLinks() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    QVERIFY2(root.mkpath("store"), "Store folder should be created.");

    const QString galleryPath = root.filePath("gallery");
    // The target lives outside the browsed folder so the grid holds exactly the
    // link and one plain file, and so the target path is visibly different from
    // the link path.
    const QString targetPath = QFileInfo(root.filePath("store/target.png")).absoluteFilePath();
    const QString linkPath = root.filePath("gallery/link.png");
    const QString plainPath = root.filePath("gallery/plain.png");

    QVERIFY2(tgtest::writeImage(targetPath, Qt::red), "Target image should be written.");
    QVERIFY2(tgtest::writeImage(plainPath, Qt::blue), "Plain image should be written.");
    if(!QFile::link(targetPath, linkPath) || !QFileInfo(linkPath).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), then files sorted: link.png (1), plain.png (2).
    QTRY_COMPARE(grid->itemCount(), 3);
    const int linkEntry = 1;
    const int plainEntry = 2;

    grid->select(linkEntry);
    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable.");

    auto *dialog = window->findChild<FileInfoDialog *>();
    QTRY_VERIFY2(dialog != nullptr, "The File info popup should exist.");
    QTRY_VERIFY2(dialog->isVisible(), "The File info popup should be visible.");

    // --- A working link: Path stays on the link, a Symlink to row names the
    // target, and the rows read through the link still describe the target. ---
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(linkPath).absoluteFilePath());
    QTRY_COMPARE(rowValueByName(dialog, "Symlink to"), targetPath);
    QCOMPARE(rowValueByName(dialog, "Size"),
             QLocale().formattedDataSize(QFileInfo(targetPath).size()));

    // --- A plain file: no Symlink to row at all, so its presence is a real
    // signal rather than a permanent fixture of the window. ---
    grid->select(plainEntry);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(plainPath).absoluteFilePath());
    QVERIFY2(!hasRow(dialog, "Symlink to"), "A plain file should have no Symlink to row.");

    // --- A dangling link: still described (not dropped to "No selection"), the
    // target named, and flagged broken. Targeted directly rather than through
    // the grid because whether a dangling link is listed at all is the
    // directory model's business, not this window's. ---
    const QString brokenLinkPath = root.filePath("gallery/broken.png");
    const QString goneTargetPath = QFileInfo(root.filePath("store/gone.png")).absoluteFilePath();
    QVERIFY2(tgtest::writeImage(goneTargetPath, Qt::green), "Soon-to-be-gone image should be written.");
    if(!QFile::link(goneTargetPath, brokenLinkPath) || !QFileInfo(brokenLinkPath).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");
    QVERIFY2(QFile::remove(goneTargetPath), "The link target should be removable.");
    QVERIFY2(!QFileInfo(brokenLinkPath).exists(), "The link should now be dangling.");

    dialog->setTarget(brokenLinkPath);
    QCOMPARE(rowValueByName(dialog, "Path"), QFileInfo(brokenLinkPath).absoluteFilePath());
    QCOMPARE(rowValueByName(dialog, "Symlink to"),
             QStringLiteral("%1 (broken link)").arg(goneTargetPath));
    // Nothing to measure at the other end: a 0-byte Size row would read as an
    // empty file rather than a missing one.
    QVERIFY2(!hasRow(dialog, "Size"), "A broken link should not show a size read through it.");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoSymlinkTargetTest)

#include "test_file_info_shows_symlink_target.moc"
