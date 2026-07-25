#include "support/thumbgrid_test_support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include "components/directorymodel.h"
#include "utils/fileoperations.h"

// Regression guard for a data-loss bug: deleting a *symlink to a directory*
// used to be classified as "directory" (QFileInfo::isDir() resolves links) and
// handed to QDir::removeRecursively(), which walked through the link and erased
// the contents of its TARGET - data living outside the browsed folder - while
// leaving the link itself behind.
//
// FileOperations::removeDir() guards against that for its own (non-trash)
// callers. DirectoryModel::removeDir() is the other guard: it branches to
// FileOperations::moveToTrash() *before* ever reaching FileOperations::
// removeDir(), so it carries its own isSymLink() check up front, covering
// both the trash=true and trash=false routes. Most of these tests exercise
// FileOperations::removeDir() directly; the trash-route test below exercises
// DirectoryModel::removeDir(), where the trash branch actually lives.
//
// Deliberately no assertions about whether a file actually landed in the
// desktop trash: QFile::moveToTrash refuses files under /tmp and its outcome
// depends on the desktop environment. The trash-route test below asserts the
// *gating* - that a symlink is routed to a plain unlink instead of
// moveToTrash() - not that trashing itself succeeded.

namespace {

bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(data) == data.size();
}

// Builds <root>/precious/treasure.txt and returns the precious dir path.
QString makePreciousTarget(const QString &root) {
    QDir dir(root);
    if(!dir.mkpath("precious"))
        return QString();
    if(!writeFile(dir.filePath("precious/treasure.txt"), "irreplaceable"))
        return QString();
    return dir.filePath("precious");
}

} // namespace

class DeleteDoesNotFollowSymlinksTest : public QObject {
    Q_OBJECT
private slots:
    void directorySymlinkWithAbsoluteTargetIsUnlinkedNotFollowed();
    void directorySymlinkWithRelativeTargetIsUnlinkedNotFollowed();
    void danglingSymlinkCanBeRemoved();
    void ordinaryDirectoryIsStillRemovedRecursively();
    void deletingADirectoryThatContainsASymlinkSparesTheTarget();
    void removingASymlinkToAFileSparesTheFile();
    void directoryModelTrashRouteUnlinksSymlinkInsteadOfMovingToTrash();
};

void DeleteDoesNotFollowSymlinksTest::directorySymlinkWithAbsoluteTargetIsUnlinkedNotFollowed() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString precious = makePreciousTarget(tmp.path());
    QVERIFY(!precious.isEmpty());
    QString treasure = precious + "/treasure.txt";

    QDir root(tmp.path());
    QVERIFY(root.mkpath("browsed"));
    QString link = root.filePath("browsed/shortcut");
    if(!QFile::link(precious, link) || !QFileInfo(link).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");

    // The trap this test exists for: the link reads as a directory.
    QVERIFY2(QFileInfo(link).isDir(), "a link to a directory resolves as a directory");

    FileOpResult result = FileOpResult::OTHER_ERROR;
    FileOperations::removeDir(link, true, result);

    // Data survival first: that is the bug, and it must be the assertion that
    // speaks up if the guard ever goes away.
    QVERIFY2(QFileInfo(treasure).isFile(), "the file inside the target must survive");
    QVERIFY2(QFileInfo(precious).isDir(), "the target directory must survive");
    QVERIFY2(!QFileInfo(link).isSymLink(), "the link itself must be gone");
    QVERIFY2(!QFileInfo(link).exists(), "nothing may be left at the link path");
    QCOMPARE(result, FileOpResult::SUCCESS);
}

void DeleteDoesNotFollowSymlinksTest::directorySymlinkWithRelativeTargetIsUnlinkedNotFollowed() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString precious = makePreciousTarget(tmp.path());
    QVERIFY(!precious.isEmpty());
    QString treasure = precious + "/treasure.txt";

    QDir root(tmp.path());
    QVERIFY(root.mkpath("browsed"));
    QString link = root.filePath("browsed/shortcut");
    // Relative target: the link body is "../precious", resolved against the
    // link's own directory. A follow-the-target bug hits this the same way.
    if(!QFile::link("../precious", link) || !QFileInfo(link).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");
    QVERIFY2(QFileInfo(link).isDir(), "the relative link must resolve to the target directory");

    FileOpResult result = FileOpResult::OTHER_ERROR;
    FileOperations::removeDir(link, true, result);

    QVERIFY2(QFileInfo(treasure).isFile(), "the file inside the target must survive");
    QVERIFY2(QFileInfo(precious).isDir(), "the target directory must survive");
    QVERIFY2(!QFileInfo(link).isSymLink(), "the link itself must be gone");
    QVERIFY2(!QFileInfo(link).exists(), "nothing may be left at the link path");
    QCOMPARE(result, FileOpResult::SUCCESS);
}

void DeleteDoesNotFollowSymlinksTest::danglingSymlinkCanBeRemoved() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QString link = root.filePath("broken");
    if(!QFile::link(root.filePath("nowhere"), link) || !QFileInfo(link).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");
    // exists() follows the link, so a broken link reads as absent; removing it
    // must still be allowed.
    QVERIFY(!QFileInfo(link).exists());

    FileOpResult check = FileOpResult::OTHER_ERROR;
    FileOperations::checkCanRemove(link, check);
    QVERIFY2(check != FileOpResult::SOURCE_DOES_NOT_EXIST,
             "a dangling link is present on disk and must not read as missing");

    FileOpResult result = FileOpResult::OTHER_ERROR;
    FileOperations::removeSymLink(link, result);

    QVERIFY2(result != FileOpResult::SOURCE_DOES_NOT_EXIST,
             "removing a dangling link must not fail as 'source does not exist'");
    QCOMPARE(result, FileOpResult::SUCCESS);
    QVERIFY2(!QFileInfo(link).isSymLink(), "the broken link must be gone");
}

