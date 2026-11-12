#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <sys/stat.h>

#include "utils/fileoperations.h"
#include "utils/fileoperations_platform.h"

namespace {

const QByteArray sourceData = QByteArrayLiteral("source bytes\0including binary data");
const QByteArray destinationData = "original destination bytes";

bool writeFile(const QString &path, const QByteArray &data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QByteArray readFile(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<unreadable>");
}

quint64 inodeOf(const QString &path) {
    struct stat info {};
    return ::lstat(QFile::encodeName(path).constData(), &info) == 0 ? info.st_ino : 0;
}

QStringList entries(const QString &path) {
    return QDir(path).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                               QDir::Name);
}

enum DestinationKind { File, EmptyDirectory, PopulatedDirectory, Link, DanglingLink };

bool makeDestination(const QString &root, DestinationKind kind) {
    const QString path = root + "/dst/entry";
    switch(kind) {
    case File:
        return writeFile(path, destinationData);
    case EmptyDirectory:
        return QDir().mkdir(path);
    case PopulatedDirectory:
        return QDir().mkdir(path) && writeFile(path + "/kept.txt", destinationData);
    case Link:
        return writeFile(root + "/target.txt", destinationData) && QFile::link(root + "/target.txt", path);
    case DanglingLink:
        return QFile::link(root + "/missing-target", path);
    }
    return false;
}

void verifyDestination(const QString &root, DestinationKind kind, quint64 inode) {
    const QString path = root + "/dst/entry";
    QCOMPARE(inodeOf(path), inode);
    switch(kind) {
    case File:
        QCOMPARE(readFile(path), destinationData);
        break;
    case EmptyDirectory:
        QVERIFY(QFileInfo(path).isDir());
        QVERIFY(entries(path).isEmpty());
        break;
    case PopulatedDirectory:
        QCOMPARE(entries(path), QStringList{"kept.txt"});
        QCOMPARE(readFile(path + "/kept.txt"), destinationData);
        break;
    case Link:
        QVERIFY(QFileInfo(path).isSymLink());
        QCOMPARE(QFileInfo(path).symLinkTarget(), root + "/target.txt");
        QCOMPARE(readFile(root + "/target.txt"), destinationData);
        break;
    case DanglingLink:
        QVERIFY(QFileInfo(path).isSymLink());
        QVERIFY(!QFileInfo(path).exists());
        QCOMPARE(QFileInfo(path).symLinkTarget(), root + "/missing-target");
        QVERIFY(!QFileInfo::exists(root + "/missing-target"));
        break;
    }
    QCOMPARE(entries(root + "/dst"), QStringList{"entry"});
}

void collisionRows() {
    QTest::addColumn<bool>("directorySource");
    QTest::addColumn<int>("destinationKind");
    const char *names[] = {"file", "empty directory", "populated directory", "symlink", "dangling symlink"};
    for(bool directorySource : {false, true}) {
        for(int kind = File; kind <= DanglingLink; ++kind) {
            const QByteArray name = QByteArray(directorySource ? "folder onto " : "file onto ") + names[kind];
            QTest::newRow(name.constData()) << directorySource << kind;
        }
    }
}

bool buildSource(const QString &root, bool directory) {
    return QDir(root).mkpath("src") && QDir(root).mkpath("dst") &&
           (!directory || QDir(root).mkpath("src/entry/nested")) &&
           writeFile(root + (directory ? "/src/entry/nested/data.bin" : "/src/entry"), sourceData);
}

void verifySource(const QString &root, bool directory, quint64 inode) {
    QCOMPARE(inodeOf(root + "/src/entry"), inode);
    QCOMPARE(readFile(root + (directory ? "/src/entry/nested/data.bin" : "/src/entry")), sourceData);
}

// Restore permissions even when an assertion returns early, so the temporary directory can be removed.
struct RestorePermissions {
    QString path;
    QFile::Permissions permissions;
    explicit RestorePermissions(const QString &value) : path(value), permissions(QFile::permissions(value)) {}
    ~RestorePermissions() { QFile::setPermissions(path, permissions); }
};

struct CrossFs {
    QTemporaryDir src;
    QTemporaryDir dst{QDir(qEnvironmentVariable("THUMBGRID_TEST_CROSS_FS_DIR", "/dev/shm"))
                          .filePath("thumbgrid-move-safety-XXXXXX")};
    bool usable() const {
        return src.isValid() && dst.isValid() &&
               FileOperationsPlatform::filesystemRelation(src.path(), dst.path()) ==
                   FileOperationsPlatform::FilesystemRelation::Different;
    }
};

} // namespace

class FileMoveSafetyTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        if(qEnvironmentVariableIsSet("THUMBGRID_TEST_CROSS_FS_DIR")) {
            CrossFs fs;
            QVERIFY2(fs.usable(), "THUMBGRID_TEST_CROSS_FS_DIR must be writable and on a separate filesystem");
        }
    }
    void nativeRenameNeverReplacesAnOccupiedDestination_data() { collisionRows(); }
    void nativeRenameNeverReplacesAnOccupiedDestination();
    void publicMovesPreserveCollidingEntries_data() { collisionRows(); }
    void publicMovesPreserveCollidingEntries();
    void sameFilesystemMovesPreserveContentAndInodes_data();
    void sameFilesystemMovesPreserveContentAndInodes();
    void forcedReplacementMovesTheFileAndRemovesItsBackup_data();
    void forcedReplacementMovesTheFileAndRemovesItsBackup();
    void failedForcedRenameRestoresBothFiles();
    void forcedReplacementPreservesDataWithOrdinaryAndLongNames_data();
    void forcedReplacementPreservesDataWithOrdinaryAndLongNames();
    void failedForcedCopyRestoresBothFiles();
    void directoryPermissionFailuresDoNotRequestCopy();
    void missingPathsHaveUnknownFilesystemAndCannotMove();
    void crossFilesystemRenameRefusesButFileMoveCopiesSafely();
    void crossFilesystemFolderRenameRequiresCopyAndPreservesSource();
    void failedCrossFilesystemCopyRestoresBothFiles();
    void failedCrossFilesystemSourceRemovalRestoresBothFiles();
};

void FileMoveSafetyTest::nativeRenameNeverReplacesAnOccupiedDestination() {
    QFETCH(bool, directorySource);
    QFETCH(int, destinationKind);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(buildSource(tmp.path(), directorySource));
    // The occupied entry is created after the same preliminary check a caller could perform.
    QVERIFY(!QFileInfo::exists(tmp.path() + "/dst/entry"));
    QVERIFY(makeDestination(tmp.path(), DestinationKind(destinationKind)));
    const auto sourceInode = inodeOf(tmp.path() + "/src/entry");
    const auto destinationInode = inodeOf(tmp.path() + "/dst/entry");

    QCOMPARE(FileOperationsPlatform::renameNoReplace(tmp.path() + "/src/entry", tmp.path() + "/dst/entry"),
             FileOperationsPlatform::RenameResult::DestinationExists);

    verifySource(tmp.path(), directorySource, sourceInode);
    verifyDestination(tmp.path(), DestinationKind(destinationKind), destinationInode);
}

void FileMoveSafetyTest::publicMovesPreserveCollidingEntries() {
    QFETCH(bool, directorySource);
    QFETCH(int, destinationKind);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(buildSource(tmp.path(), directorySource));
    QVERIFY(makeDestination(tmp.path(), DestinationKind(destinationKind)));
    const auto sourceInode = inodeOf(tmp.path() + "/src/entry");
    const auto destinationInode = inodeOf(tmp.path() + "/dst/entry");
    FileOpResult result = SUCCESS;
    if(directorySource)
        FileOperations::moveDirTo(tmp.path() + "/src/entry", tmp.path() + "/dst", result);
    else
        FileOperations::moveFileTo(tmp.path() + "/src/entry", tmp.path() + "/dst", false, result);
    QCOMPARE(result, (destinationKind == EmptyDirectory || destinationKind == PopulatedDirectory)
                         ? DESTINATION_DIR_EXISTS : DESTINATION_FILE_EXISTS);
    verifySource(tmp.path(), directorySource, sourceInode);
    verifyDestination(tmp.path(), DestinationKind(destinationKind), destinationInode);
}

void FileMoveSafetyTest::sameFilesystemMovesPreserveContentAndInodes_data() {
    QTest::addColumn<bool>("directorySource");
    QTest::newRow("file") << false;
    QTest::newRow("nested folder") << true;
}

