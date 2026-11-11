#include "treeviewcustom.h"

#include "gui/folderview/folderdrop.h"

TreeViewCustom::TreeViewCustom(QWidget *parent) : QTreeView(parent) {
    setAcceptDrops(true);
    setDropIndicatorShown(true);

    // proxy scrollbar
    this->verticalScrollBar()->setStyleSheet("max-width: 0px;");
    overlayScrollbar.setParent(this);
    overlayScrollbar.setStyleSheet("background-color: transparent;");
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, &overlayScrollbar, &QScrollBar::setRange);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, &overlayScrollbar, &QScrollBar::setValue);
    connect(&overlayScrollbar, &QScrollBar::valueChanged, [this]() {
        this->verticalScrollBar()->setValue(overlayScrollbar.value());
    });
    readSettings();
    connect(settings, &Settings::settingsChanged, this, &TreeViewCustom::readSettings);
}

void TreeViewCustom::readSettings() {
    const ColorScheme &colors = settings->colorScheme();
    if(mHandleColor == colors.scrollbar && mHandleHoverColor == colors.scrollbar_hover &&
       mHandleIdleColor == colors.folderview_hc)
        return;
    mHandleColor = colors.scrollbar;
    mHandleHoverColor = colors.scrollbar_hover;
    mHandleIdleColor = colors.folderview_hc;
    mAppliedHandleColor = QColor(); // rebuild on the next update
}

// the model's own drop handling would move files itself; the app's file operations do it instead
void TreeViewCustom::dropEvent(QDropEvent *event) {
    stopAutoScroll();
    setState(NoState);
    viewport()->update();
    const QModelIndex dropIndex = indexAt(event->position().toPoint());
    const QStringList paths = droppedLocalPaths(event->mimeData());
    if(!dropIndex.isValid() || paths.isEmpty()) {
        event->ignore();
        return;
    }
    acceptFolderDropped(event);
    emit droppedIn(paths, dropIndex, folderDropCopies(event));
}

void TreeViewCustom::dragEnterEvent(QDragEnterEvent *event) {
    QTreeView::dragEnterEvent(event);
    if(event->mimeData()->hasUrls())
        acceptFolderDrop(event);
    else
        event->ignore();
}

// over a row only; the base class draws the indicator and scrolls near the edges
void TreeViewCustom::dragMoveEvent(QDragMoveEvent *event) {
    QTreeView::dragMoveEvent(event);
    if(event->mimeData()->hasUrls() && indexAt(event->position().toPoint()).isValid())
        acceptFolderDrop(event);
    else
        event->ignore();
}

void TreeViewCustom::showEvent(QShowEvent *event) {
    QTreeView::showEvent(event);
}

void TreeViewCustom::enterEvent(QEnterEvent *event) {
    QTreeView::enterEvent(event);
    updateScrollbarStyle(true);
}
void TreeViewCustom::leaveEvent(QEvent *event) {
    QTreeView::leaveEvent(event);
    updateScrollbarStyle(false);
}

QSize TreeViewCustom::minimumSizeHint() const {
    QSize sz(QTreeView::minimumSizeHint().width(), 0);
    return sz;
}

void TreeViewCustom::resizeEvent(QResizeEvent *event) {
    QTreeView::resizeEvent(event);
    positionEditor();
    updateScrollbarStyle(underMouse());
}

void TreeViewCustom::updateScrollbarStyle(bool hovered) {
    overlayScrollbar.setGeometry(width() - SCROLLBAR_WIDTH, 0, SCROLLBAR_WIDTH, height());
    const QColor &handle = hovered ? mHandleColor : mHandleIdleColor;
    if(handle != mAppliedHandleColor) {
        mAppliedHandleColor = handle;
        overlayScrollbar.setStyleSheet("QScrollBar { background-color: transparent; } QScrollBar::handle:vertical { background-color: " +
                                       handle.name() + " } QScrollBar::handle:vertical:hover { background-color: " +
                                       mHandleHoverColor.name() + " }");
    }
    overlayScrollbar.setVisible( (this->verticalScrollBar()->maximum()) );
}

void TreeViewCustom::keyPressEvent(QKeyEvent* event) {
    if(event->key() == Qt::Key_Menu || (event->key() == Qt::Key_F10 && event->modifiers() == Qt::ShiftModifier)) {
        requestMenuForCurrent();
        event->accept();
        return;
    }
    QModelIndex currentIndex = this->currentIndex();
    if(event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        event->accept();
        emit leaveRequested();
        return;
    }
    // accepted even when unused: the app's own F2 / Delete would act on the grid's selection
    if(event->key() == Qt::Key_F2 || event->key() == Qt::Key_Delete) {
        event->accept();
        if(currentIndex.isValid() && event->modifiers() == Qt::NoModifier) {
            if(event->key() == Qt::Key_F2)
                emit renameKeyPressed(currentIndex);
            else
                emit trashKeyPressed(currentIndex);
        }
        return;
    }
    if( (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ) && currentIndex.isValid() ) {
        emit clicked(currentIndex);
    } else {
        QTreeView::keyPressEvent(event);
    }
}

