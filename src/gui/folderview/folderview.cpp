#include "folderview.h"

#include <utility>
#include "ui_folderview.h"

#include "constants.h"
#include "platform/platformdesktop.h"
#include "utils/newwindow.h"

namespace {
QDir::Filters treeFilter(bool showHidden) {
    return QDir::NoDotAndDotDot | QDir::AllDirs | (showHidden ? QDir::Hidden : QDir::Filters());
}

// string only, never the disk: "/a/b" -> "/a", "/a" -> "/", "C:/a" -> "C:/"
QString parentFolder(const QString &path) {
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    const bool topLevel = slash <= 0 || (slash == 2 && path.at(1) == QLatin1Char(':'));
    return topLevel ? path.left(slash + 1) : path.left(slash);
}
}

FolderView::FolderView(QWidget *parent) :
    FloatingWidgetContainer(parent),
    ui(new Ui::FolderView)
{
    ui->setupUi(this);

    // ------- filesystem view --------

    optionsPopup = new FVOptionsPopup();
    popupTimerClutch.start();

    dirModel = new FileSystemModelCustom(this);
    // before the root path is set, so the first listing already honours the setting
    mShowHidden = settings->showHiddenFiles();
    dirModel->setFilter(treeFilter(mShowHidden));
    ui->dirTreeView->setModel(dirModel);

    QHeaderView* header = ui->dirTreeView->header();
    header->hideSection(1); // size
    header->hideSection(2); // type
    header->hideSection(3); // mod date

    // restored as the model lists each parent, see onTreeDirectoryLoaded()
    for(const QString &saved : settings->treeExpandedPaths()) {
        const QString path = QDir::cleanPath(saved);
        if(!QDir::isAbsolutePath(path) || path.endsWith(QLatin1Char('/')))
            continue; // relative, or a root
        mRestoreFolders[parentFolder(path)] << path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
    }
    dirModel->setRootPath(PlatformDesktop::folderViewInitialRootPath());
    if(!dirModel->rootPath().isEmpty()) {
        QModelIndex idx = dirModel->index(dirModel->rootPath());
        ui->dirTreeView->setRootIndex(idx);
    }
    // -------------------------------
    ui->upButton->setAction("goUp");
    ui->upButton->setIconPath(":res/icons/common/buttons/panel/up16.png");
    ui->upButton->setTriggerMode(TriggerMode::ClickTrigger);
    ui->settingsButton->setAction("openSettings");
    ui->settingsButton->setIconPath(":res/icons/common/buttons/panel/settings20.png");
    ui->exitButton->setAction("exit");
    ui->exitButton->setIconPath(":res/icons/common/buttons/panel/close16.png");
    ui->exitButton->setIconOffset(-1, 0);
    ui->docViewButton->setAction("documentView");
    ui->docViewButton->setIconPath(":res/icons/common/buttons/panel/document-view20.png");
    ui->togglePlacesPanelButton->setCheckable(true);
    ui->togglePlacesPanelButton->setIconPath(":res/icons/common/buttons/panel/toggle-panel20.png");
    ui->togglePlacesPanelButton->setIconOffset(1, 0);

    ui->optionsPopupButton->setCheckable(true);
    ui->optionsPopupButton->setIconPath(":res/icons/common/buttons/panel/folderview20.png");

    ui->sortingComboBox->setIconPath(":res/icons/common/other/sorting-mode16.png");

    ui->newBookmarkButton->setIconPath(":res/icons/common/buttons/panel-small/add-new12.png");
    ui->homeButton->setIconPath(":res/icons/common/buttons/panel-small/home12.png");
    ui->rootButton->setIconPath(":res/icons/common/buttons/panel-small/root12.png");

    ui->upButton->setToolTipLabel(tr("Go to parent folder"), "goUp", MODE_FOLDERVIEW);
    ui->settingsButton->setToolTipLabel(tr("Settings"), "openSettings", MODE_FOLDERVIEW);
    ui->exitButton->setToolTipLabel(tr("Exit"), "exit", MODE_FOLDERVIEW);
    ui->docViewButton->setToolTipLabel(tr("Show document view"), "documentView", MODE_FOLDERVIEW);
    ui->togglePlacesPanelButton->setToolTipLabel(tr("Show / hide places panel"), "togglePlacesPanel", MODE_FOLDERVIEW);
    ui->optionsPopupButton->setToolTip(tr("Change view options"));
    ui->sortingComboBox->setToolTip(tr("Change sort order"));
    ui->zoomSlider->setToolTip(tr("Change thumbnail size"));
    ui->homeButton->setToolTip(tr("Go to home folder"));
    ui->rootButton->setToolTip(tr("Go to root folder"));
    ui->bookmarksLabel->setToolTip(tr("Show / hide bookmarks"));
    ui->directoriesLabel->setToolTip(tr("Show / hide folder tree"));

    int min = ui->thumbnailGrid->THUMBNAIL_SIZE_MIN;
    int max = ui->thumbnailGrid->THUMBNAIL_SIZE_MAX;
    int step = ui->thumbnailGrid->ZOOM_STEP;

    ui->zoomSlider->setMinimum(min / step);
    ui->zoomSlider->setMaximum(max / step);
    ui->zoomSlider->setSingleStep(1);
    ui->zoomSlider->setPageStep(1);

    ui->splitter->setStretchFactor(1, 50);

    connect(ui->thumbnailGrid, &FolderGridView::thumbnailsRequested,  this, &FolderView::thumbnailsRequested);
    connect(ui->thumbnailGrid, &FolderGridView::thumbnailSizeChanged, this, &FolderView::onThumbnailSizeChanged);
    connect(ui->thumbnailGrid, &FolderGridView::itemActivated,   this, &FolderView::itemActivated);
    connect(ui->thumbnailGrid, &FolderGridView::draggedOut,      this, &FolderView::draggedOut);
    connect(ui->thumbnailGrid, &FolderGridView::draggedOver,     this, &FolderView::draggedOver);
    connect(ui->thumbnailGrid, &FolderGridView::droppedInto,     this, &FolderView::droppedInto);
    connect(ui->thumbnailGrid, &FolderGridView::selectionChanged, this, &FolderView::selectionChanged);
    connect(ui->thumbnailGrid, &FolderGridView::convertFormatRequested, this, &FolderView::convertFormatRequested);
    connect(ui->thumbnailGrid, &FolderGridView::renameRequested, this, &FolderView::renameRequested);
    connect(ui->thumbnailGrid, &FolderGridView::searchQueryChanged, this, &FolderView::searchQueryChanged);

    connect(ui->bookmarksWidget, &BookmarksWidget::bookmarkClicked, this, &FolderView::onBookmarkClicked);

    connect(ui->newBookmarkButton, &IconButton::clicked, this, &FolderView::bookmarkCurrentFolder);
    connect(ui->bookmarksWidget, &BookmarksWidget::bookmarksChanged, this, &FolderView::updateBookmarkButton);
    connect(ui->bookmarksWidget, &BookmarksWidget::contextMenuRequested, this, &FolderView::onBookmarkMenuRequested);
    ui->bookmarksLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->bookmarksLabel, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        onBookmarkMenuRequested(QString(), ui->bookmarksLabel->mapToGlobal(pos));
    });
    updateBookmarkButton();
    connect(ui->homeButton, &IconButton::clicked, this, &FolderView::onHomeBtn);
    connect(ui->rootButton, &IconButton::clicked, this, &FolderView::onRootBtn);

    connect(ui->zoomSlider, &QSlider::valueChanged, this, &FolderView::onZoomSliderValueChanged);
    connect(ui->sortingComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &FolderView::onSortingSelected);
    connect(ui->togglePlacesPanelButton, &ActionButton::toggled, this, &FolderView::onPlacesPanelButtonChecked);

    connect(ui->optionsPopupButton, &IconButton::toggled, this, &FolderView::onOptionsPopupButtonToggled);
    connect(optionsPopup, &FVOptionsPopup::dismissed, this, &FolderView::onOptionsPopupDismissed);
    connect(optionsPopup, &FVOptionsPopup::viewModeSelected, this, &FolderView::onViewModeSelected);

    connect(ui->dirTreeView, &TreeViewCustom::droppedIn, this, &FolderView::onDroppedInByIndex);
    connect(ui->dirTreeView, &TreeViewCustom::renameCommitted, this, &FolderView::onTreeRenameCommitted);
    connect(ui->dirTreeView, &TreeViewCustom::newFolderCommitted, this, &FolderView::onTreeNewFolderCommitted);
    connect(ui->dirTreeView, &TreeViewCustom::middleClicked, this, &FolderView::onTreeMiddleClicked);
    connect(ui->dirTreeView, &TreeViewCustom::contextMenuRequestedAt, this, &FolderView::onTreeContextMenuRequested);
    connect(ui->dirTreeView, &TreeViewCustom::renameKeyPressed, this, &FolderView::onTreeRenameKey);
    connect(ui->dirTreeView, &TreeViewCustom::trashKeyPressed, this, &FolderView::onTreeTrashKey);
    connect(ui->dirTreeView, &TreeViewCustom::leaveRequested, this, [this]() {
        ui->thumbnailGrid->setFocus(Qt::ShortcutFocusReason);
    });
    connect(dirModel, &QFileSystemModel::directoryLoaded, this, &FolderView::onTreeDirectoryLoaded);
    connect(ui->dirTreeView, &QTreeView::expanded, this, &FolderView::onTreeExpanded);
    connect(ui->dirTreeView, &QTreeView::collapsed, this, &FolderView::onTreeCollapsed);
    connect(dirModel, &FileSystemModelCustom::folderIconChanged, ui->dirTreeView->viewport(),
            qOverload<>(&QWidget::update));
    connect(ui->bookmarksWidget, &BookmarksWidget::droppedIn, this,
            [this](const QStringList &paths, const QString &dirPath, bool copy) {
        if(copy)
            emit copyUrlsRequested(paths, dirPath);
        else
            emit moveUrlsRequested(paths, dirPath);
    });

    ui->sortingComboBox->setItemDelegate(new QStyledItemDelegate(ui->sortingComboBox));
    ui->sortingComboBox->view()->setTextElideMode(Qt::ElideNone);

    connect(ui->splitter, &QSplitter::splitterMoved, this, &FolderView::onSplitterMoved);

    readSettings();

    QSizePolicy sp_retain = sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    setSizePolicy(sp_retain);
    connect(settings, &Settings::settingsChanged, this, &FolderView::readSettings);

    hide();
}

