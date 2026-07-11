#include "fileoperations_platform.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
#include <cstdlib>
#include <cstring>
#include <windows.h>
#endif

bool FileOperationsPlatform::canRemoveSource(const QFileInfo &file) {
    return file.isWritable();
}

bool FileOperationsPlatform::canReplaceExistingDestination(const QFileInfo &file) {
    return file.isWritable();
}

bool FileOperationsPlatform::isWritableParentDirectory(const QFileInfo &directory) {
    return directory.exists() && directory.isDir() && directory.isWritable();
}

bool FileOperationsPlatform::moveToTrash(const QString &filePath) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QFile::moveToTrash(filePath);
#else
    QFileInfo fileinfo( filePath );
    if( !fileinfo.exists() )
        return false;
    WCHAR* from = (WCHAR*) calloc((size_t)fileinfo.absoluteFilePath().length() + 2, sizeof(WCHAR));
    fileinfo.absoluteFilePath().toWCharArray(from);
    SHFILEOPSTRUCTW fileop;
    memset( &fileop, 0, sizeof( fileop ) );
    fileop.wFunc = FO_DELETE;
    fileop.pFrom = from;
    fileop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    int rv = SHFileOperationW( &fileop );
    free(from);
    if( 0 != rv ){
        qDebug() << rv << QString::number( rv ).toInt( nullptr, 8 );
        qDebug() << "move to trash failed";
        return false;
    }
    return true;
#endif
}
