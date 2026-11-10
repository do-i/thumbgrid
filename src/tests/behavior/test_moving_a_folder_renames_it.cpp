#include "support/thumbgrid_test_support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QApplication>
#include <QCheckBox>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>
#include <QTemporaryDir>
#include <memory>
#include <sys/stat.h>
#include "components/directorymodel.h"
#include "components/fileoperationscontroller.h"
#include "gui/overlays/moveprogressoverlay.h"

// Reported freeze: moving ~100 folders (8k files, 12 GB) to a sibling folder
// hung the grid for minutes. A move never renamed anything: each folder was
// recreated and every file was QFile::copy()'d then deleted, on the GUI thread,
// even within one filesystem. A same-filesystem move must be a rename - the
// moved entries keep their inodes, which no copy can fake - while a destination
// that already exists still merges, and a move across filesystems still works.

namespace {

bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(data) == data.size();
}

struct stat statOf(const QString &path) {
    struct stat st {};
    ::lstat(QFile::encodeName(path).constData(), &st);
    return st;
}

ino_t inodeOf(const QString &path) { return statOf(path).st_ino; }

// <root>/album/sub/deep.txt, <root>/album/top.txt, <root>/dest/
bool buildAlbum(const QDir &root) {
    return root.mkpath("album/sub") && root.mkpath("dest") &&
           writeFile(root.filePath("album/top.txt"), "top") &&
           writeFile(root.filePath("album/sub/deep.txt"), "deep");
}

} // namespace

class MovingAFolderRenamesItTest : public QObject {
    Q_OBJECT
private slots:
    void aFolderMovedWithinOneFilesystemIsRenamedNotCopied();
    void aFileMovedWithinOneFilesystemIsRenamedNotCopied();
    void theMovedFolderLeavesTheListing();
    void aFolderMovedOntoAnExistingFolderStillMerges();
    void aFolderMovedToAnotherFilesystemStillArrives();
    void aMixedSelectionRenamesLocalItemsAtOnce();
    void aSecondMoveWaitsForTheRunningOne();
    void aCancelledMoveLosesNothing();
    void destroyingTheControllerMidMoveLosesNothing();
    void theProgressOverlayCancelButtonStopsTheMove();
    void theProgressOverlayHidesWhenTheMoveEnds();
    void noToAllNeverReplacesAFileWithAFolder();
    void aMoveStartedWhileTheOverlayFadesStillShowsIt();
};

void MovingAFolderRenamesItTest::aFolderMovedWithinOneFilesystemIsRenamedNotCopied() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QVERIFY(buildAlbum(root));
    const ino_t dirInode = inodeOf(root.filePath("album"));
    const ino_t deepInode = inodeOf(root.filePath("album/sub/deep.txt"));
    const auto subMtime = statOf(root.filePath("album/sub")).st_mtim.tv_sec;

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QVERIFY(ops.interactiveMove({root.filePath("album")}, root.filePath("dest")));

    QVERIFY2(!QFileInfo::exists(root.filePath("album")), "the source folder must be gone");
    QVERIFY2(QFileInfo(root.filePath("dest/album/sub/deep.txt")).isFile(), "nested content must arrive");
    QCOMPARE(inodeOf(root.filePath("dest/album")), dirInode);
    QCOMPARE(inodeOf(root.filePath("dest/album/sub/deep.txt")), deepInode);
    QVERIFY2(statOf(root.filePath("dest/album/sub")).st_mtim.tv_sec == subMtime,
             "a rename keeps nested folder timestamps that a recreate would reset");
}

void MovingAFolderRenamesItTest::aFileMovedWithinOneFilesystemIsRenamedNotCopied() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QVERIFY(buildAlbum(root));
    const ino_t inode = inodeOf(root.filePath("album/top.txt"));

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QVERIFY(ops.interactiveMove({root.filePath("album/top.txt")}, root.filePath("dest")));

    QVERIFY(!QFileInfo::exists(root.filePath("album/top.txt")));
    QCOMPARE(inodeOf(root.filePath("dest/top.txt")), inode);
}

// The rename skips the per-entry removeDir() that used to drop the grid item.
void MovingAFolderRenamesItTest::theMovedFolderLeavesTheListing() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QVERIFY(buildAlbum(root));

    auto model = std::make_shared<DirectoryModel>();
    QVERIFY(model->setDirectory(tmp.path()));
    QVERIFY(model->containsDir(root.filePath("album")));

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(model);
    QVERIFY(ops.interactiveMove({root.filePath("album")}, root.filePath("dest")));

    QVERIFY2(!model->containsDir(root.filePath("album")), "the moved folder must leave the current listing");
    QVERIFY(model->containsDir(root.filePath("dest")));
}