void FolderView::readSettings() {
    ui->thumbnailGrid->setThumbnailSize(settings->folderViewIconSize());
    ui->thumbnailGrid->setShowLabels((settings->folderViewMode() != FV_SIMPLE));
    ui->thumbnailGrid->setShowInfo(settings->folderViewShowInfo());
    ui->thumbnailGrid->setLabelFontPointSize(settings->folderViewFontPointSize());
    ui->thumbnailGrid->setLabelBackgroundColor(settings->folderViewLabelBackgroundColor());
    ui->thumbnailGrid->setCellBackgroundColor(settings->folderViewCellBackgroundColor());
    ui->topBar->setVisible(settings->folderViewTopBar());

    // setFilter re-lists every loaded folder, so only a changed value may reach it
    const bool showHidden = settings->showHiddenFiles();
    if(showHidden != mShowHidden) {
        mShowHidden = showHidden;
        dirModel->setFilter(treeFilter(showHidden));
    }

    // only stored values that changed: an unrelated setting must not reset the panel
    const bool placesPanel = settings->placesPanel();
    if(!mPanelApplied || placesPanel != mPlacesPanel) {
        mPlacesPanel = placesPanel;
        ui->togglePlacesPanelButton->setChecked(placesPanel);
        setPlacesPanel(placesPanel);
    }
    const bool bookmarksExpanded = settings->placesPanelBookmarksExpanded();
    if(!mPanelApplied || bookmarksExpanded != mBookmarksExpanded) {
        mBookmarksExpanded = bookmarksExpanded;
        ui->bookmarksWidget->setVisible(bookmarksExpanded);
    }
    const bool treeExpanded = settings->placesPanelTreeExpanded();
    if(!mPanelApplied || treeExpanded != mTreeExpanded) {
        mTreeExpanded = treeExpanded;
        ui->dirTreeView->setVisible(treeExpanded);
    }
    const int panelWidth = settings->placesPanelWidth();
    if(!mPanelApplied || panelWidth != mPanelWidth) {
        mPanelWidth = panelWidth;
        ui->splitter->setSizes({panelWidth, 1});
    }
    mPanelApplied = true;
}

