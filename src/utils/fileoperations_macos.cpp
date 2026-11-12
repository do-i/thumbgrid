#include "fileoperations_platform.h"

#include <QFile>

#include <cerrno>
#include <stdio.h>
#include <sys/stat.h>

FileOperationsPlatform::RenameResult FileOperationsPlatform::renameNoReplace(const QString &srcPath, const QString &dstPath) {
    // RENAME_EXCL has atomic no-replace semantics, including for symlink destinations.
    if(::renamex_np(QFile::encodeName(srcPath).constData(), QFile::encodeName(dstPath).constData(), RENAME_EXCL) == 0)
        return RenameResult::Success;
    switch(errno) {
    case EEXIST:
    case ENOTEMPTY:
        return RenameResult::DestinationExists;
    case EXDEV:
        return RenameResult::CrossDevice;
    case ENOSYS:
    case ENOTSUP:
    case EINVAL:
        return RenameResult::Unsupported;
    default:
        return RenameResult::Error;
    }
}

FileOperationsPlatform::FilesystemRelation FileOperationsPlatform::filesystemRelation(const QString &path, const QString &dirPath) {
    struct stat src {}, dst {};
    if(::lstat(QFile::encodeName(path).constData(), &src) != 0 ||
       ::stat(QFile::encodeName(dirPath).constData(), &dst) != 0)
        return FilesystemRelation::Unknown;
    return src.st_dev == dst.st_dev ? FilesystemRelation::Same : FilesystemRelation::Different;
}
