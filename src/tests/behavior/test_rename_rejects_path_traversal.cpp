#include "support/thumbgrid_test_support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>
#include "utils/fileoperations.h"

// Regression guard for a path-traversal bug: FileOperations::rename() built its
// destination as srcParent + "/" + newName, and newName is free-form text from
// the rename editors. A newName of "../evil.txt" therefore turned a rename into
// an out-of-folder MOVE - and with force=true it could reach the overwrite path
// and clobber a file outside the browsed folder.
//
// FileOperations::isValidFileName() now gates rename(), which returns
// FileOpResult::INVALID_NAME. These tests assert on concrete filesystem state
// (the source is still there, nothing appeared outside the browsed folder, and
// the bystander file's CONTENTS are untouched), not only on the return code.
//
// Every rejection case is run with force=false and force=true: the traversal
// must be rejected *before* the overwrite path is reachable.

namespace {

const QByteArray kSourceContents = "source contents";
const QByteArray kVictimContents = "victim contents - must never change";

bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(data) == data.size();
}

QByteArray readFile(const QString &path) {
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly))
        return QByteArray("<unreadable>");
    return f.readAll();
}

// <tmp>/browsed/source.txt  (the file being renamed)
// <tmp>/outside/victim.txt  (the bystander a traversal would reach)
struct Fixture {
    QTemporaryDir tmp;

    QString root() const { return tmp.path(); }
    QString browsed() const { return tmp.path() + QStringLiteral("/browsed"); }
    QString outside() const { return tmp.path() + QStringLiteral("/outside"); }
    QString source() const { return browsed() + QStringLiteral("/source.txt"); }
    QString victim() const { return outside() + QStringLiteral("/victim.txt"); }

    bool build() {
        QDir dir(tmp.path());
        return tmp.isValid() && dir.mkpath(QStringLiteral("browsed")) &&
               dir.mkpath(QStringLiteral("outside")) && writeFile(source(), kSourceContents) &&
               writeFile(victim(), kVictimContents);
    }

    static QStringList entries(const QString &dirPath) {
        return QDir(dirPath).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                                       QDir::Name);
    }
    QStringList rootEntries() const { return entries(root()); }
    QStringList browsedEntries() const { return entries(browsed()); }
    QStringList outsideEntries() const { return entries(outside()); }
};

} // namespace

class RenameRejectsPathTraversalTest : public QObject {
    Q_OBJECT
private slots:
    void traversalNamesAreRejectedAndNothingEscapesTheFolder_data();
    void traversalNamesAreRejectedAndNothingEscapesTheFolder();
    void ordinaryNamesAreStillRenamed_data();
    void ordinaryNamesAreStillRenamed();
    void emptyNameAndUnchangedNameStillDoNothing();
    void overwriteSemanticsStillApplyToValidNames();
    void isValidFileNameRejects_data();
    void isValidFileNameRejects();
    void isValidFileNameAccepts_data();
    void isValidFileNameAccepts();
};

void RenameRejectsPathTraversalTest::traversalNamesAreRejectedAndNothingEscapesTheFolder_data() {
    QTest::addColumn<QString>("newName");
    QTest::addColumn<bool>("force");

    const QList<QPair<const char *, QString>> names = {
        {"parent escape", QStringLiteral("../escaped.txt")},
        {"grandparent escape", QStringLiteral("../../escaped.txt")},
        {"forward-slash subpath", QStringLiteral("sub/child.txt")},
        {"backslash subpath", QStringLiteral("sub\\child.txt")},
        {"current dir", QStringLiteral(".")},
        {"parent dir", QStringLiteral("..")},
        {"absolute path", QStringLiteral("/tmp/evil.txt")},
        // The sharpest form of the bug: a relative walk that lands exactly on a
        // real file outside the browsed folder.
        {"walk onto the bystander", QStringLiteral("../outside/victim.txt")},
    };
    for(const auto &entry : names) {
        QTest::newRow(QByteArray(entry.first) + ", force=false") << entry.second << false;
        QTest::newRow(QByteArray(entry.first) + ", force=true") << entry.second << true;
    }
}

void RenameRejectsPathTraversalTest::traversalNamesAreRejectedAndNothingEscapesTheFolder() {
    QFETCH(QString, newName);
    QFETCH(bool, force);

    Fixture fx;
    QVERIFY(fx.build());

    FileOpResult result = FileOpResult::SUCCESS;
    FileOperations::rename(fx.source(), newName, force, result);

    QCOMPARE(result, FileOpResult::INVALID_NAME);

    // The rename must not have happened in any form.
    QVERIFY2(QFileInfo(fx.source()).isFile(), "the source file must still be there");
    QCOMPARE(readFile(fx.source()), kSourceContents);

    // Nothing may have been created outside the browsed folder...
    QCOMPARE(fx.browsedEntries(), QStringList{QStringLiteral("source.txt")});
    QCOMPARE(fx.outsideEntries(), QStringList{QStringLiteral("victim.txt")});
    QCOMPARE(fx.rootEntries(), (QStringList{QStringLiteral("browsed"), QStringLiteral("outside")}));

    // ...and the bystander's CONTENTS must be byte-for-byte what they were: an
    // overwrite that replaced it would leave the name in place.
    QVERIFY2(QFileInfo(fx.victim()).isFile(), "the bystander file must survive");
    QCOMPARE(readFile(fx.victim()), kVictimContents);
}