void FolderView::onSplitterMoved() {
    mPanelWidth = ui->placesPanel->width();
    settings->setPlacesPanelWidth(mPanelWidth);
}

void FolderView::onPlacesPanelButtonChecked(bool mode) {
    setPlacesPanel(mode);
    mPlacesPanel = mode;
    settings->setPlacesPanel(mode);
}

void FolderView::setPlacesPanel(bool mode) {
    if(width() >= 600)
        ui->placesPanel->setVisible(mode);
}

void FolderView::toggleBookmarks() {
    if(ui->bookmarksWidget->isVisible())
        ui->bookmarksWidget->hide();
    else
        ui->bookmarksWidget->show();
    mBookmarksExpanded = ui->bookmarksWidget->isVisible();
    settings->setPlacesPanelBookmarksExpanded(mBookmarksExpanded);
}

void FolderView::toggleFilesystemView() {
    if(ui->dirTreeView->isVisible())
        ui->dirTreeView->hide();
    else
        ui->dirTreeView->show();
    mTreeExpanded = ui->dirTreeView->isVisible();
    settings->setPlacesPanelTreeExpanded(mTreeExpanded);
}

// both ask for confirmation in FileOperationsController, like Copy to / Move to
void FolderView::onDroppedInByIndex(const QStringList &paths, const QModelIndex &index, bool copy) {
    if(copy)
        emit copyUrlsRequested(paths, dirModel->filePath(index));
    else
        emit moveUrlsRequested(paths, dirModel->filePath(index));
}

