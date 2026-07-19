#pragma once

#include <QHash>
#include <QMutex>
#include <QSize>
#include <QString>

// Persistent path+mtime+size -> pHash store (docs/003 §2.3) so repeat
// searches skip re-decoding unchanged files. Thread-safe.
class HashCache {
public:
    explicit HashCache(const QString &filePath);

    bool lookup(const QString &path, qint64 mtime, qint64 fileSize,
                quint64 &hash, QSize &dims) const;
    void insert(const QString &path, qint64 mtime, qint64 fileSize,
                quint64 hash, const QSize &dims);
    void load();
    void save();
    int count() const;

private:
    struct Entry {
        qint64 mtime = 0;
        qint64 fileSize = 0;
        quint64 hash = 0;
        QSize dims;
    };
    QString mFilePath;
    QHash<QString, Entry> mEntries;
    mutable QMutex mMutex;
    bool mDirty = false;
};
