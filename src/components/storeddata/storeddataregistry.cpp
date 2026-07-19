#include "storeddataregistry.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>

namespace {

QString trStored(const char *text, int n = -1) {
    return QCoreApplication::translate("StoredData", text, nullptr, n);
}

QString itemCount(int count) {
    return trStored("%n item(s)", count);
}

// The thumbnail cache holds only id-named PNGs; anything else in the
// directory (symlinks included) is not ours and must be left alone.
void forEachThumbnailFile(const std::function<void(const QFileInfo &)> &fn) {
    QDirIterator it(settings->thumbnailCacheDir(), {"*.png"},
                    QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    while(it.hasNext()) {
        it.next();
        fn(it.fileInfo());
    }
}

QString thumbnailDetail() {
    int count = 0;
    qint64 bytes = 0;
    forEachThumbnailFile([&](const QFileInfo &info) {
        count++;
        bytes += info.size();
    });
    if(!count)
        return trStored("empty");
    return itemCount(count) + QStringLiteral(" · ") + QLocale().formattedDataSize(bytes);
}

QString hashIndexDetail() {
    const QFileInfo info(settings->duplicateHashCachePath());
    if(!info.exists() || info.size() == 0)
        return trStored("empty");
    return QLocale().formattedDataSize(info.size());
}

} // namespace

namespace StoredData {

QList<StoredDataStore> stores() {
    return {
        {QStringLiteral("savedPaths"),
         trStored("Last session paths"),
         trStored("Folders and files reopened on startup"),
         settings->settingsFilePath(),
         [] { return settings->savedPathsStored() ? itemCount(settings->savedPaths().count())
                                                  : trStored("empty"); },
         [] { settings->clearSavedPaths(); }},
        {QStringLiteral("bookmarks"),
         trStored("Bookmarks"),
         trStored("Bookmarked folders in the places panel"),
         settings->settingsFilePath(),
         [] { const int n = settings->bookmarks().count();
              return n ? itemCount(n) : trStored("empty"); },
         [] { settings->clearBookmarks(); }},
        {QStringLiteral("dupeHistory"),
         trStored("Duplicate finder folder history"),
         trStored("Folders last searched for duplicates"),
         settings->settingsFilePath(),
         [] { const int n = settings->duplicateFinderTargets().count();
              return n ? itemCount(n) : trStored("empty"); },
         [] { settings->clearDuplicateFinderTargets(); }},
        {QStringLiteral("dupeIndex"),
         trStored("Duplicate search index"),
         trStored("Cached image hashes, stored with full file paths"),
         settings->duplicateHashCachePath(),
         [] { return hashIndexDetail(); },
         [] { QFile::remove(settings->duplicateHashCachePath()); }},
        {QStringLiteral("thumbnails"),
         trStored("Thumbnail cache"),
         trStored("Preview images generated from your files"),
         settings->thumbnailCacheDir(),
         [] { return thumbnailDetail(); },
         [] { forEachThumbnailFile([](const QFileInfo &info) {
                  QFile::remove(info.absoluteFilePath());
              }); }},
    };
}

void clearStoresMarkedForExit() {
    const QStringList ids = settings->clearOnExitStores();
    if(ids.isEmpty())
        return;
    const QList<StoredDataStore> all = stores();
    for(const StoredDataStore &store : all)
        if(ids.contains(store.id))
            store.clear();
}

}