void FileMoveSafetyTest::sameFilesystemMovesPreserveContentAndInodes() {
    QFETCH(bool, directorySource);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(buildSource(tmp.path(), directorySource));
    const QString src = tmp.path() + "/src/entry";
    const QString dst = tmp.path() + "/dst/entry";
    const auto inode = inodeOf(src);
    const auto dataInode = inodeOf(directorySource ? src + "/nested/data.bin" : src);
    QVERIFY(inode != 0);
    QVERIFY(FileOperations::onSameFilesystem(src, tmp.path() + "/dst"));
    QCOMPARE(FileOperationsPlatform::filesystemRelation(src, tmp.path() + "/dst"),
             FileOperationsPlatform::FilesystemRelation::Same);
    FileOpResult result = OTHER_ERROR;
    if(directorySource)
        FileOperations::moveDirTo(src, tmp.path() + "/dst", result);
    else
        FileOperations::moveFileTo(src, tmp.path() + "/dst", false, result);
    QCOMPARE(result, SUCCESS);
    QVERIFY(!QFileInfo::exists(src));
    QCOMPARE(inodeOf(dst), inode);
    QCOMPARE(inodeOf(directorySource ? dst + "/nested/data.bin" : dst), dataInode);
    QCOMPARE(readFile(directorySource ? dst + "/nested/data.bin" : dst), sourceData);
}

void FileMoveSafetyTest::forcedReplacementMovesTheFileAndRemovesItsBackup_data() {
    QTest::addColumn<int>("destinationKind");
    QTest::newRow("file") << int(File);
    QTest::newRow("symlink") << int(Link);
    QTest::newRow("dangling symlink") << int(DanglingLink);
}

void FileMoveSafetyTest::forcedReplacementMovesTheFileAndRemovesItsBackup() {
    QFETCH(int, destinationKind);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(buildSource(tmp.path(), false));
    QVERIFY(makeDestination(tmp.path(), DestinationKind(destinationKind)));
    const auto inode = inodeOf(tmp.path() + "/src/entry");
    FileOpResult result = OTHER_ERROR;
    FileOperations::moveFileTo(tmp.path() + "/src/entry", tmp.path() + "/dst", true, result);
    QCOMPARE(result, SUCCESS);
    QVERIFY(!QFileInfo::exists(tmp.path() + "/src/entry"));
    QCOMPARE(readFile(tmp.path() + "/dst/entry"), sourceData);
    QVERIFY(!QFileInfo(tmp.path() + "/dst/entry").isSymLink());
    QCOMPARE(inodeOf(tmp.path() + "/dst/entry"), inode);
    QCOMPARE(entries(tmp.path() + "/dst"), QStringList{"entry"});
    if(destinationKind == Link)
        QCOMPARE(readFile(tmp.path() + "/target.txt"), destinationData);
    if(destinationKind == DanglingLink)
        QVERIFY(!QFileInfo::exists(tmp.path() + "/missing-target"));
}

void FileMoveSafetyTest::failedForcedRenameRestoresBothFiles() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(buildSource(tmp.path(), false));
    QVERIFY(makeDestination(tmp.path(), File));
    const auto sourceInode = inodeOf(tmp.path() + "/src/entry");
    const auto destinationInode = inodeOf(tmp.path() + "/dst/entry");
    RestorePermissions restore(tmp.path() + "/src");
    QVERIFY(QFile::setPermissions(restore.path, QFile::ReadOwner | QFile::ExeOwner));
    if(QFileInfo(restore.path).isWritable())
        QSKIP("requires an unprivileged user whose source directory permissions are enforced");
    QCOMPARE(FileOperationsPlatform::renameNoReplace(tmp.path() + "/src/entry", tmp.path() + "/dst/probe"),
             FileOperationsPlatform::RenameResult::Error);
    FileOpResult result = SUCCESS;
    FileOperations::moveFileTo(tmp.path() + "/src/entry", tmp.path() + "/dst", true, result);
    QVERIFY(result != SUCCESS);
    verifySource(tmp.path(), false, sourceInode);
    verifyDestination(tmp.path(), File, destinationInode);
    QVERIFY(!QFileInfo::exists(tmp.path() + "/dst/probe"));
}

void FileMoveSafetyTest::forcedReplacementPreservesDataWithOrdinaryAndLongNames_data() {
    QTest::addColumn<bool>("copy");
    QTest::addColumn<int>("nameLength");
    QTest::newRow("ordinary file move") << false << 5;
    QTest::newRow("long file move") << false << 240;
    QTest::newRow("ordinary file copy") << true << 5;
    QTest::newRow("long file copy") << true << 240;
}

