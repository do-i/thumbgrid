#include "dummywatcher.h"
#include "directorywatcher_p.h"
#include <QDebug>
#include "utils/logging.h"

#define TAG         "[DummyWatcher]"
#define MESSAGE     "Directory watcher isn't yet implemented for your operating system"

class DummyWatcherWorker : public WatcherWorker {
  public:
    DummyWatcherWorker() {}
    void run() override {
        qCDebug(logDirManager) << TAG << MESSAGE;
    }
};

class DummyWatcherPrivate : public DirectoryWatcherPrivate {
  public:
    DummyWatcherPrivate(DirectoryWatcher* watcher) : DirectoryWatcherPrivate(watcher, new DummyWatcherWorker()) {}
};

DummyWatcher::DummyWatcher() : DirectoryWatcher(new DummyWatcherPrivate(this))
{
    qCDebug(logDirManager) << TAG << MESSAGE;
}
