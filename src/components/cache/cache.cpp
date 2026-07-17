#include "cache.h"
#include "utils/logging.h"

Cache::Cache() {
}

bool Cache::contains(const QString& path) const {
    return items.contains(path);
}

bool Cache::insert(const std::shared_ptr<Image>& img) {
    if(img) {
        auto it = items.find(img->filePath());
        if(it != items.end()) {
            return false;
        }
        items.insert(img->filePath(), std::make_shared<CacheItem>(img));
        qCDebug(logCache) << "insert" << img->filePath();
        return true;
    }
    return false;
}

// locks & erases the item at it (destroying it once no reserver holds it),
// returning the iterator to the next entry
QHash<QString, std::shared_ptr<CacheItem>>::iterator Cache::eraseEntry(QHash<QString, std::shared_ptr<CacheItem>>::iterator it) {
    it.value()->lock();
    return items.erase(it);
}

void Cache::remove(const QString& path) {
    auto it = items.find(path);
    if(it != items.end()) {
        qCDebug(logCache) << "remove" << path;
        eraseEntry(it);
    }
}

void Cache::clear() {
    qCDebug(logCache) << "clear" << items.count() << "items";
    for(auto it = items.begin(); it != items.end();)
        it = eraseEntry(it);
}

std::shared_ptr<Image> Cache::get(const QString& path) {
    auto it = items.find(path);
    if(it != items.end())
        return it.value()->getContents();
    return nullptr;
}

bool Cache::reserve(const QString& path) {
    auto it = items.find(path);
    if(it != items.end()) {
        it.value()->lock();
        return true;
    }
    return false;
}

bool Cache::release(const QString& path) {
    auto it = items.find(path);
    if(it != items.end()) {
        it.value()->unlock();
        return true;
    }
    return false;
}

// removes all items except the ones in pathList
void Cache::trimTo(const QStringList& pathList) {
    const QSet<QString> keep(pathList.begin(), pathList.end());
    int evicted = 0;
    for(auto it = items.begin(); it != items.end();) {
        if(keep.contains(it.key())) {
            ++it;
        } else {
            it = eraseEntry(it);
            evicted++;
        }
    }
    if(evicted)
        qCDebug(logCache) << "trimTo evicted" << evicted << "items," << items.count() << "kept";
}
