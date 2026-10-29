#include "fileoperations.h"
#include "fileoperations_platform.h"

QString FileOperations::generateHash(const QString &str) {
    return QString(QCryptographicHash::hash(str.toUtf8(), QCryptographicHash::Md5).toHex());
}

void FileOperations::removeFile(const QString &filePath, FileOpResult &result) {
    checkCanRemove(filePath, result);
    if(result != FileOpResult::SUCCESS)
        return;

    if(QFile::remove(filePath))
        result = FileOpResult::SUCCESS;
    else
        result = FileOpResult::OTHER_ERROR;
    return;
}

void FileOperations::checkCanRemove(const QString &filePath, FileOpResult &result) {
    QFileInfo file(filePath);
    // exists() resolves links; a dangling link must still be removable
    if(!file.exists() && !file.isSymLink()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    if(!FileOperationsPlatform::canRemoveSource(file)) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        return;
    }

    QFileInfo parent(file.absolutePath());
    if(!FileOperationsPlatform::isWritableParentDirectory(parent)) {
        result = FileOpResult::PARENT_DIRECTORY_NOT_WRITABLE;
        return;
    }

    result = FileOpResult::SUCCESS;
}

// non-recursive
void FileOperations::removeSymLink(const QString &linkPath, FileOpResult &result) {
    checkCanRemove(linkPath, result);
    if(result != FileOpResult::SUCCESS)
        return;

    // QFile::remove() unlinks the link itself and never touches its target.
    if(QFile::remove(linkPath))
        result = FileOpResult::SUCCESS;
    else
        result = FileOpResult::OTHER_ERROR;
}

void FileOperations::removeDir(const QString &dirPath, bool recursive, FileOpResult &result) {
    // Unlink a link to a directory: removeRecursively() would delete the target's contents.
    // DirectoryModel::removeDir() guards the trash route.
    if(QFileInfo(dirPath).isSymLink()) {
        removeSymLink(dirPath, result);
        return;
    }
    checkCanRemove(dirPath, result);
    if(result != FileOpResult::SUCCESS)
        return;

    QDir dir(dirPath);
    if(!dir.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
    } else {
        if(recursive ? dir.removeRecursively() : dir.rmdir(dirPath))
            result = FileOpResult::SUCCESS;
        else if(!recursive && !dir.isEmpty())
            result = FileOpResult::DIRECTORY_NOT_EMPTY;
        else
            result = FileOpResult::OTHER_ERROR;
    }
    return;
}

QString FileOperations::decodeResult(const FileOpResult &result) {
    switch(result) {
    case FileOpResult::SUCCESS:
        return QObject::tr("Operation completed succesfully.");
    case FileOpResult::DESTINATION_FILE_EXISTS:
        return QObject::tr("Destination file exists.");
    case FileOpResult::DESTINATION_DIR_EXISTS:
        return QObject::tr("Destination directory exists.");
    case FileOpResult::SOURCE_NOT_WRITABLE:
        return QObject::tr("Source file is not writable.");
    case FileOpResult::DESTINATION_NOT_WRITABLE:
        return QObject::tr("Destination is not writable.");
    case FileOpResult::SOURCE_DOES_NOT_EXIST:
        return QObject::tr("Source file does not exist.");
    case FileOpResult::DESTINATION_DOES_NOT_EXIST:
        return QObject::tr("Destination does not exist.");
    case FileOpResult::DIRECTORY_NOT_EMPTY:
        return QObject::tr("Directory is not empty.");
    case FileOpResult::PARENT_DIRECTORY_NOT_WRITABLE:
        return QObject::tr("Containing directory is not writable.");
    case FileOpResult::NOTHING_TO_DO:
        return QObject::tr("Nothing to do.");
    case FileOpResult::INVALID_NAME:
        return QObject::tr("Invalid name. Enter a file name without slashes, and not \".\" or \"..\".");
    case FileOpResult::DESTINATION_INSIDE_SOURCE:
        return QObject::tr("Cannot copy or move a folder into itself.");
    case FileOpResult::OTHER_ERROR:
        return QObject::tr("Other error.");
    }
    return nullptr;
}

static FileOpResult checkDestinationDir(const QString &destDirPath) {
    QFileInfo destDir(destDirPath);
    if(!destDir.exists())
        return FileOpResult::DESTINATION_DOES_NOT_EXIST;
    if(!destDir.isWritable())
        return FileOpResult::DESTINATION_NOT_WRITABLE;
    return FileOpResult::SUCCESS;
}