void RenameRejectsPathTraversalTest::ordinaryNamesAreStillRenamed_data() {
    QTest::addColumn<QString>("newName");

    QTest::newRow("plain") << QStringLiteral("renamed.txt");
    QTest::newRow("spaces") << QStringLiteral("with spaces.txt");
    QTest::newRow("unicode") << QStringLiteral("日本語 café.txt");
    QTest::newRow("multiple dots") << QStringLiteral("multiple.dots.tar.gz");
    QTest::newRow("leading dot") << QStringLiteral(".hidden");
}

void RenameRejectsPathTraversalTest::ordinaryNamesAreStillRenamed() {
    QFETCH(QString, newName);

    Fixture fx;
    QVERIFY(fx.build());

    FileOpResult result = FileOpResult::OTHER_ERROR;
    FileOperations::rename(fx.source(), newName, false, result);

    QCOMPARE(result, FileOpResult::SUCCESS);
    QVERIFY2(!QFileInfo(fx.source()).exists(), "the old name must be gone");
    QString renamed = fx.browsed() + QStringLiteral("/") + newName;
    QVERIFY2(QFileInfo(renamed).isFile(), qPrintable(QStringLiteral("expected ") + renamed));
    QCOMPARE(readFile(renamed), kSourceContents);
    // Still exactly one file, still inside the browsed folder.
    QCOMPARE(fx.browsedEntries(), QStringList{newName});
    QCOMPARE(fx.outsideEntries(), QStringList{QStringLiteral("victim.txt")});
}

void RenameRejectsPathTraversalTest::emptyNameAndUnchangedNameStillDoNothing() {
    Fixture fx;
    QVERIFY(fx.build());

    FileOpResult empty = FileOpResult::OTHER_ERROR;
    FileOperations::rename(fx.source(), QString(), false, empty);
    QVERIFY2(empty == FileOpResult::NOTHING_TO_DO,
             "an empty name is a no-op, not an invalid name");

    FileOpResult same = FileOpResult::OTHER_ERROR;
    FileOperations::rename(fx.source(), QStringLiteral("source.txt"), false, same);
    QCOMPARE(same, FileOpResult::NOTHING_TO_DO);

    QVERIFY(QFileInfo(fx.source()).isFile());
    QCOMPARE(readFile(fx.source()), kSourceContents);
    QCOMPARE(fx.browsedEntries(), QStringList{QStringLiteral("source.txt")});
}

void RenameRejectsPathTraversalTest::overwriteSemanticsStillApplyToValidNames() {
    Fixture fx;
    QVERIFY(fx.build());
    QString occupied = fx.browsed() + QStringLiteral("/taken.txt");
    QVERIFY(writeFile(occupied, "old resident"));

    FileOpResult refused = FileOpResult::SUCCESS;
    FileOperations::rename(fx.source(), QStringLiteral("taken.txt"), false, refused);
    QVERIFY2(refused == FileOpResult::DESTINATION_FILE_EXISTS,
             "a same-folder collision must still report the existing file");
    QVERIFY(QFileInfo(fx.source()).isFile());
    QCOMPARE(readFile(occupied), QByteArray("old resident"));

    FileOpResult forced = FileOpResult::OTHER_ERROR;
    FileOperations::rename(fx.source(), QStringLiteral("taken.txt"), true, forced);
    QCOMPARE(forced, FileOpResult::SUCCESS);
    QVERIFY2(!QFileInfo(fx.source()).exists(), "the source must have moved onto the taken name");
    QCOMPARE(readFile(occupied), kSourceContents);
    QCOMPARE(fx.browsedEntries(), QStringList{QStringLiteral("taken.txt")});
}

void RenameRejectsPathTraversalTest::isValidFileNameRejects_data() {
    QTest::addColumn<QString>("name");

    QTest::newRow("empty") << QString();
    QTest::newRow("parent escape") << QStringLiteral("../escaped.txt");
    QTest::newRow("grandparent escape") << QStringLiteral("../../escaped.txt");
    QTest::newRow("forward-slash subpath") << QStringLiteral("sub/child.txt");
    QTest::newRow("backslash subpath") << QStringLiteral("sub\\child.txt");
    QTest::newRow("trailing slash") << QStringLiteral("name/");
    QTest::newRow("current dir") << QStringLiteral(".");
    QTest::newRow("parent dir") << QStringLiteral("..");
    QTest::newRow("absolute path") << QStringLiteral("/tmp/evil.txt");
    QTest::newRow("bare root") << QStringLiteral("/");
    QTest::newRow("walk onto the bystander") << QStringLiteral("../outside/victim.txt");
}

void RenameRejectsPathTraversalTest::isValidFileNameRejects() {
    QFETCH(QString, name);
    QVERIFY2(!FileOperations::isValidFileName(name), qPrintable(QStringLiteral("accepted: ") + name));
}

void RenameRejectsPathTraversalTest::isValidFileNameAccepts_data() {
    QTest::addColumn<QString>("name");

    QTest::newRow("plain") << QStringLiteral("renamed.txt");
    QTest::newRow("spaces") << QStringLiteral("with spaces.txt");
    QTest::newRow("unicode") << QStringLiteral("日本語 café.txt");
    QTest::newRow("multiple dots") << QStringLiteral("multiple.dots.tar.gz");
    QTest::newRow("leading dot") << QStringLiteral(".hidden");
    QTest::newRow("dots inside") << QStringLiteral("a..b.txt");
    QTest::newRow("no extension") << QStringLiteral("README");
}

void RenameRejectsPathTraversalTest::isValidFileNameAccepts() {
    QFETCH(QString, name);
    QVERIFY2(FileOperations::isValidFileName(name), qPrintable(QStringLiteral("rejected: ") + name));
}

TG_BEHAVIOR_TEST_MAIN(RenameRejectsPathTraversalTest)

#include "test_rename_rejects_path_traversal.moc"
