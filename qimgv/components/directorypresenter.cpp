#include "directorypresenter.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

DirectoryPresenter::DirectoryPresenter(QObject *parent) : QObject(parent), mShowDirs(false) {
    connect(&thumbnailer, &Thumbnailer::thumbnailReady, this, &DirectoryPresenter::onThumbnailReady);
}

void DirectoryPresenter::unsetModel() {
    disconnect(model.get(), &DirectoryModel::fileRemoved,  this, &DirectoryPresenter::onFileRemoved);
    disconnect(model.get(), &DirectoryModel::fileAdded,    this, &DirectoryPresenter::onFileAdded);
    disconnect(model.get(), &DirectoryModel::fileRenamed,  this, &DirectoryPresenter::onFileRenamed);
    disconnect(model.get(), &DirectoryModel::fileModified, this, &DirectoryPresenter::onFileModified);
    disconnect(model.get(), &DirectoryModel::dirRemoved,   this, &DirectoryPresenter::onDirRemoved);
    disconnect(model.get(), &DirectoryModel::dirAdded,     this, &DirectoryPresenter::onDirAdded);
    disconnect(model.get(), &DirectoryModel::dirRenamed,   this, &DirectoryPresenter::onDirRenamed);
    model = nullptr;
    // also empty view?
}

void DirectoryPresenter::setView(std::shared_ptr<IDirectoryView> _view) {
    if(view)
        return;
    view = _view;
    if(model)
        view->populate(parentOffset() + (mShowDirs ? model->totalCount() : model->fileCount()));
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(itemActivated(int)),
            this, SLOT(onItemActivated(int)));
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(thumbnailsRequested(QList<int>, int, bool, bool)),
            this, SLOT(generateThumbnails(QList<int>, int, bool, bool)));
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(draggedOut()),
            this, SLOT(onDraggedOut()));
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(draggedOver(int)),
            this, SLOT(onDraggedOver(int)));
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(droppedInto(const QMimeData*,QObject*,int)),
            this, SLOT(onDroppedInto(const QMimeData*,QObject*,int)));
}

void DirectoryPresenter::setModel(std::shared_ptr<DirectoryModel> newModel) {
    if(model)
        unsetModel();
    if(!newModel)
        return;
    model = newModel;
    populateView();

    // filesystem changes
    connect(model.get(), &DirectoryModel::fileRemoved,  this, &DirectoryPresenter::onFileRemoved);
    connect(model.get(), &DirectoryModel::fileAdded,    this, &DirectoryPresenter::onFileAdded);
    connect(model.get(), &DirectoryModel::fileRenamed,  this, &DirectoryPresenter::onFileRenamed);
    connect(model.get(), &DirectoryModel::fileModified, this, &DirectoryPresenter::onFileModified);
    connect(model.get(), &DirectoryModel::dirRemoved,   this, &DirectoryPresenter::onDirRemoved);
    connect(model.get(), &DirectoryModel::dirAdded,     this, &DirectoryPresenter::onDirAdded);
    connect(model.get(), &DirectoryModel::dirRenamed,   this, &DirectoryPresenter::onDirRenamed);
}

void DirectoryPresenter::reloadModel() {
    populateView();
}

void DirectoryPresenter::populateView() {
    if(!model || !view)
        return;
    view->populate(parentOffset() + (mShowDirs ? model->totalCount() : model->fileCount()));
    selectAndFocus(0);
}

void DirectoryPresenter::disconnectView() {
   // todo
}

//------------------------------------------------------------------------------

void DirectoryPresenter::onFileRemoved(QString filePath, int index) {
    Q_UNUSED(filePath)
    if(!view)
        return;
    view->removeItem(parentOffset() + (mShowDirs ? index + model->dirCount() : index));
}

void DirectoryPresenter::onFileRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo) {
    Q_UNUSED(fromPath)
    Q_UNUSED(toPath)
    if(!view)
        return;
    if(mShowDirs) {
        indexFrom += parentOffset() + model->dirCount();
        indexTo += parentOffset() + model->dirCount();
    } else {
        indexFrom += parentOffset();
        indexTo += parentOffset();
    }
    auto oldSelection = view->selection();
    view->removeItem(indexFrom);
    view->insertItem(indexTo);
    // re-select if needed
    if(oldSelection.contains(indexFrom)) {
        if(oldSelection.count() == 1) {
            view->select(indexTo);
            view->focusOn(indexTo);
        } else if(oldSelection.count() > 1) {
            view->select(view->selection() << indexTo);
        }
    }
}

