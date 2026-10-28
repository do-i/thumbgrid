#include "foldergridview.h"

#include <algorithm>
#include <utility>

namespace {

bool isNavigationAction(const QString &action) {
    static const QStringList actions{"selectPrevious", "selectNext", "selectAbove", "selectBelow",
                                     "selectPageUp", "selectPageDown", "selectFirst", "selectLast"};
    return actions.contains(action);
}

// the keys that used to move the selection in code, named as bindings name them
QString navigationKeyName(int key) {
    switch(key) {
    case Qt::Key_Left:     return QStringLiteral("Left");
    case Qt::Key_Right:    return QStringLiteral("Right");
    case Qt::Key_Up:       return QStringLiteral("Up");
    case Qt::Key_Down:     return QStringLiteral("Down");
    case Qt::Key_PageUp:   return QStringLiteral("PgUp");
    case Qt::Key_PageDown: return QStringLiteral("PgDown");
    case Qt::Key_Home:     return QStringLiteral("Home");
    case Qt::Key_End:      return QStringLiteral("End");
    default:               return QString();
    }
}

// the event's binding name without Shift, Alt or Meta
QString plainKeyName(const QKeyEvent *event) {
    QKeyEvent plain(event->type(), event->key(),
                    event->modifiers() & ~(Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier),
                    event->nativeScanCode(), event->nativeVirtualKey(), event->nativeModifiers(),
                    event->text());
    return ShortcutBuilder::fromEvent(&plain);
}

} // namespace


// TODO: create a base class for this and the one on panel

namespace {
    int folderGridThumbnailWidth(int size) {
        return qRound(size * 1.28f);
    }
}

FolderGridView::FolderGridView(QWidget *parent)
    : ThumbnailView(Qt::Vertical, parent),
      shiftedCol(-1)
{
    offscreenPreloadArea = 2300;

    this->setAcceptDrops(true);

    this->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, true);
    this->scene.setBackgroundBrush(settings->colorScheme().folderview);
    this->setCacheMode(QGraphicsView::CacheBackground);

    // turn this off until [multi]selection is implemented
    setDrawScrollbarIndicator(false);
    setSelectMode(ACTIVATE_BY_DOUBLECLICK);

    mPreviewFit = settings->folderViewPreviewFit();
    mThumbColorSignature = settings->colorScheme().bakedThumbnailSignature();
    connect(settings, &Settings::settingsChanged, [this]() {
        this->scene.setBackgroundBrush(settings->colorScheme().folderview);
        // thumbnails bake the preview layout and recolored icons, so regenerate on fit or theme color changes
        QString colorSignature = settings->colorScheme().bakedThumbnailSignature();
        if(mPreviewFit != settings->folderViewPreviewFit() || mThumbColorSignature != colorSignature) {
            mPreviewFit = settings->folderViewPreviewFit();
            mThumbColorSignature = colorSignature;
            for(int i = 0; i < thumbnails.count(); i++)
                thumbnails.at(i)->unsetThumbnail();
            loadVisibleThumbnails();
        }
    });

    setupLayout();
    connect(this, &ThumbnailView::itemActivated,
            this, &FolderGridView::onitemSelected);

    contextMenu = new GridContextMenu(this);
    connect(contextMenu, &GridContextMenu::convertFormatRequested,
            this, &FolderGridView::convertFormatRequested);

    // keep the in-place rename editor glued to its cell while the view scrolls
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        if(renameIndex >= 0)
            positionRenameEditor();
    });
}

void FolderGridView::dropEvent(QDropEvent *event) {
    event->accept();
    ThumbnailWidget *item = qgraphicsitem_cast<ThumbnailWidget*>(itemAt(event->position().toPoint()));
    int index = -1;
    if(item) {
        index = thumbnails.indexOf(item);
        item->setDropHovered(false);
    }
    emit droppedInto(event->mimeData(), event->source(), index);
}

void FolderGridView::dragEnterEvent(QDragEnterEvent *event) {
    event->accept();
}

void FolderGridView::dragMoveEvent(QDragMoveEvent *event) {
    event->accept();
    ThumbnailWidget *item = qgraphicsitem_cast<ThumbnailWidget*>(itemAt(event->position().toPoint()));
    int index = -1;
    if(item)
        index = thumbnails.indexOf(item);
    // unselect previous
    if(index != lastDragTarget && checkRange(lastDragTarget))
        thumbnails.at(lastDragTarget)->setDropHovered(false);
    emit draggedOver(index);
    lastDragTarget = index;
}

