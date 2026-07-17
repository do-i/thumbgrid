#pragma once

#include <QCache>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QThreadPool>
#include "components/thumbnailer/thumbnailerrunnable.h"
#include "components/cache/thumbnailcache.h"
#include "settings.h"

class Thumbnailer : public QObject
{
    Q_OBJECT
public:
    explicit Thumbnailer();
    ~Thumbnailer() override;
    static std::shared_ptr<Thumbnail> getThumbnail(QString filePath, int size);
    void clearTasks();
    void keepOnlyQueuedTasks(const QList<QPair<QString, int>> &tasks);
    void waitForDone();

public slots:
    void getThumbnailAsync(const QString& path, int size, bool crop, bool force);
    void getDirThumbnailAsync(const QString& path, int size, bool previewFit, bool crop, bool force);

private:
    ThumbnailCache *cache;
    QThreadPool *pool;
    void startThumbnailerThread(QString filePath, int size, bool crop, bool force);
    void startDirThumbnailerThread(QString dirPath, int size, bool previewFit, bool crop, bool force);
    QImage dirIconBase(int size);
    void startTask(ThumbnailerRunnable *runnable);
    QString taskKey(const QString &path, int size) const;
    QMultiMap<QString, int> runningTasks;
    QHash<QString, ThumbnailerRunnable*> queuedTasks;
    // cached recolored+scaled folder icon, keyed by size + scheme color
    QImage cachedIconBase;
    int cachedIconSize = -1;
    QString cachedIconColor;

    // In-memory LRU cache on top of the disk cache: repeated navigation must
    // not re-read every visible thumbnail from disk. Entries are validated
    // against the source mtime, so an entry never outlives a file change.
    struct MemCacheEntry {
        std::shared_ptr<Thumbnail> thumbnail;
        QDateTime lastModified;
    };
    QCache<QString, MemCacheEntry> memCache;
    static QString memKey(const QString &path, int size, bool crop);
    static QString dirMemKey(const QString &path, int size, bool previewFit, const QString &colorId);
    std::shared_ptr<Thumbnail> memCacheLookup(const QString &key, const QString &path);
    void memCacheInsert(const QString &key, const std::shared_ptr<Thumbnail>& thumbnail, const QString &path);
    void applyMemCacheLimit();
    // File-type icons bake theme colors into their pixmap but (unlike image
    // thumbnails) are keyed only by path/size, so a theme change must drop them
    // from the mem cache or the stale-colored icon keeps being served.
    void onSettingsChanged();
    QString mThumbColorSignature;

private slots:
    void onTaskEnd(const std::shared_ptr<Thumbnail>& thumbnail, const QString& filePath);
    void onDirTaskEnd(const std::shared_ptr<Thumbnail>& thumbnail, const QString& dirPath);

signals:
    void thumbnailReady(const std::shared_ptr<Thumbnail>& thumbnail, const QString& filePath);
    void dirThumbnailReady(const std::shared_ptr<Thumbnail>& thumbnail, const QString& dirPath);
};
