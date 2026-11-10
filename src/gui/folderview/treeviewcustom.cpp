#include "treeviewcustom.h"

TreeViewCustom::TreeViewCustom(QWidget *parent) : QTreeView(parent) {
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragEnabled(true);

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

void TreeViewCustom::dropEvent(QDropEvent *event) {
    QModelIndex dropIndex = indexAt(event->position().toPoint());
    if(dropIndex.isValid()) {
        QStringList paths;
        // TODO: QUrl gave me some issues previosly, test
        for(const auto& url : event->mimeData()->urls())
            paths << url.toLocalFile();
        emit droppedIn(paths, dropIndex);
    }
}

void TreeViewCustom::dragEnterEvent(QDragEnterEvent *event) {
    if(event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
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
    QModelIndex currentIndex = this->currentIndex();
    if( (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ) && currentIndex.isValid() ) {
        emit clicked(currentIndex);
    } else {
        QTreeView::keyPressEvent(event);
    }
}
