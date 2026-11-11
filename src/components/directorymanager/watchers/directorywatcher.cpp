#include "directorywatcher_p.h"

#define TAG         "[DirectoryWatcher]"

DirectoryWatcherPrivate::DirectoryWatcherPrivate(DirectoryWatcher* qq, WatcherWorker* w) :
    q_ptr(qq),
    worker(w),
    workerThread(new QThread())
{
}

DirectoryWatcher::~DirectoryWatcher() {
    stopWorker();
    delete d_ptr;
    d_ptr = nullptr;
}

void DirectoryWatcher::setWatchPath(const QString& path) {
    Q_D(DirectoryWatcher);
    d->currentDirectory = path;
}

QString DirectoryWatcher::watchPath() const {
    Q_D(const DirectoryWatcher);
    return d->currentDirectory;
}

void DirectoryWatcher::observe()
{
    Q_D(DirectoryWatcher);
    if(!isObserving()) {
        // Reuse worker instance
        d->worker->setRunning(true);
        d->workerThread->start();
    }
}

void DirectoryWatcher::stopObserving()
{
    stopWorker();
}

// Joins the worker thread: one left running emits into a watcher that is going away, or into a process
// that is already exiting.
void DirectoryWatcher::stopWorker()
{
    Q_D(DirectoryWatcher);
    if(!d->worker || !d->workerThread->isRunning())
        return;
    d->worker->setRunning(false);
    d->worker->wake();
    d->workerThread->quit();
    d->workerThread->wait();
}

bool DirectoryWatcher::isObserving()
{
    Q_D(DirectoryWatcher);
    return d->workerThread->isRunning();
}

DirectoryWatcher::DirectoryWatcher(DirectoryWatcherPrivate* ptr) {
    d_ptr = ptr;
}
