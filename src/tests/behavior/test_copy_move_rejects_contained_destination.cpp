#include "support/thumbgrid_test_support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>
#include "components/fileoperationscontroller.h"

// Dropping a folder into itself or beneath it must be refused by destinationIsInsideSource(), or the copy
// recurses forever and a move can destroy the source. Both sides are canonicalized, so a destination reached
// through a symlink is caught; a source that is itself a symlink is not a container (links never recurse).
// It is a public static: no window, dialogs or controller needed.

namespace {

bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(data) == data.size();
}

// Fixture under <tmp>: source/child/deep, sibling, sourceExtra (name-prefix trap), unrelated/nested,
// linkTarget/inner (a symlink's target), links/ (the symlinks), loose.txt (a plain-file source).
struct Fixture {
    QTemporaryDir tmp;

    QString path(const QString &rel) const { return tmp.path() + QStringLiteral("/") + rel; }
    QString source() const { return path(QStringLiteral("source")); }

    bool build() {
        if(!tmp.isValid())
            return false;
        QDir dir(tmp.path());
        return dir.mkpath(QStringLiteral("source/child/deep")) &&
               dir.mkpath(QStringLiteral("sibling")) && dir.mkpath(QStringLiteral("sourceExtra")) &&
               dir.mkpath(QStringLiteral("unrelated/nested")) &&
               dir.mkpath(QStringLiteral("linkTarget/inner")) && dir.mkpath(QStringLiteral("links")) &&
               writeFile(path(QStringLiteral("loose.txt")), "just a file") &&
               writeFile(path(QStringLiteral("source/child/payload.txt")), "payload");
    }
};

} // namespace

class CopyMoveRejectsContainedDestinationTest : public QObject {
    Q_OBJECT
private slots:
    void theSourceDirectoryItselfIsAContainedDestination();
    void aDirectChildOfTheSourceIsContained();
    void aDeepDescendantOfTheSourceIsContained();
    void aSymlinkedDestinationResolvingIntoTheSourceIsContained();
    void aSiblingDirectoryIsNotContained();
    void anUnrelatedDirectoryIsNotContained();
    void aPlainFileSourceNeverContainsTheDestination();
    void aSymlinkSourceDoesNotContainItsOwnTarget();
    void oneOffendingSourceAmongManyIsEnough();
    void aNonExistentDestinationUnderASymlinkedRouteToTheSourceIsContained();
    void aNonExistentDestinationOutsideEverySourceIsNotContained();
    void theCopyExecutorRefusesTheSourcesOwnSubfolder();
    void theMoveExecutorRefusesTheSourcesOwnSubfolder();
    void theCopyExecutorStillCopiesToAnUnrelatedFolder();
};

void CopyMoveRejectsContainedDestinationTest::theSourceDirectoryItselfIsAContainedDestination() {
    Fixture fx;
    QVERIFY(fx.build());

    QVERIFY2(FileOperationsController::destinationIsInsideSource({fx.source()}, fx.source()),
             "dropping a folder into itself must be refused");
}

void CopyMoveRejectsContainedDestinationTest::aDirectChildOfTheSourceIsContained() {
    Fixture fx;
    QVERIFY(fx.build());

    QVERIFY2(FileOperationsController::destinationIsInsideSource({fx.source()},
                                                                 fx.path(QStringLiteral("source/child"))),
             "a direct child of the source must be refused");
}

void CopyMoveRejectsContainedDestinationTest::aDeepDescendantOfTheSourceIsContained() {
    Fixture fx;
    QVERIFY(fx.build());

    QVERIFY2(FileOperationsController::destinationIsInsideSource(
                 {fx.source()}, fx.path(QStringLiteral("source/child/deep"))),
             "any descendant of the source must be refused, however deep");
}

