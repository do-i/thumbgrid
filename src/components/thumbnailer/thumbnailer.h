#pragma once

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
    ~Thumbnailer();
    static std::shared_ptr<Thumbnail> getThumbnail(QString filePath, int size);
    void clearTasks();
    void keepOnlyQueuedTasks(const QList<QPair<QString, int>> &tasks);
    void waitForDone();

public slots:
    void getThumbnailAsync(QString path, int size, bool crop, bool force);
    void getDirThumbnailAsync(QString path, int size, bool previewFit, bool crop, bool force);

private:
    ThumbnailCache *cache;
    QThreadPool *pool;
    void startThumbnailerThread(QString filePath, int size, bool crop, bool force);
    void startDirThumbnailerThread(QString dirPath, int size, bool previewFit, bool crop, bool force);
    QImage dirIconBase(int size);
    void startTask(ThumbnailerRunnable *runnable);
    void removeQueuedTask(const QString &path, int size);
    QString taskKey(const QString &path, int size) const;
    QMultiMap<QString, int> runningTasks;
    QHash<QString, ThumbnailerRunnable*> queuedTasks;
    // cached recolored+scaled folder icon, keyed by size + scheme color
    QImage cachedIconBase;
    int cachedIconSize = -1;
    QString cachedIconColor;

private slots:
    void onTaskEnd(std::shared_ptr<Thumbnail> thumbnail, QString filePath);
    void onDirTaskEnd(std::shared_ptr<Thumbnail> thumbnail, QString dirPath);

signals:
    void thumbnailReady(std::shared_ptr<Thumbnail> thumbnail, QString filePath);
    void dirThumbnailReady(std::shared_ptr<Thumbnail> thumbnail, QString dirPath);
};
