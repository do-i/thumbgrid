#include "thumbnailer.h"

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
    pool->clear();
    pool->waitForDone();
}

void Thumbnailer::waitForDone() {
    pool->waitForDone();
}

void Thumbnailer::clearTasks() {
    pool->clear();
}

std::shared_ptr<Thumbnail> Thumbnailer::getThumbnail(QString filePath, int size) {
    return ThumbnailerRunnable::generate(nullptr, filePath, size, false, false);
}

void Thumbnailer::getThumbnailAsync(QString path, int size, bool crop, bool force) {
    if(!runningTasks.contains(path, size))
        startThumbnailerThread(path, size, crop, force);
}

void Thumbnailer::getDirThumbnailAsync(QString path, int size, bool previewFit, bool crop, bool force) {
    if(!runningTasks.contains(path, size))
        startDirThumbnailerThread(path, size, previewFit, crop, force);
}

void Thumbnailer::startThumbnailerThread(QString filePath, int size, bool crop, bool force) {
    auto runnable = new ThumbnailerRunnable(settings->useThumbnailCache() ? cache : nullptr, filePath, size, crop, force);
    connect(runnable, &ThumbnailerRunnable::taskStart, this, &Thumbnailer::onTaskStart);
    connect(runnable, &ThumbnailerRunnable::taskEnd, this, &Thumbnailer::onTaskEnd);
    runnable->setAutoDelete(true);
    pool->start(runnable);
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
    connect(runnable, &ThumbnailerRunnable::taskStart, this, &Thumbnailer::onTaskStart);
    connect(runnable, &ThumbnailerRunnable::dirTaskEnd, this, &Thumbnailer::onDirTaskEnd);
    runnable->setAutoDelete(true);
    pool->start(runnable);
}

void Thumbnailer::onTaskStart(QString filePath, int size) {
    runningTasks.insert(filePath, size);
}

void Thumbnailer::onTaskEnd(std::shared_ptr<Thumbnail> thumbnail, QString filePath) {
    runningTasks.remove(filePath, thumbnail->size());
    emit thumbnailReady(thumbnail, filePath);
}

void Thumbnailer::onDirTaskEnd(std::shared_ptr<Thumbnail> thumbnail, QString dirPath) {
    runningTasks.remove(dirPath, thumbnail->size());
    emit dirThumbnailReady(thumbnail, dirPath);
}
