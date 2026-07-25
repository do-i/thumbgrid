#include "support/thumbgrid_test_support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>
#include "components/fileoperationscontroller.h"

// Regression guard for a recursive copy/move: dropping a folder into itself (or
// into anything beneath itself) makes the copy walk the tree it is still
// writing into, filling the disk with an ever-deeper nesting - and as a move it
// can destroy the source.
//
// FileOperationsController::destinationIsInsideSource() is the single central
// guard used by both copyPathsTo() and movePathsTo(). It canonicalizes both
// sides, so a destination reached through a symlink cannot sneak past it, while
// a *source* that is itself a symlink is not treated as a container: links are
// recreated as links and never recurse.
//
// The function is a public static, so these tests exercise the rule directly -
// no window, no dialogs, no live controller.

namespace {

bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(data) == data.size();
}

// Shared layout:
//   <tmp>/source/child/deep/     the source directory and its subtree
//   <tmp>/sibling/              next to the source
//   <tmp>/sourceExtra/          shares the source's name as a prefix
//   <tmp>/unrelated/nested/     nothing to do with the source
//   <tmp>/linkTarget/inner/     the target of a directory symlink
//   <tmp>/links/                where symlinks live
//   <tmp>/loose.txt             a plain file used as a "source"
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

// Regression case for S4: canonicalFilePath() returns an empty string for a
// destination that does not exist, so the containment guard must not fall
// back to a raw, non-canonical path for the whole destination - it has to
// canonicalize the existing ancestor chain and only leave the missing tail
// untouched. Route: source/ is real, links/sourceLink -> source/, and the
// destination is a not-yet-created subdirectory reached through the link.
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

TG_BEHAVIOR_TEST_MAIN(CopyMoveRejectsContainedDestinationTest)

#include "test_copy_move_rejects_contained_destination.moc"
