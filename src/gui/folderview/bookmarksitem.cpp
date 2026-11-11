#include "bookmarksitem.h"

#include "gui/folderview/bookmarkdrag.h"
#include "gui/folderview/folderdrop.h"

#include <QApplication>

#include <utility>

BookmarksItem::BookmarksItem(QString _dirName, QString _dirPath, QWidget *parent)
    : QWidget(parent), dirName(std::move(_dirName)), dirPath(std::move(_dirPath)), mHighlighted(false)
{
    this->setContentsMargins(0,0,0,0);
    layout.setContentsMargins(10,6,10,6);
    setAcceptDrops(true);
    dirNameLabel.setText(dirName);
    setToolTip(QDir::toNativeSeparators(dirPath));

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
    const bool click = mPressed && !mDragged;
    mPressed = false;
    if(event->button() == Qt::LeftButton && click)
        emit clicked(dirPath);
}

void BookmarksItem::mousePressEvent(QMouseEvent *event) {
    event->accept();
    mPressed = event->button() == Qt::LeftButton;
    mDragged = false;
    mPressPos = event->position().toPoint();
}

// a click that moves less than the drag distance is still a click
void BookmarksItem::mouseMoveEvent(QMouseEvent *event) {
    if(!mPressed || mDragged || isRenaming() || !(event->buttons() & Qt::LeftButton))
        return;
    if((event->position().toPoint() - mPressPos).manhattanLength() >= QApplication::startDragDistance()) {
        mDragged = true;
        emit reorderDragRequested(dirPath);
    }
}

void BookmarksItem::contextMenuEvent(QContextMenuEvent *event) {
    event->accept();
    emit contextMenuRequested(dirPath, event->globalPos());
}

void BookmarksItem::setLabel(const QString& name) {
    if(name == dirName)
        return;
    dirName = name;
    dirNameLabel.setText(dirName);
}

void BookmarksItem::startRename() {
    if(!mEditor) {
        mEditor = new QLineEdit(this);
        mEditor->setContextMenuPolicy(Qt::NoContextMenu);
        mEditor->setObjectName(QStringLiteral("inlineRenameEditor"));
        mEditor->installEventFilter(this);
        layout.insertWidget(1, mEditor);
    }
    mEditor->setText(dirName);
    mEditor->selectAll();
    dirNameLabel.hide();
    mEditor->show();
    mEditor->setFocus(Qt::OtherFocusReason);
}

void BookmarksItem::endRename(bool commit) {
    if(!isRenaming())
        return;
    const QString text = mEditor->text().trimmed();
    mEditor->hide(); // after this the focus-out filter does nothing
    dirNameLabel.show();
    if(commit)
        emit renameCommitted(dirPath, text);
}

bool BookmarksItem::eventFilter(QObject *watched, QEvent *event) {
    if(mEditor && watched == mEditor) {
        if(event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if(key == Qt::Key_Escape) {
                endRename(false);
                return true;
            }
            if(key == Qt::Key_Return || key == Qt::Key_Enter) {
                endRename(true);
                return true;
            }
        } else if(event->type() == QEvent::FocusOut) {
            endRename(false);
        }
    }
    return QWidget::eventFilter(watched, event);
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
    if(isBookmarkDrag(event->mimeData())) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
        emit reorderDropped(QString::fromUtf8(event->mimeData()->data(QLatin1String(kBookmarkMime))),
                            mapToParent(event->position().toPoint()));
        return;
    }
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
    if(isBookmarkDrag(event->mimeData())) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
        emit reorderDragMoved(mapToParent(event->position().toPoint()));
        return;
    }
    if(event->mimeData()->hasUrls())
        acceptFolderDrop(event);
    setProperty("hover", true);
    update();
}

// Ctrl can be pressed or released while hovering
void BookmarksItem::dragMoveEvent(QDragMoveEvent *event) {
    if(isBookmarkDrag(event->mimeData())) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
        emit reorderDragMoved(mapToParent(event->position().toPoint()));
        return;
    }
    if(event->mimeData()->hasUrls())
        acceptFolderDrop(event);
    else
        event->ignore();
}

void BookmarksItem::dragLeaveEvent(QDragLeaveEvent *event) {
    Q_UNUSED(event)
    emit reorderDragLeft();
    setProperty("hover", false);
    update();
}