// If destFile exists, check it may be replaced and (with force) move it aside so a failed copy/move
// can restore it. SUCCESS means proceed.
FileOpResult FileOperations::backupExistingDestination(const QFileInfo &destFile, bool force, QString &tmpPath, bool &backedUp) {
    backedUp = false;
    if(!destFile.exists())
        return FileOpResult::SUCCESS;
    if(!FileOperationsPlatform::canReplaceExistingDestination(destFile))
        return FileOpResult::DESTINATION_NOT_WRITABLE;
    if(destFile.isDir())
        return FileOpResult::DESTINATION_DIR_EXISTS;
    if(!force)
        return FileOpResult::DESTINATION_FILE_EXISTS;
    // remove just in case it exists
    tmpPath = destFile.absoluteFilePath() + "_" + generateHash(destFile.absoluteFilePath());
    QFile::remove(tmpPath);
    // move backup
    QFile::rename(destFile.absoluteFilePath(), tmpPath);
    backedUp = true;
    return FileOpResult::SUCCESS;
}

// QFile::copy() doesn't preserve mtime/atime; false if the file could not be reopened
bool FileOperations::restoreFileTimestamps(const QString &filePath, const QDateTime &modTime, const QDateTime &readTime) {
    QFile dstF(filePath);
    if(!dstF.open(QIODevice::ReadWrite))
        return false;
    dstF.setFileTime(modTime, QFileDevice::FileModificationTime);
    dstF.setFileTime(readTime, QFileDevice::FileAccessTime);
    dstF.close();
    return true;
}

