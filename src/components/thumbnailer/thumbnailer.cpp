#include "thumbnailer.h"

#include <QDebug>

namespace {
    const int thumbnailerShutdownWaitMs = 3000;
}

Thumbnailer::Thumbnailer() {
    cache = new ThumbnailCache();
    pool = new QThreadPool(this);
    int threads = settings->thumbnailerThreadCount();
    int globalThreads = QThreadPool::globalInstance()->maxThreadCount();
    if(threads > globalThreads)
        threads = globalThreads;
    pool->setMaxThreadCount(threads);
}

Thumbnailer::~Thumbnailer() {
    clearTasks();
    if(!pool->waitForDone(thumbnailerShutdownWaitMs)) {
        qWarning() << "Thumbnailer: timed out waiting for thumbnail workers; leaking remaining tasks to avoid hanging exit";
        // Running tasks may still touch the pool and cache; intentionally leave
        // both alive on this rare timeout path instead of blocking shutdown.
        pool->setParent(nullptr);
        queuedTasks.clear();
        return;
    }

    qDeleteAll(queuedTasks);
    queuedTasks.clear();
    delete cache;
}

void Thumbnailer::waitForDone() {
    pool->waitForDone();
}

void Thumbnailer::clearTasks() {
    keepOnlyQueuedTasks({});
}

void Thumbnailer::keepOnlyQueuedTasks(const QList<QPair<QString, int>> &tasks) {
    QSet<QString> keep;
    for(const auto &task : tasks)
        keep.insert(taskKey(task.first, task.second));

    auto it = queuedTasks.begin();
    while(it != queuedTasks.end()) {
        ThumbnailerRunnable *runnable = it.value();
        if(!keep.contains(it.key()) && pool->tryTake(runnable)) {
            runningTasks.remove(runnable->taskPath(), runnable->taskSize());
            delete runnable;
            it = queuedTasks.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<Thumbnail> Thumbnailer::getThumbnail(QString filePath, int size) {
    return ThumbnailerRunnable::generate(nullptr, filePath, size, false, false);
}

void Thumbnailer::getThumbnailAsync(QString path, int size, bool crop, bool force) {
    // Track at enqueue time: a job can sit queued before its worker starts, and
    // repeated requests for the same visible cell must not queue duplicates.
    if(!runningTasks.contains(path, size)) {
        runningTasks.insert(path, size);
        startThumbnailerThread(path, size, crop, force);
    }
}

void Thumbnailer::getDirThumbnailAsync(QString path, int size, bool previewFit, bool crop, bool force) {
    if(!runningTasks.contains(path, size)) {
        runningTasks.insert(path, size);
        startDirThumbnailerThread(path, size, previewFit, crop, force);
    }
}

void Thumbnailer::startThumbnailerThread(QString filePath, int size, bool crop, bool force) {
    auto runnable = new ThumbnailerRunnable(settings->useThumbnailCache() ? cache : nullptr, filePath, size, crop, force);
    connect(runnable, &ThumbnailerRunnable::taskEnd, this, &Thumbnailer::onTaskEnd);
    startTask(runnable);
}

// Builds the recolored, pre-scaled folder icon on the GUI thread (QPixmap and
// ImageLib::recolor are not safe off-thread); the worker only composites onto it.
QImage Thumbnailer::dirIconBase(int size) {
    QString color = settings->colorScheme().icons.name();
    if(size == cachedIconSize && color == cachedIconColor && !cachedIconBase.isNull())
        return cachedIconBase;

    QPixmap source(":/res/icons/common/other/folder.png");
    if(source.isNull())
        source = QPixmap(":/res/icons/common/other/folder96.png");

    QSize pixmapSize(qRound(size * 1.10f),
                     qRound(size * 1.10f * source.height() / static_cast<qreal>(source.width())));
    QPixmap scaled = source.scaled(pixmapSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ImageLib::recolor(scaled, settings->colorScheme().icons);

    cachedIconBase = scaled.toImage();
    cachedIconSize = size;
    cachedIconColor = color;
    return cachedIconBase;
}

void Thumbnailer::startDirThumbnailerThread(QString dirPath, int size, bool previewFit, bool crop, bool force) {
    auto runnable = new ThumbnailerRunnable(settings->useThumbnailCache() ? cache : nullptr, dirPath, size, crop, force,
                                            previewFit, settings->showHiddenFiles(), dirIconBase(size),
                                            settings->colorScheme().icons.name());
    connect(runnable, &ThumbnailerRunnable::dirTaskEnd, this, &Thumbnailer::onDirTaskEnd);
    startTask(runnable);
}

void Thumbnailer::startTask(ThumbnailerRunnable *runnable) {
    runnable->setAutoDelete(false);
    queuedTasks.insert(taskKey(runnable->taskPath(), runnable->taskSize()), runnable);
    pool->start(runnable);
}

void Thumbnailer::removeQueuedTask(const QString &path, int size) {
    ThumbnailerRunnable *runnable = queuedTasks.take(taskKey(path, size));
    delete runnable;
}

QString Thumbnailer::taskKey(const QString &path, int size) const {
    return QString::number(size) + QChar(0x1f) + path;
}

void Thumbnailer::onTaskEnd(std::shared_ptr<Thumbnail> thumbnail, QString filePath) {
    runningTasks.remove(filePath, thumbnail->size());
    removeQueuedTask(filePath, thumbnail->size());
    emit thumbnailReady(thumbnail, filePath);
}

void Thumbnailer::onDirTaskEnd(std::shared_ptr<Thumbnail> thumbnail, QString dirPath) {
    runningTasks.remove(dirPath, thumbnail->size());
    removeQueuedTask(dirPath, thumbnail->size());
    emit dirThumbnailReady(thumbnail, dirPath);
}
