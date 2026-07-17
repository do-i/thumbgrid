#include "safesave.h"

#include <QDebug>
#include <QFile>
#include <QCryptographicHash>

namespace SafeSave {

bool withBackup(const QString &destPath, const QString &restorePath, const std::function<bool()> &doSave) {
    const QString tmpPath = destPath + "_" + QString(QCryptographicHash::hash(destPath.toUtf8(), QCryptographicHash::Md5).toHex());
    const bool originalExists = QFile::exists(destPath);
    bool backupExists = false;

    if(originalExists) {
        QFile::remove(tmpPath);
        if(!QFile::copy(destPath, tmpPath)) {
            qDebug() << "SafeSave::withBackup() - could not create file backup for" << destPath;
            return false;
        }
        backupExists = true;
    }

    const bool success = doSave();

    if(backupExists) {
        if(success) {
            // everything ok - remove the backup
            QFile::remove(tmpPath);
        } else if(originalExists) {
            // revert on fail
            QFile::remove(restorePath);
            QFile::copy(tmpPath, restorePath);
            QFile::remove(tmpPath);
        }
    }
    return success;
}

}
