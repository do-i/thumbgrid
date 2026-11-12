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
    NOTHING_TO_DO,
    INVALID_NAME,
    DESTINATION_INSIDE_SOURCE,
    OTHER_ERROR,
    MOVE_REQUIRES_COPY
};

class FileOperations {
public:
    static void copyFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result);
    static void moveFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result);
    // symlink-aware copy/move: recreates the link itself instead of dereferencing its target
    static void copySymLinkTo(const QString &srcLinkPath, const QString &destDirPath, bool force, FileOpResult &result);
    static void moveSymLinkTo(const QString &srcLinkPath, const QString &destDirPath, bool force, FileOpResult &result);
    // true when path (the entry itself, links not followed) can be renamed into dirPath: same filesystem
    // *and* same mount where available, since rename(2) fails with EXDEV across bind mounts too.
    // Unknown identity returns false so potentially expensive fallback work stays off the UI thread.
    static bool onSameFilesystem(const QString &path, const QString &dirPath);
    // Moves a whole directory with one rename(2) - instant on the same filesystem. Never merges into or
    // replaces an existing destination (DESTINATION_DIR_EXISTS / DESTINATION_FILE_EXISTS).
    // MOVE_REQUIRES_COPY permits a recursive fallback; other errors leave everything untouched.
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
    // If destFile exists, check it may be replaced and (with force) move it aside so a failed copy/move
    // can restore it. SUCCESS means proceed.
    static FileOpResult backupExistingDestination(const QFileInfo &destFile, bool force, QString &tmpPath, bool &backedUp);
    // restore mtime/atime after QFile::copy(); false if reopening or either timestamp update fails
    static bool restoreFileTimestamps(const QString &filePath, const QDateTime &modTime, const QDateTime &readTime);
};
