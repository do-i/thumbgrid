#include "hashcache.h"

#include <QDataStream>
#include <QFile>
#include <QSaveFile>

namespace {
const quint32 CACHE_MAGIC = 0x54474448; // "TGDH"
const quint32 CACHE_VERSION = 1;
}

HashCache::HashCache(const QString &filePath) : mFilePath(filePath) {
}

bool HashCache::lookup(const QString &path, qint64 mtime, qint64 fileSize,
                       quint64 &hash, QSize &dims) const {
    QMutexLocker lock(&mMutex);
    auto it = mEntries.constFind(path);
    if(it == mEntries.constEnd() || it->mtime != mtime || it->fileSize != fileSize)
        return false;
    hash = it->hash;
    dims = it->dims;
    return true;
}

void HashCache::insert(const QString &path, qint64 mtime, qint64 fileSize,
                       quint64 hash, const QSize &dims) {
    QMutexLocker lock(&mMutex);
    mEntries.insert(path, Entry{mtime, fileSize, hash, dims});
    mDirty = true;
}

void HashCache::load() {
    QMutexLocker lock(&mMutex);
    QFile file(mFilePath);
    if(!file.open(QIODevice::ReadOnly))
        return;
    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0, version = 0;
    in >> magic >> version;
    if(magic != CACHE_MAGIC || version != CACHE_VERSION)
        return;
    QHash<QString, Entry> entries;
    qint32 count = 0;
    in >> count;
    for(qint32 i = 0; i < count && in.status() == QDataStream::Ok; i++) {
        QString path;
        Entry e;
        in >> path >> e.mtime >> e.fileSize >> e.hash >> e.dims;
        entries.insert(path, e);
    }
    if(in.status() == QDataStream::Ok) {
        mEntries = entries;
        mDirty = false;
    }
}

void HashCache::save() {
    QMutexLocker lock(&mMutex);
    if(!mDirty)
        return;
    QSaveFile file(mFilePath);
    if(!file.open(QIODevice::WriteOnly))
        return;
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_0);
    out << CACHE_MAGIC << CACHE_VERSION << qint32(mEntries.count());
    for(auto it = mEntries.constBegin(); it != mEntries.constEnd(); ++it)
        out << it.key() << it->mtime << it->fileSize << it->hash << it->dims;
    if(file.commit())
        mDirty = false;
}

int HashCache::count() const {
    QMutexLocker lock(&mMutex);
    return mEntries.count();
}
