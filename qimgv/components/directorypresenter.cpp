#include "directorypresenter.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

namespace {
    QString formattedSize(qint64 bytes) {
        if(bytes < 1024)
            return QString::number(bytes) + " Bytes";

        static const QStringList units = {"KiB", "MiB", "GiB", "TiB"};
        double value = bytes / 1024.0;
        int unit = 0;
        while(value >= 1024.0 && unit < units.count() - 1) {
            value /= 1024.0;
            unit++;
        }
        return QString::number(value, 'f', 2) + " " + units.at(unit);
    }

    QRect visibleAlphaBounds(const QImage &image) {
        if(image.isNull())
            return QRect();

        int left = image.width();
        int top = image.height();
        int right = -1;
        int bottom = -1;
        for(int y = 0; y < image.height(); y++) {
            for(int x = 0; x < image.width(); x++) {
                if(image.pixelColor(x, y).alpha() == 0)
                    continue;
                left = qMin(left, x);
                top = qMin(top, y);
                right = qMax(right, x);
                bottom = qMax(bottom, y);
            }
        }
        if(right < left || bottom < top)
            return QRect();
        return QRect(QPoint(left, top), QPoint(right, bottom));
    }
}

DirectoryPresenter::DirectoryPresenter(QObject *parent) : QObject(parent), mShowDirs(false) {
    connect(&thumbnailer, &Thumbnailer::thumbnailReady, this, &DirectoryPresenter::onThumbnailReady);
    connect(&thumbnailer, &Thumbnailer::dirThumbnailReady, this, &DirectoryPresenter::onDirThumbnailReady);
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
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(selectionChanged()),
            this, SLOT(onSelectionChanged()));
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
    emitStatusText();
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
    emitStatusText();
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
    emitStatusText();
}

void DirectoryPresenter::onFileAdded(QString filePath) {
    if(!view)
        return;
    int index = model->indexOfFile(filePath);
    view->insertItem(parentOffset() + (mShowDirs ? model->dirCount() + index : index));
    emitStatusText();
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
    emitStatusText();
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
    emitStatusText();
}

void DirectoryPresenter::onDirAdded(QString dirPath) {
    if(!view || !mShowDirs)
        return;
    int index = model->indexOfDir(dirPath);
    view->insertItem(parentOffset() + index);
    emitStatusText();
}

bool DirectoryPresenter::showDirs() {
    return mShowDirs;
}

void DirectoryPresenter::setShowDirs(bool mode) {
    if(mode == mShowDirs)
        return;
    mShowDirs = mode;
    populateView();
    emitStatusText();
}

void DirectoryPresenter::setShowParentDir(bool mode) {
    if(mode == mShowParentDir)
        return;
    mShowParentDir = mode;
    populateView();
    emitStatusText();
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

QString DirectoryPresenter::statusText() const {
    if(!view || !model)
        return "";

    QList<QString> paths = selectedPaths();
    qint64 selectedBytes = 0;
    QStringList selectedNames;
    QStringList selectedDetails;

    for(const QString &path : paths) {
        QFileInfo info(path);
        selectedBytes += info.size();
        selectedNames << info.fileName();
        selectedDetails << info.fileName() + " " + formattedSize(info.size());
    }

    QString text = QString::number(realObjectCount()) + " object(s) / " +
                   QString::number(paths.count()) + " object(s) selected";
    if(paths.count())
        text += " [" + formattedSize(selectedBytes) + "]";

    if(paths.count() == 1)
        text += "  " + selectedDetails.first();
    else if(paths.count() > 1)
        text += "  " + selectedNames.join(", ");

    return text;
}

void DirectoryPresenter::emitStatusText() {
    emit statusTextChanged(statusText());
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
            thumbnailer.getDirThumbnailAsync(model->dirPathAt(i), size, settings->folderViewPreviewFit(), crop, force);
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

void DirectoryPresenter::onDirThumbnailReady(std::shared_ptr<Thumbnail> thumb, QString dirPath) {
    if(!view || !model)
        return;
    int index = model->indexOfDir(dirPath);
    if(index == -1)
        return;
    view->setThumbnail(parentOffset() + index, thumb);
}

void DirectoryPresenter::onItemActivated(int absoluteIndex) {
    if(!model)
        return;
    int offset = parentOffset();
    if(absoluteIndex == 0 && offset) {
        QString parentPath = parentDirPath();
        if(parentPath.isEmpty() || parentPath == model->directoryPath())
            return;
        // Route through the dedicated "go up" path so the directory we are
        // leaving gets selected/focused in the parent view, same as the
        // keyboard/up-button navigation.
        emit parentDirActivated();
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

void DirectoryPresenter::onSelectionChanged() {
    emitStatusText();
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
    if(QDir::cleanPath(dir.absolutePath()) == QDir::cleanPath(QDir::rootPath()))
        return false;
    return dir.cdUp();
}

int DirectoryPresenter::parentOffset() const {
    return hasParentDir() ? 1 : 0;
}

QString DirectoryPresenter::parentDirPath() const {
    if(!model)
        return "";
    QDir dir(model->directoryPath());
    if(QDir::cleanPath(dir.absolutePath()) == QDir::cleanPath(QDir::rootPath()))
        return "";
    if(dir.cdUp())
        return dir.absolutePath();
    return "";
}

int DirectoryPresenter::realObjectCount() const {
    if(!model)
        return 0;
    return mShowDirs ? model->totalCount() : model->fileCount();
}

std::shared_ptr<Thumbnail> DirectoryPresenter::createParentDirThumbnail(int size) {
    // QPixmap source(":/res/icons/common/buttons/panel/up16@2x.png");
    QPixmap source(":/res/icons/common/other/go-to-parent-dir.png");
    if(source.isNull())
        source = QPixmap(":/res/icons/common/buttons/panel/up16.png");
    source.setDevicePixelRatio(1.0);
    ImageLib::recolor(source, settings->folderViewParentIconColor());

    QPixmap *pixmap = new QPixmap(qRound(size * 1.10f), qRound(size * 1.10f));
    pixmap->fill(Qt::transparent);

    constexpr int kParentIconAnchorSize = 30;
    QSize iconSize(kParentIconAnchorSize, kParentIconAnchorSize);
    QPixmap scaled = source.scaled(iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(1.0);
    QRect visibleBounds = visibleAlphaBounds(scaled.toImage());
    QRectF bounds = visibleBounds.isValid() ? QRectF(visibleBounds) : QRectF(scaled.rect());
    QPointF drawPos = QRectF(pixmap->rect()).center() - bounds.center();
    QPainter painter(pixmap);
    painter.drawPixmap(drawPos, scaled);
    painter.end();

    return std::shared_ptr<Thumbnail>(new Thumbnail("..", "Parent folder", size, std::shared_ptr<QPixmap>(pixmap), false));
}
