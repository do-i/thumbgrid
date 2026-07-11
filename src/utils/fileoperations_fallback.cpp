#include "fileoperations_platform.h"

#include <QFile>
#include <QFileInfo>
#include <QtGlobal>

bool FileOperationsPlatform::canRemoveSource(const QFileInfo &) {
    return true;
}

bool FileOperationsPlatform::canReplaceExistingDestination(const QFileInfo &) {
    return true;
}

bool FileOperationsPlatform::isWritableParentDirectory(const QFileInfo &directory) {
    return directory.exists() && directory.isDir() && directory.isWritable() && directory.isExecutable();
}

bool FileOperationsPlatform::moveToTrash(const QString &filePath) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QFile::moveToTrash(filePath);
#else
    Q_UNUSED(filePath)
    return false;
#endif
}