void FileMoveSafetyTest::forcedReplacementPreservesDataWithOrdinaryAndLongNames() {
    QFETCH(bool, copy);
    QFETCH(int, nameLength);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath("src") && QDir(tmp.path()).mkpath("dst"));
    const QString name(nameLength, QLatin1Char('x'));
    const QString source = tmp.path() + "/src/" + name;
    const QString destination = tmp.path() + "/dst/" + name;
    QVERIFY(writeFile(source, sourceData));
    QVERIFY(writeFile(destination, destinationData));
    const auto sourceInode = inodeOf(source);
    FileOpResult result = OTHER_ERROR;
    if(copy)
        FileOperations::copyFileTo(source, tmp.path() + "/dst", true, result);
    else
        FileOperations::moveFileTo(source, tmp.path() + "/dst", true, result);
    QCOMPARE(result, SUCCESS);
    QCOMPARE(readFile(destination), sourceData);
    if(copy) {
        QCOMPARE(readFile(source), sourceData);
        QCOMPARE(inodeOf(source), sourceInode);
    } else {
        QVERIFY(!QFileInfo::exists(source));
        QCOMPARE(inodeOf(destination), sourceInode);
    }
    QCOMPARE(entries(tmp.path() + "/dst"), QStringList{name});
}

void FileMoveSafetyTest::failedForcedCopyRestoresBothFiles() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(buildSource(tmp.path(), false));
    QVERIFY(makeDestination(tmp.path(), File));
    const QString source = tmp.path() + "/src/entry";
    const auto sourceInode = inodeOf(source);
    const auto destinationInode = inodeOf(tmp.path() + "/dst/entry");
    RestorePermissions restore(source);
    QVERIFY(QFile::setPermissions(source, QFile::WriteOwner));
    QFile probe(source);
    if(probe.open(QIODevice::ReadOnly))
        QSKIP("requires an unprivileged user whose source read permissions are enforced");
    FileOpResult result = SUCCESS;
    FileOperations::copyFileTo(source, tmp.path() + "/dst", true, result);
    QVERIFY(result != SUCCESS);
    QVERIFY(QFile::setPermissions(source, restore.permissions));
    verifySource(tmp.path(), false, sourceInode);
    verifyDestination(tmp.path(), File, destinationInode);
}

void FileMoveSafetyTest::directoryPermissionFailuresDoNotRequestCopy() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(buildSource(tmp.path(), true));
    const QString source = tmp.path() + "/src/entry";
    const auto sourceInode = inodeOf(source);
    RestorePermissions restore(tmp.path() + "/src");
    QVERIFY(QFile::setPermissions(restore.path, QFile::ReadOwner | QFile::ExeOwner));
    if(QFileInfo(restore.path).isWritable())
        QSKIP("requires an unprivileged user whose source directory permissions are enforced");
    QCOMPARE(FileOperationsPlatform::renameNoReplace(source, tmp.path() + "/dst/entry"),
             FileOperationsPlatform::RenameResult::Error);
    FileOpResult result = SUCCESS;
    FileOperations::moveDirTo(source, tmp.path() + "/dst", result);
    QCOMPARE(result, PARENT_DIRECTORY_NOT_WRITABLE);
    verifySource(tmp.path(), true, sourceInode);
    QVERIFY(entries(tmp.path() + "/dst").isEmpty());
}

void FileMoveSafetyTest::missingPathsHaveUnknownFilesystemAndCannotMove() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(buildSource(tmp.path(), false));
    QVERIFY(makeDestination(tmp.path(), File));
    const auto destinationInode = inodeOf(tmp.path() + "/dst/entry");
    const QString missing = tmp.path() + "/absent/entry";
    QCOMPARE(FileOperationsPlatform::filesystemRelation(missing, tmp.path() + "/dst"),
             FileOperationsPlatform::FilesystemRelation::Unknown);
    QVERIFY(!FileOperations::onSameFilesystem(missing, tmp.path() + "/dst"));
    QCOMPARE(FileOperationsPlatform::filesystemRelation(tmp.path() + "/src/entry", tmp.path() + "/missing"),
             FileOperationsPlatform::FilesystemRelation::Unknown);
    QVERIFY(!FileOperations::onSameFilesystem(tmp.path() + "/src/entry", tmp.path() + "/missing"));
    QCOMPARE(FileOperationsPlatform::renameNoReplace(missing, tmp.path() + "/dst/unoccupied"),
             FileOperationsPlatform::RenameResult::Error);
    FileOpResult result = SUCCESS;
    FileOperations::moveFileTo(missing, tmp.path() + "/dst", true, result);
    QCOMPARE(result, SOURCE_DOES_NOT_EXIST);
    verifyDestination(tmp.path(), File, destinationInode);
    QCOMPARE(readFile(tmp.path() + "/src/entry"), sourceData);
}