void MovingAFolderRenamesItTest::aFolderMovedOntoAnExistingFolderStillMerges() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QVERIFY(buildAlbum(root));
    QVERIFY(root.mkpath("dest/album"));
    QVERIFY(writeFile(root.filePath("dest/album/already-here.txt"), "kept"));

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QVERIFY(ops.interactiveMove({root.filePath("album")}, root.filePath("dest")));

    QVERIFY2(QFileInfo(root.filePath("dest/album/already-here.txt")).isFile(),
             "a rename must never replace an existing destination folder");
    QVERIFY(QFileInfo(root.filePath("dest/album/top.txt")).isFile());
    QVERIFY(QFileInfo(root.filePath("dest/album/sub/deep.txt")).isFile());
    QVERIFY(!QFileInfo::exists(root.filePath("album")));
}

// Across filesystems there is no rename: the bytes must be copied, so that runs on a worker and the
// call returns at once instead of freezing the GUI.
struct CrossFs {
    QTemporaryDir src;
    QTemporaryDir dst{QStringLiteral("/dev/shm/thumbgrid-move-XXXXXX")};
    bool usable() const {
        return src.isValid() && dst.isValid() && statOf(src.path()).st_dev != statOf(dst.path()).st_dev;
    }
};

// <root>/<name>/f<i>.txt and <root>/<name>/nested/g<i>.txt
bool buildBigFolder(const QDir &root, const QString &name, int files) {
    if(!root.mkpath(name + "/nested"))
        return false;
    for(int i = 0; i < files; i++) {
        if(!writeFile(root.filePath(QStringLiteral("%1/f%2.txt").arg(name).arg(i)), QByteArray(4096, 'x')) ||
           !writeFile(root.filePath(QStringLiteral("%1/nested/g%2.txt").arg(name).arg(i)), "g"))
            return false;
    }
    return true;
}

// spin (no event loop needed: the worker runs on its own) until the move has really begun
bool waitUntilExists(const QString &path) {
    QElapsedTimer t;
    t.start();
    while(!QFileInfo::exists(path)) {
        if(t.elapsed() > 10000)
            return false;
        QThread::usleep(200);
    }
    return true;
}

// every file of buildBigFolder() is in exactly one of the two folders - nothing lost, nothing doubled
void verifyEachFileInExactlyOnePlace(const QString &srcFolder, const QString &dstFolder, int files) {
    for(int i = 0; i < files; i++) {
        for(const QString &rel : {QStringLiteral("f%1.txt").arg(i), QStringLiteral("nested/g%1.txt").arg(i)}) {
            const bool inSrc = QFileInfo::exists(srcFolder + "/" + rel);
            const bool inDst = QFileInfo::exists(dstFolder + "/" + rel);
            QVERIFY2(inSrc != inDst, qPrintable(rel + (inSrc ? " is in both places" : " is lost")));
        }
    }
}

void MovingAFolderRenamesItTest::aFolderMovedToAnotherFilesystemStillArrives() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs two filesystems (temp dir and /dev/shm)");
    QDir root(fs.src.path());
    QVERIFY(buildAlbum(root));

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QSignalSpy finished(&ops, &FileOperationsController::moveFinished);
    QVERIFY(ops.interactiveMove({root.filePath("album")}, fs.dst.path()));
    QVERIFY2(ops.isMoveRunning(), "a cross-filesystem move must continue on the worker");

    QVERIFY(finished.wait(10000));
    QVERIFY(!ops.isMoveRunning());
    QCOMPARE(finished.first().at(0).toInt(), 1);     // moved
    QCOMPARE(finished.first().at(1).toInt(), 0);     // failures
    QCOMPARE(finished.first().at(2).toBool(), false); // cancelled
    QVERIFY2(QFileInfo(fs.dst.path() + "/album/sub/deep.txt").isFile(), "nested content must arrive");
    QVERIFY(QFileInfo(fs.dst.path() + "/album/top.txt").isFile());
    QVERIFY2(!QFileInfo::exists(root.filePath("album")), "and the source must be removed");
}

void MovingAFolderRenamesItTest::aMixedSelectionRenamesLocalItemsAtOnce() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs two filesystems (temp dir and /dev/shm)");
    // "remote" sits on dest's filesystem, "album" does not
    QDir dst(fs.dst.path());
    QVERIFY(dst.mkpath("remote/inner") && dst.mkpath("target"));
    const ino_t remoteInode = inodeOf(dst.filePath("remote"));
    QDir root(fs.src.path());
    QVERIFY(buildAlbum(root));

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QSignalSpy finished(&ops, &FileOperationsController::moveFinished);
    QVERIFY(ops.interactiveMove({dst.filePath("remote"), root.filePath("album")}, dst.filePath("target")));

    QCOMPARE(inodeOf(dst.filePath("target/remote")), remoteInode); // renamed before the call returned
    QVERIFY(finished.wait(10000));
    QVERIFY(QFileInfo(dst.filePath("target/album/sub/deep.txt")).isFile());
    QVERIFY(!QFileInfo::exists(root.filePath("album")));
}