void FolderGridView::dragLeaveEvent(QDragLeaveEvent *event) {
    event->accept();
    if(lastDragTarget < 0 || lastDragTarget >= thumbnails.count())
        return;
    thumbnails.at(lastDragTarget)->setDropHovered(false);
}

void FolderGridView::setDragHover(int index) {
    if(!checkRange(index))
        return;
    auto item = thumbnails.at(index);
    item->setDropHovered(true);
}

void FolderGridView::onitemSelected() {
    shiftedCol = -1;
}

void FolderGridView::updateScrollbarIndicator() {
    if(!thumbnails.count() || !selection().count())
        return;
    ThumbnailWidget *thumb = thumbnails.at(lastSelected());
    qreal itemCenter = (thumb->pos().y() + (thumb->height() / 2)) / scene.height();
    indicator = QRect(2, scrollBar->height() * itemCenter - indicatorSize, scrollBar->width() - 4, indicatorSize);
}

// probably unneeded
void FolderGridView::show() {
    ThumbnailView::show();
    setFocus();
}

// probably unneeded
void FolderGridView::hide() {
    cancelRename();
    exitSearchMode();
    ThumbnailView::hide();
    clearFocus();
}

// a repopulate means we are looking at a different (or re-read) directory
void FolderGridView::populate(int count) {
    exitSearchMode();
    // reused cells would carry the editor over to another file
    cancelRename();
    ThumbnailView::populate(count);
}

void FolderGridView::setDirectoryPath(QString path) {
    exitSearchMode();
    ThumbnailView::setDirectoryPath(std::move(path));
}

void FolderGridView::setShowLabels(bool mode) {
    ThumbnailStyle style = mode ? THUMB_NORMAL_CENTERED : THUMB_SIMPLE;
    for(int i = 0; i < thumbnails.count(); i++)
        thumbnails.at(i)->setThumbStyle(style);
    updateLayout();
    fitSceneToContents();
    focusOnSelection();
}

void FolderGridView::setShowInfo(bool mode) {
    for(int i = 0; i < thumbnails.count(); i++)
        thumbnails.at(i)->setShowInfo(mode);
    updateLayout();
    fitSceneToContents();
    focusOnSelection();
}

void FolderGridView::setLabelFontPointSize(int size) {
    for(int i = 0; i < thumbnails.count(); i++)
        thumbnails.at(i)->setLabelFontPointSize(size);
    updateLayout();
    fitSceneToContents();
    focusOnSelection();
}

void FolderGridView::setLabelBackgroundColor(const QColor &color) {
    for(int i = 0; i < thumbnails.count(); i++)
        thumbnails.at(i)->setLabelBackgroundColor(color);
}

void FolderGridView::setCellBackgroundColor(const QColor &color) {
    for(int i = 0; i < thumbnails.count(); i++)
        thumbnails.at(i)->setCellBackgroundColor(color);
}

void FolderGridView::focusOnSelection() {
    if(!thumbnails.count() || lastSelected() == -1)
        return;
    ThumbnailWidget *thumb = thumbnails.at(lastSelected());
    ensureVisible(thumb, 0, 0);
}