void FolderView::onTreeContextMenuRequested(const QModelIndex &index, const QPoint &globalPos) {
    if(!mTreeMenu) {
        mTreeMenu = new TreeContextMenu(this);
        connect(mTreeMenu, &TreeContextMenu::openRequested, this, &FolderView::directorySelected);
        connect(mTreeMenu, &TreeContextMenu::newWindowRequested, this, &NewWindow::open);
        connect(mTreeMenu, &TreeContextMenu::copyPathRequested, this, &FolderView::copyFolderPath);
        connect(mTreeMenu, &TreeContextMenu::collapseAllRequested, this, &FolderView::collapseTree);
        connect(mTreeMenu, &TreeContextMenu::newFolderRequested, this, [this](const QString &path) {
            ui->dirTreeView->startNewFolder(dirModel->index(path), tr("New folder"));
        });
        connect(mTreeMenu, &TreeContextMenu::renameRequested, this, [this](const QString &path) {
            const QModelIndex index = dirModel->index(path);
            ui->dirTreeView->startRename(index, dirModel->fileName(index));
        });
        connect(mTreeMenu, &TreeContextMenu::trashRequested, this, [this](const QString &path) {
            emit folderOperationRequested(TreeFolderOp::Trash, path, QString());
        });
        connect(mTreeMenu, &TreeContextMenu::deleteRequested, this, [this](const QString &path) {
            emit folderOperationRequested(TreeFolderOp::Delete, path, QString());
        });
        connect(mTreeMenu, &TreeContextMenu::addBookmarkRequested, ui->bookmarksWidget, &BookmarksWidget::addBookmark);
    }
    const QString path = dirModel->filePath(index);
    mTreeMenu->openFor({path, ui->bookmarksWidget->hasBookmark(path), isProtectedTreeFolder(path)}, globalPos);
}

void FolderView::copyFolderPath(const QString &path) {
    emit folderOperationRequested(TreeFolderOp::CopyPath, QDir::toNativeSeparators(path), QString());
}

