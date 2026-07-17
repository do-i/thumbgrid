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
            qDebug() << "XDG_DATA_HOME not yet tested";
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
            qDebug() << "Can`t detect trash folder";
        TrashPathInfo = TrashPath + "/info";
        TrashPathFiles = TrashPath + "/files";
        if( !QDir( TrashPathInfo ).exists() || !QDir( TrashPathFiles ).exists() )
            qDebug() << "Trash doesn`t look like FreeDesktop.org Trash specification";
        TrashInitialized = true;
    }
    if( TrashPath.isEmpty() || !QDir( TrashPathInfo ).exists() || !QDir( TrashPathFiles ).exists() )
        return false;
    QFileInfo original( filePath );
    if( !original.exists() ) {
        qDebug() << "File doesn`t exist, cant move to trash";
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
        qDebug() << "move to trash failed";
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
    qDebug() << "Trash in server-mode not supported";
    return false;
#endif
#endif
}