void DeleteDoesNotFollowSymlinksTest::ordinaryDirectoryIsStillRemovedRecursively() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QVERIFY(root.mkpath("doomed/nested"));
    QVERIFY(writeFile(root.filePath("doomed/top.txt"), "a"));
    QVERIFY(writeFile(root.filePath("doomed/nested/deep.txt"), "b"));
    QString doomed = root.filePath("doomed");

    FileOpResult result = FileOpResult::OTHER_ERROR;
    FileOperations::removeDir(doomed, true, result);

    QCOMPARE(result, FileOpResult::SUCCESS);
    QVERIFY2(!QFileInfo(doomed).exists(), "a real directory must still be removed");
    QVERIFY(!QFileInfo(root.filePath("doomed/nested/deep.txt")).exists());
}

void DeleteDoesNotFollowSymlinksTest::deletingADirectoryThatContainsASymlinkSparesTheTarget() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString precious = makePreciousTarget(tmp.path());
    QVERIFY(!precious.isEmpty());
    QString treasure = precious + "/treasure.txt";

    QDir root(tmp.path());
    QVERIFY(root.mkpath("doomed"));
    QVERIFY(writeFile(root.filePath("doomed/own.txt"), "expendable"));
    QString link = root.filePath("doomed/shortcut");
    if(!QFile::link(precious, link) || !QFileInfo(link).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");
    QString doomed = root.filePath("doomed");

    FileOpResult result = FileOpResult::OTHER_ERROR;
    FileOperations::removeDir(doomed, true, result);

    QCOMPARE(result, FileOpResult::SUCCESS);
    QVERIFY2(!QFileInfo(doomed).exists(), "the containing directory must be removed");
    QVERIFY2(QFileInfo(precious).isDir(), "the link's target directory must survive");
    QVERIFY2(QFileInfo(treasure).isFile(), "the file inside the target must survive");
}

void DeleteDoesNotFollowSymlinksTest::removingASymlinkToAFileSparesTheFile() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QString target = root.filePath("original.txt");
    QVERIFY(writeFile(target, "keep me"));
    QString link = root.filePath("alias.txt");
    if(!QFile::link(target, link) || !QFileInfo(link).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");

    FileOpResult result = FileOpResult::OTHER_ERROR;
    FileOperations::removeSymLink(link, result);

    QCOMPARE(result, FileOpResult::SUCCESS);
    QVERIFY2(!QFileInfo(link).isSymLink(), "the link must be gone");
    QVERIFY2(QFileInfo(target).isFile(), "the target file must survive");
    QFile targetFile(target);
    QVERIFY(targetFile.open(QIODevice::ReadOnly));
    QCOMPARE(targetFile.readAll(), QByteArray("keep me"));
}

void DeleteDoesNotFollowSymlinksTest::directoryModelTrashRouteUnlinksSymlinkInsteadOfMovingToTrash() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QString precious = makePreciousTarget(tmp.path());
    QVERIFY(!precious.isEmpty());
    QString treasure = precious + "/treasure.txt";

    QDir root(tmp.path());
    QVERIFY(root.mkpath("browsed"));
    QString link = root.filePath("browsed/shortcut");
    if(!QFile::link(precious, link) || !QFileInfo(link).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");
    QVERIFY2(QFileInfo(link).isDir(), "a link to a directory resolves as a directory");

    // DirectoryModel::removeDir(dirPath, trash, ...) used to branch straight to
    // FileOperations::moveToTrash() when trash=true, reaching
    // FileOperations::removeDir()'s own symlink guard only on the trash=false
    // branch (S3). The fix hoists an isSymLink() check above that branch so a
    // symlink is always sent to FileOperations::removeSymLink() - a plain
    // unlink - regardless of trash.
    //
    // QFile::moveToTrash() refuses paths under /tmp (where QTemporaryDir
    // lives), so its outcome cannot be used to assert anything here - per
    // memory, test the gating, never the trash outcome. This exploits that
    // refusal as a probe instead: removeSymLink() is a plain unlink and does
    // not care whether the path is under /tmp, so it deterministically
    // succeeds - while a moveToTrash() call on the same /tmp path would not.
    // A SUCCESS result below is therefore evidence the symlink route (not
    // moveToTrash()) was actually taken, without depending on desktop trash
    // infrastructure at all.
    DirectoryModel model;
    FileOpResult result = FileOpResult::OTHER_ERROR;
    model.removeDir(link, /*trash=*/true, /*recursive=*/true, result);

    QVERIFY2(QFileInfo(treasure).isFile(), "the file inside the target must survive");
    QVERIFY2(QFileInfo(precious).isDir(), "the target directory must survive");
    QVERIFY2(!QFileInfo(link).isSymLink(), "the link itself must be gone");
    QVERIFY2(!QFileInfo(link).exists(), "nothing may be left at the link path");
    QCOMPARE(result, FileOpResult::SUCCESS);
}

TG_BEHAVIOR_TEST_MAIN(DeleteDoesNotFollowSymlinksTest)

#include "test_delete_does_not_follow_symlinks.moc"