void FileOperations::copyFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result) {
    QFileInfo srcFile(srcFilePath);
    QString tmpPath;
    bool exists = false;
    // error checks
    if(destDirPath == srcFile.absolutePath()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    if(!srcFile.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    result = checkDestinationDir(destDirPath);
    if(result != FileOpResult::SUCCESS)
        return;
    QFileInfo destFile(destDirPath + "/" + srcFile.fileName());
    result = backupExistingDestination(destFile, force, tmpPath, exists);
    if(result != FileOpResult::SUCCESS)
        return;
    // copy
    auto srcModTime = srcFile.lastModified();
    auto srcReadTime = srcFile.lastRead();
    if(QFile::copy(srcFile.absoluteFilePath(), destFile.absoluteFilePath())) {
        result = FileOpResult::SUCCESS;
        if(!restoreFileTimestamps(destFile.absoluteFilePath(), srcModTime, srcReadTime)) {
            result = FileOpResult::OTHER_ERROR;
            return;
        }
        // ok; remove the backup
        if(exists)
            QFile::remove(tmpPath);
    } else {
        result = FileOpResult::OTHER_ERROR;
        // fail; revert
        QFile::rename(tmpPath, destFile.absoluteFilePath());
    }
    return;
}

void FileOperations::moveFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result) {
    QFileInfo srcFile(srcFilePath);
    QString tmpPath;
    bool exists = false;
    // error checks
    if(destDirPath == srcFile.absolutePath()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    if(!srcFile.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    if(!FileOperationsPlatform::canRemoveSource(srcFile)) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        return;
    }
    result = checkDestinationDir(destDirPath);
    if(result != FileOpResult::SUCCESS)
        return;
    QFileInfo destFile(destDirPath + "/" + srcFile.fileName());
    result = backupExistingDestination(destFile, force, tmpPath, exists);
    if(result != FileOpResult::SUCCESS)
        return;
    // move
    auto srcModTime = srcFile.lastModified();
    auto srcReadTime = srcFile.lastRead();
    if(QFile::copy(srcFile.absoluteFilePath(), destFile.absoluteFilePath())) {
        // remove original file
        FileOpResult removeResult;
        removeFile(srcFile.absoluteFilePath(), removeResult);
        if(removeResult == FileOpResult::SUCCESS) {
            // OK
            result = FileOpResult::SUCCESS;
            if(!restoreFileTimestamps(destFile.absoluteFilePath(), srcModTime, srcReadTime)) {
                result = FileOpResult::OTHER_ERROR;
                return;
            }
            // remove backup
            if(exists)
                QFile::remove(tmpPath);
            return;
        }
        // revert on failure
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        if(QFile::remove(destFile.absoluteFilePath()))
            result = FileOpResult::OTHER_ERROR;
    } else {
        // could not COPY
        result = FileOpResult::OTHER_ERROR;
    }
    if(exists) // failed; revert backup
        QFile::rename(tmpPath, destFile.absoluteFilePath());
    return;
}

// Recreates the link with an absolute target; never copies the target's contents.
void FileOperations::copySymLinkTo(const QString &srcLinkPath, const QString &destDirPath, bool force, FileOpResult &result) {
    QFileInfo srcLink(srcLinkPath);
    if(!srcLink.isSymLink()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    if(destDirPath == srcLink.absolutePath()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    result = checkDestinationDir(destDirPath);
    if(result != FileOpResult::SUCCESS)
        return;
    QString linkTarget = srcLink.symLinkTarget(); // absolute target path (empty if dangling)
    if(linkTarget.isEmpty()) {
        // Can't resolve the target (e.g. broken link); refuse rather than silently lose it.
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    QString destLinkPath = destDirPath + "/" + srcLink.fileName();
    QFileInfo destLink(destLinkPath);
    QString tmpPath;
    bool exists = false;
    if(destLink.exists() || destLink.isSymLink()) {
        if(destLink.isDir() && !destLink.isSymLink()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
            return;
        }
        if(!force) {
            result = FileOpResult::DESTINATION_FILE_EXISTS;
            return;
        }
        // back up whatever is in the way, then restore on failure
        tmpPath = destLinkPath + "_" + generateHash(destLinkPath);
        QFile::remove(tmpPath);
        QFile::rename(destLinkPath, tmpPath);
        exists = true;
    }
    if(QFile::link(linkTarget, destLinkPath)) {
        result = FileOpResult::SUCCESS;
        if(exists)
            QFile::remove(tmpPath);
    } else {
        result = FileOpResult::OTHER_ERROR;
        if(exists)
            QFile::rename(tmpPath, destLinkPath);
    }
}

void FileOperations::moveSymLinkTo(const QString &srcLinkPath, const QString &destDirPath, bool force, FileOpResult &result) {
    copySymLinkTo(srcLinkPath, destDirPath, force, result);
    if(result == FileOpResult::SUCCESS) {
        // remove the original link only (QFile::remove unlinks the link, not its target)
        FileOpResult removeResult;
        removeFile(srcLinkPath, removeResult);
        if(removeResult != FileOpResult::SUCCESS)
            result = removeResult;
    }
}

bool FileOperations::isValidFileName(const QString &name) {
    if(name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral(".."))
        return false;
    // '/' disqualifies; '\' is an ordinary filename character
    if(name.contains(QLatin1Char('/')))
        return false;
    if(QDir::isAbsolutePath(name))
        return false;
    // Belt and braces: whatever the platform considers the leaf must be the
    // whole string, so nothing can smuggle in a directory component.
    return QFileInfo(name).fileName() == name;
}

void FileOperations::rename(const QString &srcFilePath, const QString &newName, bool force, FileOpResult &result) {
    QFileInfo srcFile(srcFilePath);
    QString tmpPath;
    // error checks
    if(!srcFile.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    if(!FileOperationsPlatform::canRemoveSource(srcFile)) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        return;
    }
    if(newName.isEmpty() || newName == srcFile.fileName()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    // newName is free text and the path is concatenated: keep renames inside the folder, for every UI route
    if(!isValidFileName(newName)) {
        result = FileOpResult::INVALID_NAME;
        return;
    }
    QString newFilePath = srcFile.absolutePath() + "/" + newName;
    QFileInfo destFile(newFilePath);
    if(destFile.exists()) {
        if(!FileOperationsPlatform::canReplaceExistingDestination(destFile))
            result = FileOpResult::DESTINATION_NOT_WRITABLE;
        if(destFile.isDir()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
            return;
        }
        if(!force) {
            result = FileOpResult::DESTINATION_FILE_EXISTS;
            return;
        }
        tmpPath = newFilePath + "_" + generateHash(newFilePath);
        QFile::remove(tmpPath);
        // move dest file
        QFile::rename(newFilePath, tmpPath);
    }
    if(QFile::rename(srcFile.filePath(), newFilePath)) {
        result = FileOpResult::SUCCESS;
        if(QFile::exists(tmpPath))
            QFile::remove(tmpPath);
    } else {
        result = FileOpResult::OTHER_ERROR;
        // restore dest file
        QFile::rename(tmpPath, newFilePath);
    }
}

void FileOperations::createDirectory(const QString &dirPath, FileOpResult &result) {
    QFileInfo fi(dirPath);
    if(fi.exists()) {
        result = fi.isDir() ? FileOpResult::DESTINATION_DIR_EXISTS
                            : FileOpResult::DESTINATION_FILE_EXISTS;
        return;
    }
    // Parent must already exist; mkdir() (not mkpath()) keeps this a single,
    // explicit folder creation in the current directory.
    if(QDir().mkdir(dirPath))
        result = FileOpResult::SUCCESS;
    else
        result = FileOpResult::OTHER_ERROR;
}

void FileOperations::moveToTrash(const QString &filePath, FileOpResult &result) {
    checkCanRemove(filePath, result);
    if(result != FileOpResult::SUCCESS)
        return;

    if(FileOperationsPlatform::moveToTrash(filePath))
        result = FileOpResult::SUCCESS;
    else
        result = FileOpResult::OTHER_ERROR;
    return;
}