// path is empty for the panel itself; the menu opens on first use
void FolderView::onBookmarkMenuRequested(const QString &path, const QPoint &globalPos) {
    if(!mBookmarkMenu) {
        mBookmarkMenu = new BookmarkContextMenu(this);
        connect(mBookmarkMenu, &BookmarkContextMenu::openRequested, this, &FolderView::directorySelected);
        connect(mBookmarkMenu, &BookmarkContextMenu::newWindowRequested, this, &NewWindow::open);
        connect(mBookmarkMenu, &BookmarkContextMenu::renameRequested, ui->bookmarksWidget,
                &BookmarksWidget::startRename);
        connect(mBookmarkMenu, &BookmarkContextMenu::copyPathRequested, this, &FolderView::copyFolderPath);
        connect(mBookmarkMenu, &BookmarkContextMenu::removeRequested, ui->bookmarksWidget,
                &BookmarksWidget::removeBookmark);
        connect(mBookmarkMenu, &BookmarkContextMenu::addFolderRequested, this, &FolderView::chooseBookmarkFolder);
        connect(mBookmarkMenu, &BookmarkContextMenu::bookmarkHereRequested, this,
                &FolderView::bookmarkCurrentFolder);
    }
    if(path.isEmpty())
        mBookmarkMenu->openForPanel(ui->newBookmarkButton->isEnabled(), globalPos);
    else
        mBookmarkMenu->openForBookmark(path, globalPos);
}

// the same guards as the menu's Rename and Move to trash: home and the root stay put
void FolderView::onTreeRenameKey(const QModelIndex &index) {
    const QString path = dirModel->filePath(index);
    if(!isProtectedTreeFolder(path))
        ui->dirTreeView->startRename(index, dirModel->fileName(index));
}

void FolderView::onTreeTrashKey(const QModelIndex &index) {
    const QString path = dirModel->filePath(index);
    if(!isProtectedTreeFolder(path))
        emit folderOperationRequested(TreeFolderOp::Trash, path, QString());
}

// shows the places panel and the tree section first, as their own toggles do
void FolderView::toggleTreeFocus() {
    TreeViewCustom *tree = ui->dirTreeView;
    if(tree->hasFocus()) {
        ui->thumbnailGrid->setFocus(Qt::ShortcutFocusReason);
        return;
    }
    if(!ui->placesPanel->isVisible())
        ui->togglePlacesPanelButton->setChecked(true);
    if(!tree->isVisible() && ui->placesPanel->isVisible())
        toggleFilesystemView();
    if(!tree->isVisible())
        return; // too narrow for the panel
    if(mTreeScrollTarget.isValid() && mTreeScrollTarget != tree->rootIndex()) {
        tree->setCurrentIndex(mTreeScrollTarget);
        tree->scrollTo(mTreeScrollTarget);
    }
    tree->setFocus(Qt::ShortcutFocusReason);
}

void FolderView::onTreeMiddleClicked(const QModelIndex &index) {
    NewWindow::open(dirModel->filePath(index));
}

// everything folds except the path down to the current folder, which scrollTo() re-expands
void FolderView::collapseTree() {
    // nothing from the last session comes back after this
    mRestoreFolders.clear();
    mExpandedFolders.clear();
    ui->dirTreeView->collapseAll();
    const QModelIndex current = ui->dirTreeView->currentIndex();
    if(current.isValid())
        ui->dirTreeView->scrollTo(current);
}

void FolderView::onTreeRenameCommitted(const QModelIndex &index, const QString &name) {
    emit folderOperationRequested(TreeFolderOp::Rename, dirModel->filePath(index), name);
}

// Core creates the folder; the row is picked once the watcher has made the model list it
void FolderView::onTreeNewFolderCommitted(const QModelIndex &parent, const QString &name) {
    const QString parentPath = dirModel->filePath(parent);
    mPendingNewFolder = QDir::cleanPath(parentPath + QLatin1Char('/') + name);
    if(!mPendingConnection)
        mPendingConnection = connect(dirModel, &QAbstractItemModel::rowsInserted, this, &FolderView::onTreeRowsInserted);
    emit folderOperationRequested(TreeFolderOp::NewFolder, parentPath, name);
}

void FolderView::onTreeRowsInserted(const QModelIndex &parent, int first, int last) {
    if(mPendingNewFolder.isEmpty()) {
        disconnect(mPendingConnection);
        return;
    }
    for(int row = first; row <= last; ++row) {
        if(dirModel->filePath(dirModel->index(row, 0, parent)) != mPendingNewFolder)
            continue;
        disconnect(mPendingConnection);
        // the model sorts its new rows on a queued call: pick the row after that
        QMetaObject::invokeMethod(this, &FolderView::showNewFolder, Qt::QueuedConnection);
        return;
    }
}