void FolderGridView::selectAbove() {
    if(!thumbnails.count() || lastSelected() == -1 || flowLayout->sameRow(0, lastSelected()))
        return;
    int newIndex;
    newIndex = flowLayout->itemAbove(lastSelected());
    if(shiftedCol >= 0) {
        int diff = shiftedCol - flowLayout->columnOf(lastSelected());
        newIndex += diff;
        shiftedCol = -1;
    }
    if(!checkRange(newIndex))
        newIndex = 0;
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectBelow() {
    if(!thumbnails.count() || lastSelected() == -1 || flowLayout->sameRow(lastSelected(), thumbnails.count() - 1))
        return;
    shiftedCol = -1;
    int newIndex = flowLayout->itemBelow(lastSelected());
    if(!checkRange(newIndex))
        newIndex = thumbnails.count() - 1;
    if(flowLayout->columnOf(newIndex) != flowLayout->columnOf(lastSelected()))
        shiftedCol = flowLayout->columnOf(lastSelected());
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectNext() {
    if(!thumbnails.count() || lastSelected() == thumbnails.count() - 1)
        return;
    if(!rangeSelection && lastSelected() == thumbnails.count() - 1) {
        select(lastSelected());
        return;
    }
    shiftedCol = -1;
    int newIndex = lastSelected() + 1;
    if(!checkRange(newIndex))
        newIndex = thumbnails.count() - 1;
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectPrev() {
    if(!thumbnails.count() || lastSelected() == 0)
        return;
    shiftedCol = -1;
    int newIndex = lastSelected() - 1;
    if(!checkRange(newIndex))
        newIndex = 0;
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::pageUp() {
    if(!thumbnails.count() || lastSelected() == -1 || flowLayout->sameRow(0, lastSelected()))
        return;
    int newIndex = lastSelected();
    int tmp;
    // 4 rows up
    for(int i = 0; i < 4; i++) {
        tmp = flowLayout->itemAbove(newIndex);
        if(checkRange(tmp))
            newIndex = tmp;
    }
    if(shiftedCol >= 0) {
        int diff = shiftedCol - flowLayout->columnOf(newIndex);
        newIndex += diff;
        shiftedCol = -1;
    }
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::pageDown() {
    if(!thumbnails.count() || lastSelected() == -1 || flowLayout->sameRow(lastSelected(), thumbnails.count() - 1))
        return;
    shiftedCol = -1;
    int newIndex = lastSelected();
    int tmp;
    // 4 rows down
    for(int i = 0; i < 4; i++) {
        tmp = flowLayout->itemBelow(newIndex);
        if(checkRange(tmp))
            newIndex = tmp;
    }
    if(flowLayout->columnOf(newIndex) != flowLayout->columnOf(lastSelected()))
        shiftedCol = flowLayout->columnOf(lastSelected());
    if(rangeSelection)
        addSelectionRange(newIndex);
    else
        select(newIndex);
    scrollToCurrent();
}

void FolderGridView::selectFirst() {
    if(!thumbnails.count())
        return;
    shiftedCol = -1;
    if(rangeSelection)
        addSelectionRange(0);
    else
        select(0);
    scrollToCurrent();
}

void FolderGridView::selectLast() {
    if(!thumbnails.count())
        return;
    shiftedCol = -1;
    if(rangeSelection)
        addSelectionRange(thumbnails.count() - 1);
    else
        select(thumbnails.count() - 1);
    scrollToCurrent();
}

void FolderGridView::scrollToCurrent() {
    scrollToItem(lastSelected());
}

// same as scrollToItem minus the animation
void FolderGridView::focusOn(int index) {
    if(!checkRange(index))
        return;
    ThumbnailWidget *thumb = thumbnails.at(index);
    ensureVisible(thumb, 0, 0);
    loadVisibleThumbnailsDelayed();
}

void FolderGridView::setupLayout() {
    this->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    flowLayout = new FlowLayout();
    flowLayout->setContentsMargins(4,3,4,0);
    setFrameShape(QFrame::NoFrame);
    scene.addItem(&holderWidget);
    holderWidget.setLayout(flowLayout);
    holderWidget.setContentsMargins(0,0,0,0);
}

ThumbnailWidget* FolderGridView::createThumbnailWidget() {
    ThumbnailWidget *widget = new ThumbnailWidget();
    widget->setPadding(3);
    widget->setMargins(1,1);
    widget->setLabelSpacing(5);
    widget->setFixedBackgroundRect(true);
    widget->setCellBorderVisible(true);
    widget->setTransparencyGridVisible(true);
    widget->setSymlinkBadgeVisible(true);
    widget->setThumbnailTopMargin(4);
    widget->setCellHeightRatio(0.93);
    ThumbnailStyle style = (settings->folderViewMode() == FV_SIMPLE) ? THUMB_SIMPLE : THUMB_NORMAL_CENTERED;
    widget->setThumbStyle(style);
    widget->setShowInfo(settings->folderViewShowInfo());
    widget->setLabelFontPointSize(settings->folderViewFontPointSize());
    widget->setLabelBackgroundColor(settings->folderViewLabelBackgroundColor());
    widget->setCellBackgroundColor(settings->folderViewCellBackgroundColor());
    widget->setThumbnailSize(this->mThumbnailSize); // TODO: constructor
    widget->setThumbnailAreaSize(folderGridThumbnailWidth(this->mThumbnailSize), this->mThumbnailSize);
    return widget;
}

void FolderGridView::addItemToLayout(ThumbnailWidget* widget, int pos) {
    widget->setThumbnailAreaSize(folderGridThumbnailWidth(this->mThumbnailSize), this->mThumbnailSize);
    scene.addItem(widget);
    flowLayout->insertItem(pos, widget);
}

void FolderGridView::removeItemFromLayout(int pos) {
    flowLayout->removeAt(pos);
}

void FolderGridView::updateLayout() {
    shiftedCol = -1;
    flowLayout->invalidate();
    flowLayout->activate();
}

// block native tab-switching so we can use it in shortcuts
bool FolderGridView::focusNextPrevChild(bool) {
    return false;
}

void FolderGridView::keyPressEvent(QKeyEvent *event) {
    // The search owns the keyboard while it is active, so nothing below runs -
    // not the selection keys, and not the shortcuts in actionManager.
    if(handleSearchKey(event))
        return;

    ThumbnailView::keyPressEvent(event);
    event->accept();

    // Enter, Backspace, Ctrl+A and "/" are ordinary bindings (openSelected,
    // goUp, selectAll, startSearch) and reach them through processEvent below
    if(event->modifiers() & Qt::ControlModifier) {
        actionManager->processEvent(event);
        return;
    }

    // navigation keys run the selection action of their plain key; Shift extends; numpad counts
    const QString navigationKey = navigationKeyName(event->key());
    if(!navigationKey.isEmpty()) {
        const QString action = actionManager->actionForShortcut(MODE_FOLDERVIEW, navigationKey);
        if(isNavigationAction(action)) {
            actionManager->invokeAction(action);
            return;
        }
    }
    if(actionManager->processEvent(event))
        return;
    // An unbound modified key falls back to its plain key's selection action,
    // so Shift extends the selection on any key bound to one.
    const QString action = actionManager->actionForShortcut(MODE_FOLDERVIEW, plainKeyName(event));
    if(isNavigationAction(action))
        actionManager->invokeAction(action);
}

void FolderGridView::navigate(GridNavigation direction) {
    switch(direction) {
    case GridNavigation::Previous: selectPrev(); break;
    case GridNavigation::Next:     selectNext(); break;
    case GridNavigation::Above:    selectAbove(); break;
    case GridNavigation::Below:    selectBelow(); break;
    case GridNavigation::PageUp:   pageUp(); break;
    case GridNavigation::PageDown: pageDown(); break;
    case GridNavigation::First:    selectFirst(); break;
    case GridNavigation::Last:     selectLast(); break;
    }
}

// a modified gesture without its own binding uses the plain one
namespace {
QString gridMouseAction(const QString &name, const QString &plainName) {
    const QString action = actionManager->actionForShortcut(MODE_FOLDERVIEW, name);
    return action.isEmpty() ? actionManager->actionForShortcut(MODE_FOLDERVIEW, plainName) : action;
}
} // namespace

// tile double-click runs LMB_TileDoubleClick; empty space gets LMB_DoubleClick
void FolderGridView::itemDoubleClicked(int index, QMouseEvent *event) {
    QString name = ShortcutBuilder::fromEvent(event);
    name.replace(QStringLiteral("_DoubleClick"), QStringLiteral("_TileDoubleClick"));
    const QString action = gridMouseAction(name, QStringLiteral("LMB_TileDoubleClick"));
    if(action == QLatin1String("openSelected"))
        emit itemActivated(index);
    else if(!action.isEmpty())
        actionManager->invokeAction(action);
}

// Dragging the selection runs the grid's LMB_Drag binding: dragFileOut by
// default, which drags the selected files out of the app.
void FolderGridView::selectionDragged(QMouseEvent *event) {
    QMouseEvent press(QEvent::MouseButtonPress, event->position(), event->globalPosition(),
                      Qt::LeftButton, Qt::LeftButton, event->modifiers());
    if(gridMouseAction(ShortcutBuilder::fromDrag(&press), QStringLiteral("LMB_Drag")) == QLatin1String("dragFileOut"))
        emit draggedOut();
}

void FolderGridView::wheelEvent(QWheelEvent *event) {
    // Ctrl+WheelUp/Ctrl+WheelDown run their grid binding (zoomIn/zoomOut, which
    // change the thumbnail size, by default)
    if(event->modifiers().testFlag(Qt::ControlModifier)) {
        event->accept();
        actionManager->processEvent(event);
    } else {
        event->accept();
        int pixelDelta = event->pixelDelta().y();
        if(pixelDelta) {
            scrollPrecise(pixelDelta);
            return;
        }

        int angleDelta = event->angleDelta().y();
        if(!angleDelta)
            return;
        if(settings->enableSmoothScroll())
            scrollSmooth(angleDelta, 0.85, 1.15, true);
        else
            scrollPrecise(angleDelta);
    }
}

void FolderGridView::startRename(const QString& name) {
    int idx = lastSelected();
    if(!checkRange(idx))
        return;
    if(!renameEditor) {
        renameEditor = new QLineEdit(viewport());
        renameEditor->setContextMenuPolicy(Qt::NoContextMenu);
        const auto &cs = settings->colorScheme();
        renameEditor->setStyleSheet(QStringLiteral(
            "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 2px; padding: 1px 3px; }")
            .arg(cs.widget.name(), cs.text.name(), cs.accent.name()));
        renameEditor->installEventFilter(this);
    }
    renameIndex = idx;
    renameEditor->setText(name);
    // preselect the base name, leaving the extension untouched
    int dot = name.lastIndexOf('.');
    if(dot <= 0)
        dot = name.length();
    renameEditor->setSelection(0, dot);
    positionRenameEditor();
    renameEditor->show();
    renameEditor->raise();
    renameEditor->setFocus(Qt::OtherFocusReason);
}

void FolderGridView::positionRenameEditor() {
    if(!renameEditor || !checkRange(renameIndex))
        return;
    ThumbnailWidget *thumb = thumbnails.at(renameIndex);
    QRect rect = mapFromScene(thumb->mapToScene(thumb->labelGeometry())).boundingRect();
    int minH = renameEditor->minimumSizeHint().height();
    if(rect.height() < minH) {
        int cy = rect.center().y();
        rect.setTop(cy - minH / 2);
        rect.setHeight(minH);
    }
    renameEditor->setGeometry(rect);
}

void FolderGridView::commitRename() {
    if(!renameEditor || renameIndex < 0)
        return;
    QString newName = renameEditor->text().trimmed();
    renameIndex = -1; // clear before hide() so the focus-out filter is a no-op
    renameEditor->hide();
    setFocus();
    if(!newName.isEmpty())
        emit renameRequested(newName);
}

void FolderGridView::cancelRename() {
    if(!renameEditor || renameIndex < 0)
        return;
    renameIndex = -1;
    renameEditor->hide();
    setFocus();
}

bool FolderGridView::searchMode() const {
    return mSearchMode;
}

QString FolderGridView::searchQuery() const {
    return mSearchQuery;
}

void FolderGridView::enterSearchMode() {
    mSearchMode = true;
    mSearchQuery.clear();
    mSearchMatched = true;
    updateSearchIndicator();
}

void FolderGridView::startSearch() {
    if(thumbnails.count())
        enterSearchMode();
}

void FolderGridView::exitSearchMode() {
    if(!mSearchMode)
        return;
    mSearchMode = false;
    mSearchQuery.clear();
    mSearchMatched = true;
    if(searchIndicator)
        searchIndicator->hide();
}

void FolderGridView::setSearchMatched(bool matched) {
    if(!mSearchMode || mSearchMatched == matched)
        return;
    mSearchMatched = matched;
    updateSearchIndicator();
}

bool FolderGridView::handleSearchKey(QKeyEvent *event) {
    if(!mSearchMode)
        return false;
    event->accept();

    switch(event->key()) {
    case Qt::Key_Escape:
        exitSearchMode();
        return true;
    case Qt::Key_Backspace:
        // backspacing past the first character deletes the leading `/` as well,
        // which is what leaves search mode
        if(mSearchQuery.isEmpty()) {
            exitSearchMode();
            return true;
        }
        mSearchQuery.chop(1);
        mSearchMatched = true;
        updateSearchIndicator();
        if(!mSearchQuery.isEmpty())
            emit searchQueryChanged(mSearchQuery);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        exitSearchMode();
        emit itemActivated(lastSelected());
        return true;
    default:
        break;
    }

    // Ctrl+Alt may be AltGr text; other modifier combos and F-keys are shortcuts:
    // global ones still run mid-search, grid ones don't
    bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
    bool alt  = event->modifiers().testFlag(Qt::AltModifier);
    bool functionKey = event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35;
    if(functionKey || event->modifiers().testFlag(Qt::MetaModifier) || (ctrl != alt)) {
        const QString shortcut = ShortcutBuilder::fromEvent(event);
        const QString action = actionManager->actionForShortcut(MODE_GLOBAL, shortcut);
        if(!action.isEmpty() && actionManager->actionForShortcut(MODE_FOLDERVIEW, shortcut) == action)
            actionManager->invokeAction(action);
        return true;
    }

    QString text = event->text();
    if(!text.isEmpty() && text.at(0).isPrint()) {
        mSearchQuery.append(text);
        updateSearchIndicator();
        emit searchQueryChanged(mSearchQuery);
    }
    // anything else (arrows, other non-text keys) is swallowed
    return true;
}

void FolderGridView::updateSearchIndicator() {
    if(!mSearchMode)
        return;
    if(!searchIndicator) {
        searchIndicator = new QLabel(viewport());
        searchIndicator->setObjectName(QStringLiteral("gridSearchIndicator"));
        searchIndicator->setTextInteractionFlags(Qt::NoTextInteraction);
        searchIndicator->setFocusPolicy(Qt::NoFocus);
    }
    const auto &cs = settings->colorScheme();
    // dim red marks a prefix that matches nothing; the query is kept either way
    // so the next backspace gets back to a matching one
    QString textColor = mSearchMatched ? cs.text.name() : QStringLiteral("#e05252");
    searchIndicator->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 2px; padding: 2px 6px; }")
        .arg(cs.widget.name(), textColor, cs.accent.name()));
    searchIndicator->setText(QStringLiteral("/") + mSearchQuery);
    positionSearchIndicator();
    searchIndicator->show();
    searchIndicator->raise();
}

void FolderGridView::positionSearchIndicator() {
    if(!searchIndicator)
        return;
    QSize size = searchIndicator->sizeHint();
    constexpr int margin = 6;
    searchIndicator->setGeometry(margin,
                                 viewport()->height() - size.height() - margin,
                                 size.width(), size.height());
}

void FolderGridView::focusOutEvent(QFocusEvent *event) {
    // a popup (context menu) stealing focus is not navigating away
    if(event->reason() != Qt::PopupFocusReason && event->reason() != Qt::ActiveWindowFocusReason)
        exitSearchMode();
    ThumbnailView::focusOutEvent(event);
}

void FolderGridView::hideEvent(QHideEvent *event) {
    exitSearchMode();
    ThumbnailView::hideEvent(event);
}

bool FolderGridView::eventFilter(QObject *o, QEvent *ev) {
    if(renameEditor && o == renameEditor) {
        if(ev->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent*>(ev);
            if(ke->key() == Qt::Key_Escape) {
                cancelRename();
                return true;
            }
            if(ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                commitRename();
                return true;
            }
        } else if(ev->type() == QEvent::FocusOut) {
            cancelRename();
        }
    }
    return ThumbnailView::eventFilter(o, ev);
}

void FolderGridView::contextMenuEvent(QContextMenuEvent *event) {
    // a right-press already selected the item under the cursor (see ThumbnailView).
    // The view toggles are always available; file-op entries are gated on the selection.
    contextMenu->setSelectionInfo(selectionInfo());
    contextMenu->showAt(event->globalPos());
    event->accept();
}

void FolderGridView::zoomIn() {
    setThumbnailSize(this->mThumbnailSize + ZOOM_STEP);
}

void FolderGridView::zoomOut() {
    setThumbnailSize(this->mThumbnailSize - ZOOM_STEP);
}

void FolderGridView::setThumbnailSize(int newSize) {
    newSize = std::clamp(newSize, THUMBNAIL_SIZE_MIN, THUMBNAIL_SIZE_MAX);
    mThumbnailSize = newSize;
    for(int i = 0; i < thumbnails.count(); i++) {
        thumbnails.at(i)->setThumbnailSize(newSize);
        thumbnails.at(i)->setThumbnailAreaSize(folderGridThumbnailWidth(newSize), newSize);
    }
    updateLayout();
    fitSceneToContents();
    if(lastSelected() != -1)
        ensureVisible(thumbnails.at(lastSelected()), 0, 40);
    emit thumbnailSizeChanged(mThumbnailSize);
    loadVisibleThumbnails();
}

void FolderGridView::fitSceneToContents() {
    if(scrollBar->isVisible())
        holderWidget.setGeometry(0,0, width() - scrollBar->width(), height());
    else
        holderWidget.setGeometry(0,0, width(), height());
    ThumbnailView::fitSceneToContents();
}

void FolderGridView::resizeEvent(QResizeEvent *event) {
    if(this->isVisible()) {
        ThumbnailView::resizeEvent(event);
        fitSceneToContents();
        loadVisibleThumbnailsDelayed();
        if(renameIndex >= 0)
            positionRenameEditor();
        if(mSearchMode)
            positionSearchIndicator();
    }
}
