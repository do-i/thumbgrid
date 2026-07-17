#pragma once

#include <QDebug>
#include <QMap>
#include <QSet>
#include <QSemaphore>
#include <QMutexLocker>
#include "sourcecontainers/image.h"
#include "components/cache/cacheitem.h"
#include "utils/imagefactory.h"

class Cache {
public:
    explicit Cache();
    bool contains(const QString& path) const;
    void remove(const QString& path);
    void clear();

    bool insert(const std::shared_ptr<Image>& img);
    void trimTo(const QStringList& list);

    std::shared_ptr<Image> get(const QString& path);
    bool release(const QString& path);
    bool reserve(const QString& path);
    const QStringList keys() const;

private:
    QMap<QString, std::shared_ptr<CacheItem>> items;
    // locks & erases the entry at it, returning the iterator to the next entry
    QMap<QString, std::shared_ptr<CacheItem>>::iterator eraseEntry(QMap<QString, std::shared_ptr<CacheItem>>::iterator it);
};