void FolderView::showNewFolder() {
    const QModelIndex index = dirModel->index(mPendingNewFolder);
    mPendingNewFolder.clear();
    if(!index.isValid())
        return;
    ui->dirTreeView->setCurrentIndex(index);
    ui->dirTreeView->scrollTo(index);
}

void FolderView::onOptionsPopupButtonToggled(bool mode) {
    if(mode) {
        // on Windows the popup reopened right after dismissing it via this button
        if(popupTimerClutch.elapsed() < 10) {
            ui->optionsPopupButton->setChecked(false);
            return;
        }
        QPoint pos = ui->optionsPopupButton->geometry().bottomRight() -
                     QPoint(optionsPopup->width(), 0);
        optionsPopup->showAt(mapToGlobal(pos));
    }
}

void FolderView::onOptionsPopupDismissed() {
    popupTimerClutch.start();
    ui->optionsPopupButton->setChecked(false);
}

void FolderView::onViewModeSelected(FolderViewMode mode) {
    settings->setFolderViewMode(mode);
    ui->thumbnailGrid->setShowLabels((settings->folderViewMode() != FV_SIMPLE));
    emit showFoldersChanged((mode == FV_EXT_FOLDERS));
}

void FolderView::onThumbnailSizeChanged(int newSize) {
    ui->zoomSlider->setValue(newSize / ui->thumbnailGrid->ZOOM_STEP);
    settings->setFolderViewIconSize(newSize);
}

void FolderView::onZoomSliderValueChanged(int value) {
    ui->thumbnailGrid->setThumbnailSize(value * ui->thumbnailGrid->ZOOM_STEP);
}

// changed by user via combobox
void FolderView::onSortingSelected(int mode) {
    emit sortingSelected(static_cast<SortingMode>(mode));
}

void FolderView::onSortingChanged(SortingMode mode) {
    ui->sortingComboBox->blockSignals(true);
    ui->sortingComboBox->setCurrentIndex(static_cast<int>(mode));
    ui->sortingComboBox->blockSignals(false);
}

FolderView::~FolderView() {
    delete ui;
}

// probably unneeded
void FolderView::show() {
    QWidget::show();
    ui->thumbnailGrid->setFocus();
}

// probably unneeded
void FolderView::hide() {
    QWidget::hide();
    ui->thumbnailGrid->clearFocus();
}

void FolderView::onFullscreenModeChanged(bool mode) {
    ui->exitButton->setHidden(!mode);
    if(mode) // hide 2px spacer
        ui->panelRightEdgeSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    else // show spacer
        ui->panelRightEdgeSpacer->changeSize(2, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->topBar->layout()->invalidate();
}

// a click on empty panel space or window activation lands on the grid; Tab passes on into the panel
void FolderView::focusInEvent(QFocusEvent *event) {
    if(event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason) {
        focusNextPrevChild(event->reason() == Qt::TabFocusReason);
        return;
    }
    ui->thumbnailGrid->setFocus();
}

void FolderView::populate(int count) {
    ui->thumbnailGrid->populate(count);
}

void FolderView::setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) {
    ui->thumbnailGrid->setThumbnail(pos, thumb);
}

void FolderView::select(QList<int> indices) {
    ui->thumbnailGrid->select(indices);
}

void FolderView::select(int index) {
    ui->thumbnailGrid->select(index);
}

QList<int> FolderView::selection() {
    return ui->thumbnailGrid->selection();
}

void FolderView::startRename(const QString& name) {
    ui->thumbnailGrid->startRename(name);
}

void FolderView::startSearch() {
    ui->thumbnailGrid->startSearch();
}

void FolderView::navigate(GridNavigation direction) {
    ui->thumbnailGrid->navigate(direction);
}

void FolderView::zoomThumbnails(int steps) {
    for(; steps > 0; --steps)
        ui->thumbnailGrid->zoomIn();
    for(; steps < 0; ++steps)
        ui->thumbnailGrid->zoomOut();
}

void FolderView::setSearchMatched(bool matched) {
    ui->thumbnailGrid->setSearchMatched(matched);
}

void FolderView::focusOn(int index) {
    ui->thumbnailGrid->focusOn(index);
}

void FolderView::focusOnSelection() {
    ui->thumbnailGrid->focusOnSelection();
}

void FolderView::onHomeBtn() {
    emit directorySelected(QDir::homePath());
}