void CopyMoveRejectsContainedDestinationTest::aSymlinkedDestinationResolvingIntoTheSourceIsContained() {
    Fixture fx;
    QVERIFY(fx.build());

    // The whole reason the guard canonicalizes: this path does not look like it
    // is under the source, but it resolves there.
    QString link = fx.path(QStringLiteral("links/shortcut"));
    if(!QFile::link(fx.path(QStringLiteral("source/child")), link) || !QFileInfo(link).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");
    QVERIFY2(!link.contains(QStringLiteral("/source/")),
             "the destination path must not textually reveal the source");

    QVERIFY2(FileOperationsController::destinationIsInsideSource({fx.source()}, link),
             "a symlinked destination resolving inside the source must be refused");
}

void CopyMoveRejectsContainedDestinationTest::aSiblingDirectoryIsNotContained() {
    Fixture fx;
    QVERIFY(fx.build());

    QVERIFY2(!FileOperationsController::destinationIsInsideSource({fx.source()},
                                                                  fx.path(QStringLiteral("sibling"))),
             "copying next to the source is perfectly normal");
    // A name that merely starts with the source's name is not inside it.
    QVERIFY2(!FileOperationsController::destinationIsInsideSource({fx.source()},
                                                                  fx.path(QStringLiteral("sourceExtra"))),
             "a sibling whose name shares the source's prefix is not inside it");
}

void CopyMoveRejectsContainedDestinationTest::anUnrelatedDirectoryIsNotContained() {
    Fixture fx;
    QVERIFY(fx.build());

    QVERIFY(!FileOperationsController::destinationIsInsideSource(
        {fx.source()}, fx.path(QStringLiteral("unrelated/nested"))));
    // The source's own parent is above it, not inside it.
    QVERIFY(!FileOperationsController::destinationIsInsideSource({fx.source()}, fx.tmp.path()));
}

void CopyMoveRejectsContainedDestinationTest::aPlainFileSourceNeverContainsTheDestination() {
    Fixture fx;
    QVERIFY(fx.build());

    // A file cannot contain anything - copying it into its own folder is a
    // duplicate, handled by the normal overwrite flow, not by this guard.
    QVERIFY2(!FileOperationsController::destinationIsInsideSource(
                 {fx.path(QStringLiteral("loose.txt"))}, fx.tmp.path()),
             "a plain file source must never block its own parent as destination");
    QVERIFY(!FileOperationsController::destinationIsInsideSource(
        {fx.path(QStringLiteral("source/child/payload.txt"))}, fx.path(QStringLiteral("source/child"))));
}

void CopyMoveRejectsContainedDestinationTest::aSymlinkSourceDoesNotContainItsOwnTarget() {
    Fixture fx;
    QVERIFY(fx.build());

    QString link = fx.path(QStringLiteral("links/dirlink"));
    if(!QFile::link(fx.path(QStringLiteral("linkTarget")), link) || !QFileInfo(link).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");
    QVERIFY2(QFileInfo(link).isDir(), "a link to a directory resolves as a directory");

    // Copying the link recreates a link; it never walks into the target, so a
    // destination inside the target is not a recursion risk.
    QVERIFY2(!FileOperationsController::destinationIsInsideSource(
                 {link}, fx.path(QStringLiteral("linkTarget/inner"))),
             "a symlink source must not block destinations inside its target");
    QVERIFY(!FileOperationsController::destinationIsInsideSource(
        {link}, fx.path(QStringLiteral("linkTarget"))));
}

void CopyMoveRejectsContainedDestinationTest::oneOffendingSourceAmongManyIsEnough() {
    Fixture fx;
    QVERIFY(fx.build());

    QStringList paths = {fx.path(QStringLiteral("sibling")),
                         fx.path(QStringLiteral("loose.txt")),
                         fx.source(),
                         fx.path(QStringLiteral("unrelated"))};
    QVERIFY2(FileOperationsController::destinationIsInsideSource(
                 paths, fx.path(QStringLiteral("source/child/deep"))),
             "the whole operation must be refused if any one source contains the destination");

    // The same set with a destination outside every source goes through.
    QVERIFY(!FileOperationsController::destinationIsInsideSource(
        paths, fx.path(QStringLiteral("linkTarget/inner"))));
}

// canonicalFilePath() is empty for a missing destination, so the guard must canonicalize the existing ancestor
// chain; route: links/sourceLink -> source/, destination a not-yet-created subdirectory beneath the link.
void CopyMoveRejectsContainedDestinationTest::aNonExistentDestinationUnderASymlinkedRouteToTheSourceIsContained() {
    Fixture fx;
    QVERIFY(fx.build());

    QString link = fx.path(QStringLiteral("links/sourceLink"));
    if(!QFile::link(fx.source(), link) || !QFileInfo(link).isSymLink())
        QSKIP("filesystem/platform does not support symlinks");

    QString destination = link + QStringLiteral("/newsub");
    QVERIFY2(!QFileInfo(destination).exists(), "the destination must not exist yet for this case to be meaningful");
    QVERIFY2(FileOperationsController::destinationIsInsideSource({fx.source()}, destination),
             "a non-existent destination reached through a symlinked route to the source must still be refused");
}

// Same shape, but nothing links back to a source: a non-existent destination
// must not be flagged just because it fails to canonicalize.
void CopyMoveRejectsContainedDestinationTest::aNonExistentDestinationOutsideEverySourceIsNotContained() {
    Fixture fx;
    QVERIFY(fx.build());

    QString destination = fx.path(QStringLiteral("sibling/newsub"));
    QVERIFY2(!QFileInfo(destination).exists(), "the destination must not exist yet for this case to be meaningful");
    QVERIFY2(!FileOperationsController::destinationIsInsideSource({fx.source()}, destination),
             "a non-existent destination that is genuinely outside every source must be accepted");
}

// Core::pasteFile() calls interactiveCopy() directly, bypassing copyPathsTo()'s guard, so these drive the executors
// every route ends in; the refusal must happen before any copying starts.
void CopyMoveRejectsContainedDestinationTest::theCopyExecutorRefusesTheSourcesOwnSubfolder() {
    Fixture fx;
    QVERIFY(fx.build());

    MW *window = new MW();
    FileOperationsController ops(window);

    const QString child = fx.path(QStringLiteral("source/child"));
    QVERIFY(QFileInfo(child).isDir());

    QVERIFY2(!ops.interactiveCopy({fx.source()}, child),
             "pasting a folder into its own subfolder must be refused by the executor");
    QVERIFY2(!QFileInfo(child + QStringLiteral("/source")).exists(),
             "the refusal must land before any copying starts");
}

void CopyMoveRejectsContainedDestinationTest::theMoveExecutorRefusesTheSourcesOwnSubfolder() {
    Fixture fx;
    QVERIFY(fx.build());

    MW *window = new MW();
    FileOperationsController ops(window);

    const QString child = fx.path(QStringLiteral("source/child"));
    QVERIFY2(!ops.interactiveMove({fx.source()}, child),
             "moving a folder into its own subfolder must be refused by the executor");
    QVERIFY2(QFileInfo(fx.source()).isDir(), "the source must survive a refused move");
}

// Positive control: the executor must still do ordinary work, or the guard
// above could pass by refusing everything.
void CopyMoveRejectsContainedDestinationTest::theCopyExecutorStillCopiesToAnUnrelatedFolder() {
    Fixture fx;
    QVERIFY(fx.build());

    MW *window = new MW();
    FileOperationsController ops(window);

    const QString dest = fx.path(QStringLiteral("unrelated"));
    QVERIFY2(ops.interactiveCopy({fx.path(QStringLiteral("loose.txt"))}, dest),
             "an ordinary copy to an unrelated folder must still be accepted");
    QVERIFY2(QFileInfo(dest + QStringLiteral("/loose.txt")).isFile(),
             "the file must actually arrive");
}

TG_BEHAVIOR_TEST_MAIN(CopyMoveRejectsContainedDestinationTest)

#include "test_copy_move_rejects_contained_destination.moc"
