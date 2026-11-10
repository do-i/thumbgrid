#pragma once

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QtGlobal>
#include <cstdint>

enum FileOpResult : std::uint8_t {
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
    INVALID_NAME,
    DESTINATION_INSIDE_SOURCE,
    OTHER_ERROR
};

class FileOperations {
public:
    static void copyFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result);
    static void moveFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result);
    // symlink-aware copy/move: recreates the link itself instead of dereferencing its target
    static void copySymLinkTo(const QString &srcLinkPath, const QString &destDirPath, bool force, FileOpResult &result);
    static void moveSymLinkTo(const QString &srcLinkPath, const QString &destDirPath, bool force, FileOpResult &result);
    // true when path (the entry itself, links not followed) can be renamed into dirPath: same filesystem
    // *and* same mount, since rename(2) fails with EXDEV across bind mounts of one filesystem too. Also
    // true when either cannot be stat'ed - then the caller's normal path reports the error.
    static bool onSameFilesystem(const QString &path, const QString &dirPath);
    // Moves a whole directory with one rename(2) - instant on the same filesystem. Never merges into or
    // replaces an existing destination (DESTINATION_DIR_EXISTS / DESTINATION_FILE_EXISTS); any other
    // failure (e.g. a different filesystem) leaves everything untouched, so the caller can fall back to
    // the recursive per-entry move.
    static void moveDirTo(const QString &srcDirPath, const QString &destDirPath, FileOpResult &result);
    static void rename(const QString &srcFilePath, const QString &newName, bool force, FileOpResult &result);
    // true only for a single leaf filename component (no separators, no traversal)
    static bool isValidFileName(const QString &name);
    static void checkCanRemove(const QString &filePath, FileOpResult &result);
    static void removeFile(const QString &filePath, FileOpResult &result);
    // unlinks the link itself; never dereferences or touches its target
    static void removeSymLink(const QString &linkPath, FileOpResult &result);
    static void removeDir(const QString &dirPath, bool recursive, FileOpResult &result);
    static void createDirectory(const QString &dirPath, FileOpResult &result);
    static void moveToTrash(const QString &filePath, FileOpResult &result);

    static QString decodeResult(const FileOpResult &result);

private:
    static QString generateHash(const QString &str);
    // rename(2) that refuses to replace an existing destination; false on any failure (EXDEV, EEXIST, ...)
    static bool renameNoReplace(const QString &srcPath, const QString &dstPath);
    // If destFile exists, check it may be replaced and (with force) move it aside so a failed copy/move
    // can restore it. SUCCESS means proceed.
    static FileOpResult backupExistingDestination(const QFileInfo &destFile, bool force, QString &tmpPath, bool &backedUp);
    // restore mtime/atime after QFile::copy(); false if the file could not be reopened
    static bool restoreFileTimestamps(const QString &filePath, const QDateTime &modTime, const QDateTime &readTime);
};