void FileMoveSafetyTest::crossFilesystemRenameRefusesButFileMoveCopiesSafely() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs a writable separate filesystem; set THUMBGRID_TEST_CROSS_FS_DIR (default /dev/shm)");
    QVERIFY(writeFile(fs.src.path() + "/entry", sourceData));
    QVERIFY(!FileOperations::onSameFilesystem(fs.src.path() + "/entry", fs.dst.path()));
    QCOMPARE(FileOperationsPlatform::renameNoReplace(fs.src.path() + "/entry", fs.dst.path() + "/entry"),
             FileOperationsPlatform::RenameResult::CrossDevice);
    QCOMPARE(readFile(fs.src.path() + "/entry"), sourceData);
    QVERIFY(!QFileInfo::exists(fs.dst.path() + "/entry"));
    QVERIFY(writeFile(fs.dst.path() + "/entry", destinationData));
    FileOpResult result = OTHER_ERROR;
    FileOperations::moveFileTo(fs.src.path() + "/entry", fs.dst.path(), true, result);
    QCOMPARE(result, SUCCESS);
    QVERIFY(!QFileInfo::exists(fs.src.path() + "/entry"));
    QCOMPARE(readFile(fs.dst.path() + "/entry"), sourceData);
    QCOMPARE(entries(fs.dst.path()), QStringList{"entry"});
}

void FileMoveSafetyTest::crossFilesystemFolderRenameRequiresCopyAndPreservesSource() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs a writable separate filesystem; set THUMBGRID_TEST_CROSS_FS_DIR (default /dev/shm)");
    QVERIFY(buildSource(fs.src.path(), true));
    const auto sourceInode = inodeOf(fs.src.path() + "/src/entry");
    FileOpResult result = SUCCESS;
    FileOperations::moveDirTo(fs.src.path() + "/src/entry", fs.dst.path(), result);
    QCOMPARE(result, MOVE_REQUIRES_COPY);
    verifySource(fs.src.path(), true, sourceInode);
    QVERIFY(entries(fs.dst.path()).isEmpty());
}

void FileMoveSafetyTest::failedCrossFilesystemCopyRestoresBothFiles() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs a writable separate filesystem; set THUMBGRID_TEST_CROSS_FS_DIR (default /dev/shm)");
    const QString source = fs.src.path() + "/entry";
    const QString destination = fs.dst.path() + "/entry";
    QVERIFY(writeFile(source, sourceData));
    QVERIFY(writeFile(destination, destinationData));
    const auto sourceInode = inodeOf(source);
    const auto destinationInode = inodeOf(destination);
    RestorePermissions restore(source);
    QVERIFY(QFile::setPermissions(source, QFile::WriteOwner));
    QFile probe(source);
    if(probe.open(QIODevice::ReadOnly))
        QSKIP("requires an unprivileged user whose source read permissions are enforced");
    FileOpResult result = SUCCESS;
    FileOperations::moveFileTo(source, fs.dst.path(), true, result);
    QVERIFY(result != SUCCESS);
    QVERIFY(QFile::setPermissions(source, restore.permissions));
    QCOMPARE(readFile(source), sourceData);
    QCOMPARE(inodeOf(source), sourceInode);
    QCOMPARE(readFile(destination), destinationData);
    QCOMPARE(inodeOf(destination), destinationInode);
    QCOMPARE(entries(fs.dst.path()), QStringList{"entry"});
}

void FileMoveSafetyTest::failedCrossFilesystemSourceRemovalRestoresBothFiles() {
    CrossFs fs;
    if(!fs.usable())
        QSKIP("needs a writable separate filesystem; set THUMBGRID_TEST_CROSS_FS_DIR (default /dev/shm)");
    const QString source = fs.src.path() + "/entry";
    const QString destination = fs.dst.path() + "/entry";
    QVERIFY(writeFile(source, sourceData));
    QVERIFY(writeFile(destination, destinationData));
    const auto sourceInode = inodeOf(source);
    const auto destinationInode = inodeOf(destination);
    RestorePermissions restore(fs.src.path());
    QVERIFY(QFile::setPermissions(restore.path, QFile::ReadOwner | QFile::ExeOwner));
    if(QFileInfo(restore.path).isWritable())
        QSKIP("requires an unprivileged user whose source directory permissions are enforced");
    FileOpResult result = SUCCESS;
    FileOperations::moveFileTo(source, fs.dst.path(), true, result);
    QVERIFY(result != SUCCESS);
    QCOMPARE(readFile(source), sourceData);
    QCOMPARE(inodeOf(source), sourceInode);
    QCOMPARE(readFile(destination), destinationData);
    QCOMPARE(inodeOf(destination), destinationInode);
    QCOMPARE(entries(fs.dst.path()), QStringList{"entry"});
}

QTEST_GUILESS_MAIN(FileMoveSafetyTest)
#include "test_file_moves_preserve_data.moc"
