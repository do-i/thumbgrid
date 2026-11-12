#include "fileoperations_platform.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QtGlobal>

#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include "utils/logging.h"

FileOperationsPlatform::RenameResult FileOperationsPlatform::renameNoReplace(const QString &srcPath, const QString &dstPath) {
    if(::renameat2(AT_FDCWD, QFile::encodeName(srcPath).constData(),
                   AT_FDCWD, QFile::encodeName(dstPath).constData(), RENAME_NOREPLACE) == 0)
        return RenameResult::Success;
    switch(errno) {
    case EEXIST:
    case ENOTEMPTY:
        return RenameResult::DestinationExists;
    case EXDEV:
        return RenameResult::CrossDevice;
    case ENOSYS:
    case EINVAL: // A kernel/filesystem without support for RENAME_NOREPLACE.
    case EOPNOTSUPP:
        return RenameResult::Unsupported;
    default:
        return RenameResult::Error;
    }
}

FileOperationsPlatform::FilesystemRelation FileOperationsPlatform::filesystemRelation(const QString &path, const QString &dirPath) {
    struct statx src {}, dst {};
    if(::statx(AT_FDCWD, QFile::encodeName(path).constData(), AT_SYMLINK_NOFOLLOW, STATX_MNT_ID, &src) != 0 ||
       ::statx(AT_FDCWD, QFile::encodeName(dirPath).constData(), 0, STATX_MNT_ID, &dst) != 0)
        return FilesystemRelation::Unknown;
    if(src.stx_dev_major != dst.stx_dev_major || src.stx_dev_minor != dst.stx_dev_minor)
        return FilesystemRelation::Different;
    // A bind mount keeps the device but rename still fails with EXDEV across its boundary.
    if((src.stx_mask & STATX_MNT_ID) && (dst.stx_mask & STATX_MNT_ID))
        return src.stx_mnt_id == dst.stx_mnt_id ? FilesystemRelation::Same : FilesystemRelation::Different;
    return FilesystemRelation::Unknown;
}

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
#ifdef QT_GUI_LIB
    bool TrashInitialized = false;
    QString TrashPath;
    QString TrashPathInfo;
    QString TrashPathFiles;
    if(!TrashInitialized) {
        QStringList paths;
        const char* xdg_data_home = getenv( "XDG_DATA_HOME" );
        if(xdg_data_home) {
            qCDebug(logCore) << "XDG_DATA_HOME not yet tested";
            QString xdgTrash( xdg_data_home );
            paths.append(xdgTrash + "/Trash");
        }
        QString home = QStandardPaths::writableLocation( QStandardPaths::HomeLocation );
        paths.append( home + "/.local/share/Trash" );
        paths.append( home + "/.trash" );
        for( const auto &path : std::as_const(paths) ){
            if( TrashPath.isEmpty() ){
                QDir dir( path );
                if( dir.exists() ){
                    TrashPath = path;
                }
            }
        }
        if( TrashPath.isEmpty() )
            qCWarning(logCore) << "Can`t detect trash folder";
        TrashPathInfo = TrashPath + "/info";
        TrashPathFiles = TrashPath + "/files";
        if( !QDir( TrashPathInfo ).exists() || !QDir( TrashPathFiles ).exists() )
            qCWarning(logCore) << "Trash doesn`t look like FreeDesktop.org Trash specification";
        TrashInitialized = true;
    }
    if( TrashPath.isEmpty() || !QDir( TrashPathInfo ).exists() || !QDir( TrashPathFiles ).exists() )
        return false;
    QFileInfo original( filePath );
    if( !original.exists() ) {
        qCWarning(logCore) << "File doesn`t exist, cant move to trash";
        return false;
    }
    QString info;
    info += "[Trash Info]\nPath=";
    info += original.absoluteFilePath();
    info += "\nDeletionDate=";
    info += QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss");
    info += "\n";
    QString trashname = original.fileName();
    QString infopath = TrashPathInfo + "/" + trashname + ".trashinfo";
    QString filepath = TrashPathFiles + "/" + trashname;
    int nr = 1;
    while( QFileInfo( infopath ).exists() || QFileInfo( filepath ).exists() ){
        nr++;
        trashname = original.baseName() + "." + QString::number( nr );
        if( !original.completeSuffix().isEmpty() ){
            trashname += QString( "." ) + original.completeSuffix();
        }
        infopath = TrashPathInfo + "/" + trashname + ".trashinfo";
        filepath = TrashPathFiles + "/" + trashname;
    }
    QDir dir;
    if( !dir.rename( original.absoluteFilePath(), filepath ) ){
        qCWarning(logCore) << "move to trash failed";
        return false;
    }
    QFile infoFile(infopath);
    if( !infoFile.open(QIODevice::WriteOnly | QIODevice::Text) ) {
        QFile::rename(filepath, original.absoluteFilePath());
        return false;
    }
    QTextStream out(&infoFile);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(false);
    out << info;
    infoFile.close();
    return true;
#else
    Q_UNUSED(filePath)
    qCWarning(logCore) << "Trash in server-mode not supported";
    return false;
#endif
#endif
}
