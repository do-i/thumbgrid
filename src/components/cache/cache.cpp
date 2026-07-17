#include "cache.h"

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
        items.insert(img->filePath(), new CacheItem(img));
        return true;
    }
    // TODO: what state returns here ??
    return true;
}

// locks & deletes the item at it, returning the iterator to the next entry
QMap<QString, CacheItem*>::iterator Cache::eraseEntry(QMap<QString, CacheItem*>::iterator it) {
    it.value()->lock();
    delete it.value();
    return items.erase(it);
}

void Cache::remove(const QString& path) {
    auto it = items.find(path);
    if(it != items.end())
        eraseEntry(it);
}

void Cache::clear() {
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
    for(auto it = items.begin(); it != items.end();) {
        if(keep.contains(it.key()))
            ++it;
        else
            it = eraseEntry(it);
    }
}

const QStringList Cache::keys() const {
    return items.keys();
}