void TreeViewCustom::requestMenuForCurrent() {
    const QModelIndex index = currentIndex();
    if(!index.isValid())
        return;
    scrollTo(index);
    emit contextMenuRequestedAt(index, viewport()->mapToGlobal(visualRect(index).bottomLeft()));
}

// the row under the pointer; empty space opens nothing
void TreeViewCustom::contextMenuEvent(QContextMenuEvent *event) {
    event->accept();
    if(event->reason() == QContextMenuEvent::Keyboard) {
        requestMenuForCurrent();
        return;
    }
    const QModelIndex index = indexAt(event->pos());
    if(index.isValid())
        emit contextMenuRequestedAt(index, event->globalPos());
}

// only the left button selects and opens; a right or middle click must not change the current folder
void TreeViewCustom::mousePressEvent(QMouseEvent *event) {
    if(event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }
    QTreeView::mousePressEvent(event);
}

void TreeViewCustom::mouseReleaseEvent(QMouseEvent *event) {
    if(event->button() != Qt::LeftButton) {
        event->accept();
        if(event->button() == Qt::MiddleButton) {
            const QModelIndex index = indexAt(event->position().toPoint());
            if(index.isValid())
                emit middleClicked(index);
        }
        return;
    }
    QTreeView::mouseReleaseEvent(event);
}

void TreeViewCustom::startRename(const QModelIndex &index, const QString &name) {
    beginEdit(EditMode::Rename, index, name);
}

void TreeViewCustom::startNewFolder(const QModelIndex &parent, const QString &name) {
    beginEdit(EditMode::NewFolder, parent, name);
}

void TreeViewCustom::beginEdit(EditMode mode, const QModelIndex &index, const QString &name) {
    if(!index.isValid())
        return;
    cancelEdit();
    if(!mEditor) {
        mEditor = new QLineEdit(viewport());
        mEditor->setContextMenuPolicy(Qt::NoContextMenu);
        mEditor->setObjectName(QStringLiteral("inlineRenameEditor"));
        mEditor->installEventFilter(this);
    }
    mEditMode = mode;
    mEditIndex = index;
    if(mode == EditMode::NewFolder)
        expand(index);
    mEditor->setText(name);
    mEditor->selectAll();
    scrollTo(index);
    positionEditor();
    mEditor->show();
    mEditor->raise();
    mEditor->setFocus(Qt::OtherFocusReason);
}

// over the renamed row, or over the row below the parent for a new folder
void TreeViewCustom::positionEditor() {
    if(mEditMode == EditMode::None)
        return;
    if(!mEditIndex.isValid()) {
        cancelEdit();
        return;
    }
    QRect rect = visualRect(mEditIndex);
    const int rowHeight = qMax(rect.height(), mEditor->minimumSizeHint().height());
    if(mEditMode == EditMode::NewFolder) {
        rect.translate(indentation(), rect.height());
        // the row below must show: bring the parent up when it is the last one in view
        if(rect.bottom() > viewport()->height() - 1)
            verticalScrollBar()->setValue(verticalScrollBar()->value() + rect.bottom() - viewport()->height() + 2);
        if(rect.bottom() > viewport()->height() - 1) {
            rect = visualRect(mEditIndex);
            rect.translate(indentation(), rect.height());
        }
    }
    rect.setRight(viewport()->width() - 1);
    rect.setHeight(rowHeight);
    mEditor->setGeometry(rect);
}

void TreeViewCustom::scrollContentsBy(int dx, int dy) {
    QTreeView::scrollContentsBy(dx, dy);
    positionEditor();
}

void TreeViewCustom::commitEdit() {
    if(mEditMode == EditMode::None)
        return;
    const EditMode mode = mEditMode;
    const QModelIndex index = mEditIndex;
    const QString name = mEditor->text().trimmed();
    mEditMode = EditMode::None; // before hide(), so the focus-out filter does nothing
    mEditor->hide();
    setFocus();
    if(name.isEmpty() || !index.isValid())
        return;
    if(mode == EditMode::Rename)
        emit renameCommitted(index, name);
    else
        emit newFolderCommitted(index, name);
}

void TreeViewCustom::cancelEdit() {
    if(mEditMode == EditMode::None)
        return;
    mEditMode = EditMode::None;
    mEditor->hide();
    setFocus();
}

bool TreeViewCustom::eventFilter(QObject *watched, QEvent *event) {
    if(mEditor && watched == mEditor) {
        if(event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if(key == Qt::Key_Escape) {
                cancelEdit();
                return true;
            }
            if(key == Qt::Key_Return || key == Qt::Key_Enter) {
                commitEdit();
                return true;
            }
        } else if(event->type() == QEvent::FocusOut) {
            cancelEdit();
        }
    }
    return QTreeView::eventFilter(watched, event);
}