void MovingAFolderRenamesItTest::aSecondMoveWaitsForTheRunningOne() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs two filesystems (temp dir and /dev/shm)");
    QDir root(fs.src.path());
    QVERIFY(buildBigFolder(root, "big", 200));
    QVERIFY(buildAlbum(root));

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QSignalSpy finished(&ops, &FileOperationsController::moveFinished);
    QVERIFY(ops.interactiveMove({root.filePath("big")}, fs.dst.path()));
    QVERIFY2(!ops.interactiveMove({root.filePath("album")}, fs.dst.path()),
             "only one background move at a time");
    QVERIFY2(QFileInfo(root.filePath("album/top.txt")).isFile(), "the refused move must not touch anything");
    QVERIFY(finished.wait(10000));
}

void MovingAFolderRenamesItTest::aCancelledMoveLosesNothing() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs two filesystems (temp dir and /dev/shm)");
    QDir root(fs.src.path());
    QVERIFY(buildBigFolder(root, "big", 3000));

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QSignalSpy finished(&ops, &FileOperationsController::moveFinished);
    QVERIFY(ops.interactiveMove({root.filePath("big")}, fs.dst.path()));
    QVERIFY(waitUntilExists(fs.dst.path() + "/big/f0.txt")); // files are moving
    ops.cancelMove();
    QVERIFY(finished.wait(10000));

    QVERIFY2(finished.first().at(2).toBool(), "the move must report it was cancelled");
    QCOMPARE(finished.first().at(0).toInt(), 0); // the folder did not leave its source
    QVERIFY2(QFileInfo(root.filePath("big")).isDir(), "a cancelled move leaves the rest in the source");
    QVERIFY2(QFileInfo(root.filePath("big/nested/g0.txt")).isFile(), "and stops before it is done");
    verifyEachFileInExactlyOnePlace(root.filePath("big"), fs.dst.path() + "/big", 3000);
}

// Quitting mid-move: the controller's destructor stops the worker after its current file and waits.
void MovingAFolderRenamesItTest::destroyingTheControllerMidMoveLosesNothing() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs two filesystems (temp dir and /dev/shm)");
    QDir root(fs.src.path());
    QVERIFY(buildBigFolder(root, "big", 3000));

    MW *window = new MW();
    {
        FileOperationsController ops(window);
        ops.setModel(std::make_shared<DirectoryModel>());
        QVERIFY(ops.interactiveMove({root.filePath("big")}, fs.dst.path()));
        QVERIFY(waitUntilExists(fs.dst.path() + "/big/f0.txt")); // files are moving
    }
    QVERIFY2(QFileInfo(root.filePath("big/nested/g0.txt")).isFile(), "the worker must stop, not finish");
    verifyEachFileInExactlyOnePlace(root.filePath("big"), fs.dst.path() + "/big", 3000);
}

// The Cancel button is the user's only way to stop a move (Esc quits the app).
void MovingAFolderRenamesItTest::theProgressOverlayCancelButtonStopsTheMove() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs two filesystems (temp dir and /dev/shm)");
    QDir root(fs.src.path());
    QVERIFY(buildBigFolder(root, "big", 3000));

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QSignalSpy finished(&ops, &FileOperationsController::moveFinished);
    QVERIFY(ops.interactiveMove({root.filePath("big")}, fs.dst.path()));

    auto *overlay = window->findChild<MoveProgressOverlay *>();
    QVERIFY2(overlay, "a background move must show the progress overlay");
    QVERIFY2(!overlay->isHidden(), "and the overlay must be visible while it runs");
    auto *cancel = overlay->findChild<QPushButton *>();
    QVERIFY(cancel && cancel->isEnabled());

    QVERIFY(waitUntilExists(fs.dst.path() + "/big/f0.txt")); // files are moving
    cancel->click();
    QVERIFY2(!cancel->isEnabled(), "Cancel greys out while the current file finishes");
    QVERIFY(finished.wait(10000));
    QVERIFY2(finished.first().at(2).toBool(), "clicking Cancel must cancel the move");
    QVERIFY(QFileInfo(root.filePath("big/nested/g0.txt")).isFile());
    verifyEachFileInExactlyOnePlace(root.filePath("big"), fs.dst.path() + "/big", 3000);
}