void DirectoryPresenter::onFileAdded(QString filePath) {
    if(!view)
        return;
    int index = model->indexOfFile(filePath);
    view->insertItem(parentOffset() + (mShowDirs ? model->dirCount() + index : index));
}

void DirectoryPresenter::onFileModified(QString filePath) {
    if(!view)
        return;
    int index = model->indexOfFile(filePath);
    view->reloadItem(parentOffset() + (mShowDirs ? model->dirCount() + index : index));
}

void DirectoryPresenter::onDirRemoved(QString dirPath, int index) {
    Q_UNUSED(dirPath)
    if(!view || !mShowDirs)
        return;
    view->removeItem(parentOffset() + index);
}

void DirectoryPresenter::onDirRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo) {
    Q_UNUSED(fromPath)
    Q_UNUSED(toPath)
    if(!view || !mShowDirs)
        return;
    indexFrom += parentOffset();
    indexTo += parentOffset();
    auto oldSelection = view->selection();
    view->removeItem(indexFrom);
    view->insertItem(indexTo);
    // re-select if needed
    if(oldSelection.contains(indexFrom)) {
        if(oldSelection.count() == 1) {
            view->select(indexTo);
            view->focusOn(indexTo);
        } else if(oldSelection.count() > 1) {
            view->select(view->selection() << indexTo);
        }
    }
}

void DirectoryPresenter::onDirAdded(QString dirPath) {
    if(!view || !mShowDirs)
        return;
    int index = model->indexOfDir(dirPath);
    view->insertItem(parentOffset() + index);
}

bool DirectoryPresenter::showDirs() {
    return mShowDirs;
}

void DirectoryPresenter::setShowDirs(bool mode) {
    if(mode == mShowDirs)
        return;
    mShowDirs = mode;
    populateView();
}

void DirectoryPresenter::setShowParentDir(bool mode) {
    if(mode == mShowParentDir)
        return;
    mShowParentDir = mode;
    populateView();
}

QList<QString> DirectoryPresenter::selectedPaths() const {
    QList<QString> paths;
    if(!view || !model)
        return paths;
    int offset = parentOffset();
    if(mShowDirs) {
        for(auto i : view->selection()) {
            if(i < offset)
                continue;
            i -= offset;
            if(i < model->dirCount())
                paths << model->dirPathAt(i);
            else
                paths << model->filePathAt(i - model->dirCount());
        }
    } else {
        for(auto i : view->selection()) {
            if(i < offset)
                continue;
            i -= offset;
            paths << model->filePathAt(i);
        }
    }
    return paths;
}

void DirectoryPresenter::generateThumbnails(QList<int> indexes, int size, bool crop, bool force) {
    if(!view || !model)
        return;
    thumbnailer.clearTasks();
    int offset = parentOffset();
    for(int i : indexes) {
        if(i == 0 && offset) {
            view->setThumbnail(i, createParentDirThumbnail(size));
        }
    }
    if(!mShowDirs) {
        for(int i : indexes) {
            i -= offset;
            if(i < 0)
                continue;
            thumbnailer.getThumbnailAsync(model->filePathAt(i), size, crop, force);
        }
        return;
    }
    for(int i : indexes) {
        i -= offset;
        if(i < 0)
            continue;
        if(i < model->dirCount()) {
            view->setThumbnail(i + offset, createDirThumbnail(model->dirPathAt(i), model->dirNameAt(i), "Folder", size));
        } else {
            QString path = model->filePathAt(i - model->dirCount());
            thumbnailer.getThumbnailAsync(path, size, crop, force);
        }
    }
}

void DirectoryPresenter::onThumbnailReady(std::shared_ptr<Thumbnail> thumb, QString filePath) {
    if(!view || !model)
        return;
    int index = model->indexOfFile(filePath);
    if(index == -1)
        return;
    view->setThumbnail(parentOffset() + (mShowDirs ? model->dirCount() + index : index), thumb);
}

