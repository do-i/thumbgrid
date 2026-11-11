#include "bookmarkswidget.h"

BookmarksWidget::BookmarksWidget(QWidget *parent) : QWidget(parent), highlightedPath("") {
    setAcceptDrops(true);
    setContentsMargins(0,0,0,0);
    layout.setContentsMargins(0,0,0,0);
    layout.setSpacing(0);
    setLayout(&layout);
    connect(settings, &Settings::settingsChanged, this, &BookmarksWidget::readSettings);
    readSettings();
}

BookmarksWidget::~BookmarksWidget() {
}

// the folder's name; the root has none, so it shows its native path ("/", "C:\\")
QString BookmarksWidget::defaultLabel(const QString& dirPath) {
    const QString clean = QDir::cleanPath(dirPath);
    const QString name = QFileInfo(clean).fileName();
    return name.isEmpty() ? QDir::toNativeSeparators(clean.isEmpty() ? dirPath : clean) : name;
}

QString BookmarksWidget::labelFor(const QString& dirPath) const {
    return labels.value(dirPath, defaultLabel(dirPath));
}

BookmarksItem *BookmarksWidget::itemFor(const QString& dirPath) const {
    for(int i = 0; i < layout.count(); i++) {
        auto w = dynamic_cast<BookmarksItem*>(layout.itemAt(i)->widget());
        if(w && w->path() == dirPath)
            return w;
    }
    return nullptr;
}

BookmarksItem *BookmarksWidget::createItem(const QString& dirPath) {
    auto *item = new BookmarksItem(labelFor(dirPath), dirPath);
    connect(item, &BookmarksItem::clicked, this, &BookmarksWidget::bookmarkClicked);
    connect(item, &BookmarksItem::removeClicked, this, &BookmarksWidget::removeBookmark);
    connect(item, &BookmarksItem::droppedIn, this, &BookmarksWidget::droppedIn);
    connect(item, &BookmarksItem::renameCommitted, this, &BookmarksWidget::renameBookmark);
    connect(item, &BookmarksItem::contextMenuRequested, this, &BookmarksWidget::contextMenuRequested);
    item->setHighlighted(dirPath == highlightedPath);
    return item;
}

// makes the rows match the stored list: only rows that differ are added, removed or moved; no save
void BookmarksWidget::readSettings() {
    QStringList stored = settings->bookmarks();
    const bool storedEmpty = stored.isEmpty();
    if(storedEmpty)
        stored << QDir::homePath();
    labels = settings->bookmarkLabels();
    for(int i = layout.count() - 1; i >= 0; i--) {
        auto w = dynamic_cast<BookmarksItem*>(layout.itemAt(i)->widget());
        if(w && !stored.contains(w->path()))
            dropItem(w);
        else if(w)
            w->setLabel(labelFor(w->path()));
    }
    for(int i = 0; i < stored.count(); i++) {
        BookmarksItem *item = itemFor(stored.at(i));
        if(!item)
            item = createItem(stored.at(i));
        else if(layout.itemAt(i)->widget() == item)
            continue;
        else
            layout.removeWidget(item);
        layout.insertWidget(i, item);
    }
    const bool changed = paths != stored;
    paths = stored;
    // an empty list starts with home, as it always did
    if(storedEmpty)
        saveBookmarks();
    if(changed)
        emit bookmarksChanged();
}

void BookmarksWidget::saveBookmarks() {
    settings->setBookmarks(paths);
}

void BookmarksWidget::addBookmark(const QString& dirPath) {
    if(paths.contains(dirPath))
        return;
    paths.push_back(dirPath);
    layout.addWidget(createItem(dirPath));
    saveBookmarks();
    emit bookmarksChanged();
}

bool BookmarksWidget::hasBookmark(const QString& dirPath) const {
    return paths.contains(dirPath) || paths.contains(QDir::cleanPath(dirPath));
}

void BookmarksWidget::dropItem(BookmarksItem *item) {
    layout.removeWidget(item);
    item->hide();
    item->disconnect(this);
    item->deleteLater();
}

void BookmarksWidget::removeBookmark(const QString& dirPath) {
    if(BookmarksItem *item = itemFor(dirPath)) {
        dropItem(item);
        paths.removeAll(dirPath);
        if(labels.remove(dirPath))
            settings->setBookmarkLabels(labels);
        saveBookmarks();
        emit bookmarksChanged();
    }
}

void BookmarksWidget::renameBookmark(const QString& dirPath, const QString& label) {
    BookmarksItem *item = itemFor(dirPath);
    if(!item)
        return;
    const QString text = label.trimmed();
    if(text.isEmpty() || text == defaultLabel(dirPath))
        labels.remove(dirPath);
    else
        labels.insert(dirPath, text);
    settings->setBookmarkLabels(labels);
    item->setLabel(labelFor(dirPath));
}

void BookmarksWidget::startRename(const QString& dirPath) {
    if(BookmarksItem *item = itemFor(dirPath))
        item->startRename();
}

void BookmarksWidget::onPathChanged(const QString& path) {
    if(highlightedPath == path)
        return;
    if(BookmarksItem *old = itemFor(highlightedPath))
        old->setHighlighted(false);
    highlightedPath = path;
    if(BookmarksItem *item = itemFor(path))
        item->setHighlighted(true);
}

void BookmarksWidget::dropEvent(QDropEvent *) {
}

void BookmarksWidget::dragEnterEvent(QDragEnterEvent *event) {
    if(event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void BookmarksWidget::contextMenuEvent(QContextMenuEvent *event) {
    event->accept();
    emit contextMenuRequested(QString(), event->globalPos());
}