void FolderView::onRootBtn() {
    emit directorySelected("/");
}

void FolderView::setDirectoryPath(QString path) {
    const QString rootPath = PlatformDesktop::folderViewRootPathFor(path);
    if(!rootPath.isEmpty() && dirModel->rootPath() != rootPath) {
        dirModel->setRootPath(rootPath);
        QModelIndex idx = dirModel->index(dirModel->rootPath());
        ui->dirTreeView->setRootIndex(idx);
    }
    ui->pathLabel->setText(path);
    ui->bookmarksWidget->onPathChanged(path);
    mCurrentPath = path;
    updateBookmarkButton();

    // a tree click already made the row current
    const QString cleanPath = QDir::cleanPath(path);
    if(dirModel->filePath(ui->dirTreeView->currentIndex()) == cleanPath) {
        mTreeScrollTarget = ui->dirTreeView->currentIndex();
        return;
    }

    const QModelIndex targetIndex = dirModel->index(cleanPath);
    // ancestors not listed yet: rows they add shift the target, so scroll again once each loads
    mTreeScrollWaits.clear();
    const QModelIndex rootIndex = ui->dirTreeView->rootIndex();
    QModelIndex parent = targetIndex.parent();
    for(; parent.isValid() && parent != rootIndex; parent = parent.parent()) {
        if(dirModel->canFetchMore(parent))
            mTreeScrollWaits << dirModel->filePath(parent);
    }
    if(parent != rootIndex)
        mTreeScrollWaits.clear(); // not shown in the tree
    else if(dirModel->canFetchMore(rootIndex))
        mTreeScrollWaits << dirModel->filePath(rootIndex);
    mTreeScrollTarget = targetIndex;
    ui->dirTreeView->setCurrentIndex(targetIndex);
    // expands the ancestors; other expanded folders stay as the user left them
    ui->dirTreeView->scrollTo(targetIndex);
}

void FolderView::onTreeDirectoryLoaded(const QString &dirPath) {
    if(mTreeScrollWaits.removeOne(dirPath)) {
        // the model would sort the new rows on its next timer tick; sort now so the row is final
        dirModel->sort(0);
        fsTreeScrollToCurrent();
    }
    if(!mRestoreFolders.isEmpty())
        restoreExpandedIn(dirPath, true);
}

// looks only among the rows the model already lists; expanding lists the child in the gatherer thread
void FolderView::restoreExpandedIn(const QString &dir, bool listed) {
    auto it = mRestoreFolders.find(dir);
    if(it == mRestoreFolders.end())
        return;
    const QModelIndex parent = dirModel->index(dir);
    QStringList names = it.value();
    QList<QPersistentModelIndex> found;
    const int rows = parent.isValid() ? dirModel->rowCount(parent) : 0;
    for(int row = 0; row < rows && !names.isEmpty(); ++row) {
        const QModelIndex child = dirModel->index(row, 0, parent);
        if(names.removeOne(dirModel->fileName(child)))
            found << child;
    }
    // a fully listed folder lacks the rest: they were deleted or are filtered out
    if(listed || names.isEmpty())
        mRestoreFolders.erase(it);
    else
        it.value() = names;
    // the current row must not slide out of view when a folder above it fills
    TreeViewCustom *tree = ui->dirTreeView;
    const bool keepCurrent = !found.isEmpty() && mTreeScrollTarget.isValid() &&
                             tree->visualRect(mTreeScrollTarget).intersects(tree->viewport()->rect());
    for(const QPersistentModelIndex &child : std::as_const(found)) {
        if(!child.isValid())
            continue;
        tree->expand(child);
        const QString childPath = dirModel->filePath(child);
        if(keepCurrent && !mTreeScrollWaits.contains(childPath))
            mTreeScrollWaits << childPath;
        // may be listed already (e.g. by navigation), then no directoryLoaded comes for it
        if(!dirModel->canFetchMore(child))
            restoreExpandedIn(childPath, false);
    }
    if(keepCurrent)
        fsTreeScrollToCurrent();
}

void FolderView::onTreeExpanded(const QModelIndex &index) {
    mExpandedFolders.insert(dirModel->filePath(index));
}

void FolderView::onTreeCollapsed(const QModelIndex &index) {
    mExpandedFolders.remove(dirModel->filePath(index));
}