void DirectoryPresenter::onItemActivated(int absoluteIndex) {
    if(!model)
        return;
    int offset = parentOffset();
    if(absoluteIndex == 0 && offset) {
        emit dirActivated(parentDirPath());
        return;
    }
    absoluteIndex -= offset;
    if(absoluteIndex < 0)
        return;
    if(!mShowDirs) {
        emit fileActivated(model->filePathAt(absoluteIndex));
        return;
    }
    if(absoluteIndex < model->dirCount())
        emit dirActivated(model->dirPathAt(absoluteIndex));
    else
        emit fileActivated(model->filePathAt(absoluteIndex - model->dirCount()));
}

void DirectoryPresenter::onDraggedOut() {
    emit draggedOut(selectedPaths());
}

void DirectoryPresenter::onDraggedOver(int index) {
    if(!model || view->selection().contains(index))
        return;
    int viewIndex = index;
    int offset = parentOffset();
    if(index == 0 && offset) {
        view->setDragHover(viewIndex);
        return;
    }
    index -= offset;
    if(showDirs() && index >= 0 && index < model->dirCount())
        view->setDragHover(viewIndex);

}

void DirectoryPresenter::onDroppedInto(const QMimeData *data, QObject *source, int targetIndex) {
    if(!data->hasUrls() || model->source() != SOURCE_DIRECTORY)
        return;

    // ignore drops into selected / current folder when we are the source of dropEvent
    if(source && (view->selection().contains(targetIndex) || targetIndex == -1) )
        return;
    // ignore drops into a file
    // todo: drop into a current dir when target is a file
    int offset = parentOffset();
    int modelIndex = targetIndex - offset;
    if(targetIndex == 0 && offset) {
        // parent dir is a valid drop target
    } else if(showDirs() && modelIndex >= model->dirCount()) {
        return;
    } else if(!showDirs() && targetIndex >= 0) {
        return;
    }

    // convert urls to qstrings
    QStringList pathList;
    QList<QUrl> urlList = data->urls();
    for(int i = 0; i < urlList.size(); ++i)
        pathList.append(urlList.at(i).toLocalFile());

    // get target dir path
    QString destDir;
    if(targetIndex == 0 && offset)
        destDir = parentDirPath();
    else if(showDirs() && modelIndex >= 0 && modelIndex < model->dirCount())
        destDir = model->dirPathAt(modelIndex);
    if(destDir.isEmpty()) // fallback to the current dir
        destDir = model->directoryPath();
    pathList.removeAll(destDir); // remove target dir from source list

    // pass to core
    emit droppedInto(pathList, destDir);
}

void DirectoryPresenter::selectAndFocus(QString path) {
    if(!model || !view || path.isEmpty())
        return;
    if(model->containsDir(path) && showDirs()) {
        int dirIndex = model->indexOfDir(path);
        view->select(parentOffset() + dirIndex);
        view->focusOn(parentOffset() + dirIndex);
    } else if(model->containsFile(path)) {
        int fileIndex = parentOffset() + (showDirs() ? model->indexOfFile(path) + model->dirCount() : model->indexOfFile(path));
        view->select(fileIndex);
        view->focusOn(fileIndex);
    }
}

void DirectoryPresenter::selectAndFocus(int absoluteIndex) {
    if(!model || !view)
        return;
    view->select(absoluteIndex);
    view->focusOn(absoluteIndex);
}

bool DirectoryPresenter::hasParentDir() const {
    if(!mShowParentDir || !model || model->source() != SOURCE_DIRECTORY)
        return false;
    QDir dir(model->directoryPath());
    return dir.cdUp();
}

int DirectoryPresenter::parentOffset() const {
    return hasParentDir() ? 1 : 0;
}

QString DirectoryPresenter::parentDirPath() const {
    if(!model)
        return "";
    QDir dir(model->directoryPath());
    if(dir.cdUp())
        return dir.absolutePath();
    return "";
}