void MovingAFolderRenamesItTest::theProgressOverlayHidesWhenTheMoveEnds() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs two filesystems (temp dir and /dev/shm)");
    QDir root(fs.src.path());
    QVERIFY(buildAlbum(root));

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QSignalSpy finished(&ops, &FileOperationsController::moveFinished);
    QVERIFY(ops.interactiveMove({root.filePath("album")}, fs.dst.path()));
    QVERIFY(finished.wait(10000));
    auto *overlay = window->findChild<MoveProgressOverlay *>();
    QVERIFY(overlay);
    QTRY_VERIFY2(overlay->isHidden(), "the overlay must go away once the move is done");

    // a second move re-arms the button
    QVERIFY(buildAlbum(root));
    QVERIFY(QDir(fs.dst.path()).mkpath("second")); // fresh: a name clash would open the replace dialog
    QVERIFY(ops.interactiveMove({root.filePath("album")}, fs.dst.path() + "/second"));
    QVERIFY(!overlay->isHidden());
    QVERIFY(overlay->findChild<QPushButton *>()->isEnabled());
    QVERIFY(finished.wait(10000));
}

// Answers every replace dialog that opens with "No" and "apply to all" ticked.
void answerReplaceDialogsWithNoToAll(QObject *context) {
    auto *poll = new QTimer(context);
    QObject::connect(poll, &QTimer::timeout, poll, []() {
        QWidget *modal = QApplication::activeModalWidget();
        if(!modal)
            return;
        auto *all = modal->findChild<QCheckBox *>(QStringLiteral("applyAllCheckBox"));
        auto *no = modal->findChild<QPushButton *>(QStringLiteral("noButton"));
        if(!all || !no)
            return;
        all->setChecked(true);
        no->click();
    });
    poll->start(20);
}

// "No to all" from an earlier clash must also cover a folder whose name is taken by a file: the
// file at the destination has to survive, not be deleted to make room.
void MovingAFolderRenamesItTest::noToAllNeverReplacesAFileWithAFolder() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QVERIFY(root.mkpath("src/folder") && root.mkpath("dest"));
    QVERIFY(writeFile(root.filePath("src/a.txt"), "new a"));
    QVERIFY(writeFile(root.filePath("src/folder/inner.txt"), "inner"));
    QVERIFY(writeFile(root.filePath("dest/a.txt"), "old a"));          // file clash -> answered No to all
    QVERIFY(writeFile(root.filePath("dest/folder"), "precious file")); // folder-onto-file clash

    MW *window = new MW();
    FileOperationsController ops(window);
    ops.setModel(std::make_shared<DirectoryModel>());
    QObject answerer;
    answerReplaceDialogsWithNoToAll(&answerer);
    QVERIFY(ops.interactiveMove({root.filePath("src/a.txt"), root.filePath("src/folder")}, root.filePath("dest")));

    QVERIFY2(QFileInfo(root.filePath("dest/folder")).isFile(), "No to all must keep the destination file");
    QFile kept(root.filePath("dest/folder"));
    QVERIFY(kept.open(QIODevice::ReadOnly));
    QCOMPARE(kept.readAll(), QByteArray("precious file"));
    QVERIFY2(QFileInfo(root.filePath("src/folder/inner.txt")).isFile(), "the skipped folder stays in the source");
}

// The overlay fades out briefly after a move; a move started in that window must still show it,
// with a working Cancel even if the previous move was cancelled. Driven through MW directly: a real
// second move can finish inside the wait and hide the overlay legitimately.
void MovingAFolderRenamesItTest::aMoveStartedWhileTheOverlayFadesStillShowsIt() {
    MW *window = new MW();
    window->beginMoveProgress();
    auto *overlay = window->findChild<MoveProgressOverlay *>();
    QVERIFY(overlay);
    auto *cancel = overlay->findChild<QPushButton *>();
    cancel->click();                 // first move cancelled: button greys out
    QVERIFY(!cancel->isEnabled());
    window->hideMoveProgress();      // first move ends: fade-out starts
    QVERIFY2(!overlay->isHidden(), "precondition: still fading");

    window->beginMoveProgress();     // second move starts mid-fade
    window->showMoveProgress(10, 100);
    QTest::qWait(400);               // past the fade
    QVERIFY2(!overlay->isHidden(), "the second move must keep its progress overlay");
    QVERIFY2(cancel->isEnabled(), "and its Cancel must work");
}

TG_BEHAVIOR_TEST_MAIN(MovingAFolderRenamesItTest)

#include "test_moving_a_folder_renames_it.moc"
