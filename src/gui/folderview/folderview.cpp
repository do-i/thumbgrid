#include "folderview.h"

#include <utility>
#include "ui_folderview.h"

#include "platform/platformdesktop.h"
#include "utils/newwindow.h"

namespace {
QDir::Filters treeFilter(bool showHidden) {
    return QDir::NoDotAndDotDot | QDir::AllDirs | (showHidden ? QDir::Hidden : QDir::Filters());
}
}

FolderView::FolderView(QWidget *parent) :
    FloatingWidgetContainer(parent),
    ui(new Ui::FolderView)
{
    ui->setupUi(this);

    // ------- filesystem view --------
    QString style = "font: %1pt;";
    style = style.arg(QApplication::font().pointSize());
    ui->dirTreeView->setStyleSheet(style);

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
    ui->newBookmarkButton->setToolTip(tr("Add bookmark"));
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

    connect(ui->newBookmarkButton, &IconButton::clicked, this, &FolderView::newBookmark);
    connect(ui->homeButton, &IconButton::clicked, this, &FolderView::onHomeBtn);
    connect(ui->rootButton, &IconButton::clicked, this, &FolderView::onRootBtn);

    connect(ui->zoomSlider, &QSlider::valueChanged, this, &FolderView::onZoomSliderValueChanged);
    connect(ui->sortingComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &FolderView::onSortingSelected);
    connect(ui->togglePlacesPanelButton, &ActionButton::toggled, this, &FolderView::onPlacesPanelButtonChecked);

    connect(ui->optionsPopupButton, &IconButton::toggled, this, &FolderView::onOptionsPopupButtonToggled);
    connect(optionsPopup, &FVOptionsPopup::dismissed, this, &FolderView::onOptionsPopupDismissed);
    connect(optionsPopup, &FVOptionsPopup::viewModeSelected, this, &FolderView::onViewModeSelected);

    connect(ui->dirTreeView, &TreeViewCustom::droppedIn, this, &FolderView::onDroppedInByIndex);
    connect(ui->dirTreeView, &TreeViewCustom::contextMenuRequestedAt, this, &FolderView::onTreeContextMenuRequested);
    connect(dirModel, &QFileSystemModel::directoryLoaded, this, &FolderView::onTreeDirectoryLoaded);
    connect(dirModel, &FileSystemModelCustom::folderIconChanged, ui->dirTreeView->viewport(),
            qOverload<>(&QWidget::update));
    connect(ui->bookmarksWidget, &BookmarksWidget::droppedIn, this, &FolderView::moveUrlsRequested); // ask what to do via popup? copy or move

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

// TODO: ask what to do
void FolderView::onDroppedInByIndex(QStringList paths, QModelIndex index) {
    emit moveUrlsRequested(std::move(paths), dirModel->filePath(index));
}

void FolderView::onTreeContextMenuRequested(const QModelIndex &index, const QPoint &globalPos) {
    if(!mTreeMenu) {
        mTreeMenu = new TreeContextMenu(this);
        connect(mTreeMenu, &TreeContextMenu::openRequested, this, &FolderView::directorySelected);
        connect(mTreeMenu, &TreeContextMenu::newWindowRequested, this, &NewWindow::open);
        connect(mTreeMenu, &TreeContextMenu::copyPathRequested, this, [this](const QString &path) {
            emit folderOperationRequested(TreeFolderOp::CopyPath, QDir::toNativeSeparators(path), QString());
        });
        connect(mTreeMenu, &TreeContextMenu::addBookmarkRequested, ui->bookmarksWidget, &BookmarksWidget::addBookmark);
    }
    const QString path = dirModel->filePath(index);
    mTreeMenu->openFor({path, ui->bookmarksWidget->hasBookmark(path)}, globalPos);
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

void FolderView::focusInEvent(QFocusEvent *event) {
    Q_UNUSED(event)
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

    // a tree click already made the row current
    const QString cleanPath = QDir::cleanPath(path);
    if(dirModel->filePath(ui->dirTreeView->currentIndex()) == cleanPath)
        return;

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
    if(!mTreeScrollWaits.removeOne(dirPath))
        return;
    // the model would sort the new rows on its next timer tick; sort now so the row is final
    dirModel->sort(0);
    fsTreeScrollToCurrent();
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

void FolderView::newBookmark() {
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