std::shared_ptr<Thumbnail> DirectoryPresenter::createDirThumbnail(const QString &path, const QString &name, const QString &info, int size) {
    QSvgRenderer svgRenderer;
    svgRenderer.load(QString(":/res/icons/common/other/folder32-scalable.svg"));
    int factor = (size * 0.90f) / svgRenderer.defaultSize().width();
    QPixmap *pixmap = new QPixmap(svgRenderer.defaultSize() * factor);
    pixmap->fill(Qt::transparent);
    QPainter pixPainter(pixmap);
    svgRenderer.render(&pixPainter);
    pixPainter.end();

    ImageLib::recolor(*pixmap, settings->colorScheme().icons);
    drawDirPreview(*pixmap, dirPreviewImages(path, size));

    return std::shared_ptr<Thumbnail>(new Thumbnail(name, info, size, std::shared_ptr<QPixmap>(pixmap)));
}

QList<QImage> DirectoryPresenter::dirPreviewImages(const QString &path, int targetSize) const {
    QList<QImage> images;
    QDir dir(path);
    if(!dir.exists() || !dir.isReadable())
        return images;

    QDir::Filters filters = QDir::Files | QDir::Readable | QDir::NoDotAndDotDot;
    if(settings->showHiddenFiles())
        filters |= QDir::Hidden;

    const int maxScannedEntries = 80;
    int scanned = 0;
    QFileInfoList entries = dir.entryInfoList(filters, QDir::Name | QDir::IgnoreCase);
    for(const QFileInfo &entry : entries) {
        if(scanned++ >= maxScannedEntries || images.count() >= 4)
            break;

        QImageReader reader(entry.absoluteFilePath());
        if(!reader.canRead())
            continue;

        QSize originalSize = reader.size();
        if(originalSize.isValid()) {
            QSize scaledSize = originalSize.scaled(targetSize, targetSize, Qt::KeepAspectRatioByExpanding);
            reader.setScaledSize(scaledSize);
        }

        QImage image = reader.read();
        if(!image.isNull())
            images << image;
    }
    return images;
}

void DirectoryPresenter::drawDirPreview(QPixmap &pixmap, const QList<QImage> &images) const {
    if(images.isEmpty())
        return;

    int gutter = qMax(2, pixmap.width() / 28);
    QRect previewRect(pixmap.width() * 0.16,
                      pixmap.height() * 0.30,
                      pixmap.width() * 0.69,
                      pixmap.height() * 0.52);

    int columns = images.count() == 1 ? 1 : 2;
    int rows = images.count() <= 2 ? 1 : 2;
    int cellWidth = (previewRect.width() - gutter * (columns - 1)) / columns;
    int cellHeight = (previewRect.height() - gutter * (rows - 1)) / rows;
    if(cellWidth <= 0 || cellHeight <= 0)
        return;

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setPen(QPen(settings->colorScheme().folderview, qMax(1, pixmap.width() / 42)));
    painter.setBrush(Qt::NoBrush);

    for(int i = 0; i < images.count(); i++) {
        int row = i / columns;
        int column = i % columns;
        QRect cell(previewRect.left() + column * (cellWidth + gutter),
                   previewRect.top() + row * (cellHeight + gutter),
                   cellWidth,
                   cellHeight);

        QImage scaled = images.at(i).scaled(cell.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QRect source((scaled.width() - cell.width()) / 2,
                     (scaled.height() - cell.height()) / 2,
                     cell.width(),
                     cell.height());
        painter.drawImage(cell, scaled, source);
        painter.drawRect(cell.adjusted(0, 0, -1, -1));
    }
}

std::shared_ptr<Thumbnail> DirectoryPresenter::createParentDirThumbnail(int size) {
    QPixmap source(":/res/icons/common/buttons/panel/up16@2x.png");
    if(source.isNull())
        source = QPixmap(":/res/icons/common/buttons/panel/up16.png");
    ImageLib::recolor(source, settings->colorScheme().icons);

    QPixmap *pixmap = new QPixmap(size, size);
    pixmap->fill(Qt::transparent);

    QSize iconSize(size * 0.55f, size * 0.55f);
    QPixmap scaled = source.scaled(iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter painter(pixmap);
    painter.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
    painter.end();

    return std::shared_ptr<Thumbnail>(new Thumbnail("..", "Parent folder", size, std::shared_ptr<QPixmap>(pixmap)));
}