// shallowest first, then by path, so the cap keeps the folders nearest the root
void FolderView::saveTreeState() {
    QSet<QString> all = mExpandedFolders;
    for(auto it = mRestoreFolders.cbegin(); it != mRestoreFolders.cend(); ++it) {
        for(const QString &name : it.value())
            all.insert(it.key().endsWith(QLatin1Char('/')) ? it.key() + name : it.key() + QLatin1Char('/') + name);
    }
    // scrollTo() expands the folders above the root too; they are not rows of this tree
    const QString root = dirModel->rootPath();
    QList<std::pair<qsizetype, QString>> byDepth;
    byDepth.reserve(all.size());
    for(const QString &path : std::as_const(all)) {
        if(path == root || path.endsWith(QLatin1Char('/')) || root.startsWith(path + QLatin1Char('/')))
            continue;
        byDepth.append({path.count(QLatin1Char('/')), path});
    }
    std::sort(byDepth.begin(), byDepth.end());
    QStringList paths;
    for(const auto &entry : std::as_const(byDepth)) {
        if(paths.size() == tg::limit::kTreeExpandedMax)
            break;
        paths << entry.second;
    }
    settings->setTreeExpandedPaths(paths);
}

void FolderView::fsTreeScrollToCurrent() {
    if(mTreeScrollTarget.isValid())
        ui->dirTreeView->scrollTo(mTreeScrollTarget);
}

void FolderView::onTreeViewClicked(QModelIndex index) {
    emit directorySelected(dirModel->fileInfo(index).absoluteFilePath());
}

void FolderView::onBookmarkClicked(QString dirPath) {
    emit directorySelected(std::move(dirPath));
}

void FolderView::bookmarkCurrentFolder() {
    if(!mCurrentPath.isEmpty())
        ui->bookmarksWidget->addBookmark(QDir::cleanPath(mCurrentPath));
}

// the button bookmarks the folder you are in, so it rests once that is done
void FolderView::updateBookmarkButton() {
    const bool bookmarked = ui->bookmarksWidget->hasBookmark(mCurrentPath);
    ui->newBookmarkButton->setEnabled(!mCurrentPath.isEmpty() && !bookmarked);
    ui->newBookmarkButton->setToolTip(bookmarked ? tr("This folder is already bookmarked")
                                                 : tr("Bookmark this folder"));
}

void FolderView::chooseBookmarkFolder() {
    QFileDialog dialog;
    dialog.setDirectory(QDir::homePath());
    dialog.setWindowTitle(tr("Select folder"));
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);
    dialog.setOption(QFileDialog::DontResolveSymlinks);
    connect(&dialog, &QFileDialog::fileSelected, ui->bookmarksWidget, &BookmarksWidget::addBookmark);
    dialog.exec();
}

void FolderView::addItem() {
    ui->thumbnailGrid->addItem();
}

void FolderView::insertItem(int index) {
    ui->thumbnailGrid->insertItem(index);
}

void FolderView::removeItem(int index) {
    ui->thumbnailGrid->removeItem(index);
}

void FolderView::reloadItem(int index) {
    ui->thumbnailGrid->reloadItem(index);
}

void FolderView::setDragHover(int index) {
    ui->thumbnailGrid->setDragHover(index);
}

void FolderView::setSelectionInfoProvider(std::function<SelectionInfo()> provider) {
    ui->thumbnailGrid->setSelectionInfoProvider(std::move(provider));
}

// prevent passthrough to parent
void FolderView::wheelEvent(QWheelEvent *event) {
    event->accept();
}

void FolderView::paintEvent(QPaintEvent *) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void FolderView::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event)
    if(width() < 600)
        ui->placesPanel->setVisible(false);
    else if (ui->togglePlacesPanelButton->isChecked())
        ui->placesPanel->setVisible(true);

    if(width() < 510) {
        ui->zoomSlider->setVisible(false);
        ui->zoomSliderSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->pathbarSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->topBar->layout()->invalidate();
    } else {
        ui->zoomSlider->setVisible(true);
        ui->zoomSliderSpacer->changeSize(3, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->pathbarSpacer->changeSize(12, 20, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        ui->topBar->layout()->invalidate();
    }
}
