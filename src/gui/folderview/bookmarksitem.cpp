#include "bookmarksitem.h"

#include "gui/folderview/folderdrop.h"

#include <utility>

BookmarksItem::BookmarksItem(QString _dirName, QString _dirPath, QWidget *parent)
    : QWidget(parent), dirName(std::move(_dirName)), dirPath(std::move(_dirPath)), mHighlighted(false)
{
    this->setContentsMargins(0,0,0,0);
    layout.setContentsMargins(10,6,10,6);
    setAcceptDrops(true);
    dirNameLabel.setText(dirName);

    spacer = new QSpacerItem(16, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    folderIconWidget.setAttribute(Qt::WA_TransparentForMouseEvents, true);
    folderIconWidget.setIconPath(":/res/icons/common/menuitem/folder16.png");
    folderIconWidget.setMinimumSize(16, 16);
    folderIconWidget.installEventFilter(this);

    removeItemButton.setIconPath(":/res/icons/common/buttons/panel-small/remove12.png");
    removeItemButton.setMinimumSize(16, 16);
    removeItemButton.installEventFilter(this);

    removeItemButton.setAccessibleName("BookmarksItemRemoveLabel");

    connect(&removeItemButton, &IconButton::clicked, this, &BookmarksItem::onRemoveClicked);

    layout.addWidget(&folderIconWidget);
    layout.addWidget(&dirNameLabel);
    layout.addSpacerItem(spacer);
    layout.addWidget(&removeItemButton);

    setMouseTracking(true);

    setLayout(&layout);
}

QString BookmarksItem::path() {
    return dirPath;
}

void BookmarksItem::setHighlighted(bool mode) {
    if(mode != mHighlighted) {
        mHighlighted = mode;
        setProperty("highlighted", mHighlighted);
        style()->unpolish(this);
        style()->polish(this);
    }
}

void BookmarksItem::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
    if(event->button() == Qt::LeftButton)
        emit clicked(dirPath);
}

void BookmarksItem::mousePressEvent(QMouseEvent *event) {
    event->accept();
}

void BookmarksItem::onRemoveClicked() {
    emit removeClicked(dirPath);
}

void BookmarksItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void BookmarksItem::dropEvent(QDropEvent *event) {
    setProperty("hover", false);
    update();
    const QStringList paths = droppedLocalPaths(event->mimeData());
    if(paths.isEmpty()) {
        event->ignore();
        return;
    }
    acceptFolderDropped(event);
    emit droppedIn(paths, dirPath, folderDropCopies(event));
}

void BookmarksItem::dragEnterEvent(QDragEnterEvent *event) {
    if(event->mimeData()->hasUrls())
        acceptFolderDrop(event);
    setProperty("hover", true);
    update();
}

// Ctrl can be pressed or released while hovering
void BookmarksItem::dragMoveEvent(QDragMoveEvent *event) {
    if(event->mimeData()->hasUrls())
        acceptFolderDrop(event);
    else
        event->ignore();
}

void BookmarksItem::dragLeaveEvent(QDragLeaveEvent *event) {
    Q_UNUSED(event)
    setProperty("hover", false);
    update();
}
