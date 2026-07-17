#pragma once

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QtGlobal>

enum FileOpResult {
    SUCCESS,
    DESTINATION_FILE_EXISTS,
    DESTINATION_DIR_EXISTS,
    SOURCE_NOT_WRITABLE,
    DESTINATION_NOT_WRITABLE,
    SOURCE_DOES_NOT_EXIST,
    DESTINATION_DOES_NOT_EXIST,
    DIRECTORY_NOT_EMPTY,
    PARENT_DIRECTORY_NOT_WRITABLE,
    NOTHING_TO_DO, // todo: maybe just return SUCCESS?
    OTHER_ERROR
};

class FileOperations {
public:
    static void copyFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result);
    static void moveFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result);
    // symlink-aware copy/move: recreates the link itself instead of dereferencing its target
    static void copySymLinkTo(const QString &srcLinkPath, const QString &destDirPath, bool force, FileOpResult &result);
    static void moveSymLinkTo(const QString &srcLinkPath, const QString &destDirPath, bool force, FileOpResult &result);
    static void rename(const QString &srcFilePath, const QString &newName, bool force, FileOpResult &result);
    static void checkCanRemove(const QString &filePath, FileOpResult &result);
    static void removeFile(const QString &filePath, FileOpResult &result);
    static void removeDir(const QString &dirPath, bool recursive, FileOpResult &result);
    static void createDirectory(const QString &dirPath, FileOpResult &result);
    static void moveToTrash(const QString &filePath, FileOpResult &result);

    static QString decodeResult(const FileOpResult &result);

private:
    static QString generateHash(const QString &str);
    // shared "back up an existing destination file" step for copyFileTo()/moveFileTo():
    // validates destFile can be replaced and, if force is set, moves it aside to a
    // hash-suffixed tmp path so a failed copy/move can restore it. Returns SUCCESS to
    // mean "proceed" (with tmpPath/backedUp set only when destFile existed); any other
    // FileOpResult means bail out with that result.
    static FileOpResult backupExistingDestination(const QFileInfo &destFile, bool force, QString &tmpPath, bool &backedUp);
    // shared "restore mtime/atime after a QFile::copy()" step for copyFileTo()/moveFileTo();
    // returns false if filePath could not be reopened, in which case the timestamps were
    // left untouched
    static bool restoreFileTimestamps(const QString &filePath, const QDateTime &modTime, const QDateTime &readTime);
};
