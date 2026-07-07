#include "portablewatcher.h"
#include "directorywatcher_p.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHash>
#include <QTimer>

namespace {
    const int changeCoalesceMs = 100;

    class NoopWatcherWorker : public WatcherWorker {
    public:
        void run() override {}
    };

    struct EntryState {
        QDateTime lastModified;
        qint64 size = 0;
        bool isDir = false;

        bool operator==(const EntryState &other) const {
            return lastModified == other.lastModified &&
                   size == other.size &&
                   isDir == other.isDir;
        }
    };

    QHash<QString, EntryState> snapshotDirectory(const QString &path) {
        QHash<QString, EntryState> snapshot;
        QDir dir(path);
        QFileInfoList entries = dir.entryInfoList(QDir::AllEntries |
                                                  QDir::NoDotAndDotDot |
                                                  QDir::Hidden |
                                                  QDir::System);
        for(const QFileInfo &entry : entries) {
            snapshot.insert(entry.fileName(), EntryState{
                entry.lastModified(),
                entry.isDir() ? 0 : entry.size(),
                entry.isDir()
            });
        }
        return snapshot;
    }
}

class PortableWatcherPrivate : public DirectoryWatcherPrivate {
    Q_OBJECT
public:
    PortableWatcherPrivate(DirectoryWatcher *watcher)
        : DirectoryWatcherPrivate(watcher, new NoopWatcherWorker()),
          fsWatcher(new QFileSystemWatcher(this)),
          changeTimer(new QTimer(this))
    {
        changeTimer->setSingleShot(true);
        changeTimer->setInterval(changeCoalesceMs);
    }

    QFileSystemWatcher *fsWatcher;
    QTimer *changeTimer;
    QHash<QString, EntryState> entries;
    bool observing = false;

public slots:
    void scheduleScan() {
        changeTimer->start();
    }

    void scan() {
        Q_Q(PortableWatcher);
        QHash<QString, EntryState> next = snapshotDirectory(currentDirectory);

        QStringList removed;
        QStringList added;
        for(auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
            if(!next.contains(it.key()))
                removed << it.key();
        }
        for(auto it = next.constBegin(); it != next.constEnd(); ++it) {
            if(!entries.contains(it.key()))
                added << it.key();
        }

        if(removed.count() == 1 && added.count() == 1) {
            emit q->fileRenamed(removed.first(), added.first());
        } else {
            for(const QString &name : removed)
                emit q->fileDeleted(name);
            for(const QString &name : added)
                emit q->fileCreated(name);
        }

        for(auto it = next.constBegin(); it != next.constEnd(); ++it) {
            if(entries.contains(it.key()) && !(entries.value(it.key()) == it.value()))
                emit q->fileModified(it.key());
        }

        entries = next;
    }

private:
    Q_DECLARE_PUBLIC(PortableWatcher)
};

PortableWatcher::PortableWatcher()
    : DirectoryWatcher(new PortableWatcherPrivate(this))
{
    Q_D(PortableWatcher);
    connect(d->fsWatcher, &QFileSystemWatcher::directoryChanged,
            d, &PortableWatcherPrivate::scheduleScan);
    connect(d->changeTimer, &QTimer::timeout,
            d, &PortableWatcherPrivate::scan);
}

void PortableWatcher::setWatchPath(const QString &path) {
    Q_D(PortableWatcher);
    DirectoryWatcher::setWatchPath(path);

    if(!d->fsWatcher->directories().isEmpty())
        d->fsWatcher->removePaths(d->fsWatcher->directories());
    d->entries = snapshotDirectory(path);
    if(d->observing)
        d->fsWatcher->addPath(path);
}

void PortableWatcher::observe() {
    Q_D(PortableWatcher);
    if(d->observing)
        return;

    d->entries = snapshotDirectory(d->currentDirectory);
    d->fsWatcher->addPath(d->currentDirectory);
    d->observing = true;
    emit observingStarted();
}

void PortableWatcher::stopObserving() {
    Q_D(PortableWatcher);
    if(!d->observing)
        return;

    d->changeTimer->stop();
    if(!d->fsWatcher->directories().isEmpty())
        d->fsWatcher->removePaths(d->fsWatcher->directories());
    d->observing = false;
    emit observingStopped();
}

bool PortableWatcher::isObserving() {
    Q_D(PortableWatcher);
    return d->observing;
}

#include "portablewatcher.moc"
