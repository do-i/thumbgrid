#include "mainwindow.h"

#include <utility>

#include <QDir>

#include "platform/platformdesktop.h"
#include "gui/dialogs/custommessagebox.h"

// TODO: nuke this and rewrite

namespace {
// "thumbgrid 2026.10.3": ends every window title.
QString appTitle() {
    return QCoreApplication::applicationName() + " " + QCoreApplication::applicationVersion();
}

// A path under the home directory with the home part shown as "~".
QString tildePath(const QString &path) {
    const QString home = QDir::homePath();
    if(path == home)
        return QStringLiteral("~");
    if(home != "/" && path.startsWith(home + "/"))
        return "~" + path.mid(home.length());
    return path;
}
}

MW::MW(QWidget *parent)
    : FloatingWidgetContainer(parent),
      currentDisplay(0),
      maximized(false),
      activeSidePanel(SIDEPANEL_NONE),
      copyOverlay(nullptr),
      saveOverlay(nullptr),
      renameOverlay(nullptr),
      infoBarFullscreen(nullptr),
      floatingMessage(nullptr),
      cropPanel(nullptr),
      cropOverlay(nullptr)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    layout.setContentsMargins(0,0,0,0);
    layout.setSpacing(0);

    setMinimumSize(10,10);

    // a container: no focus on click, keys arrive via child passthrough
    setFocusPolicy(Qt::NoFocus);

    this->setLayout(&layout);

    setWindowTitle(appTitle());

    this->setMouseTracking(true);
    this->setAcceptDrops(true);
    this->setAccessibleName("mainwindow");
    windowGeometryChangeTimer.setSingleShot(true);
    windowGeometryChangeTimer.setInterval(30);
    setupUi();

    connect(settings, &Settings::settingsChanged, this, &MW::readSettings);
    connect(&windowGeometryChangeTimer, &QTimer::timeout, this, &MW::onWindowGeometryChanged);
    connect(this, &MW::fullscreenStateChanged, this, &MW::adaptToWindowState);

    readSettings();
    currentDisplay = settings->lastDisplay();
    maximized = settings->maximizedWindow();
    restoreWindowGeometry();
}

/*                                                             |--[ImageViewer]
 *                        |--[DocumentWidget]--[ViewerWidget]--|
 * [MW]--[CentralWidget]--|                                    |--[VideoPlayer]
 *                        |--[FolderView]
 *
 *  (not counting floating widgets)
 *  ViewerWidget exists for input handling reasons (correct overlay hover handling)
 */
void MW::setupUi() {
    viewerWidget.reset(new ViewerWidget(this));
    infoBarWindowed.reset(new InfoBarProxy(this));
    docWidget.reset(new DocumentWidget(viewerWidget, infoBarWindowed));
    folderView.reset(new FolderViewProxy(this));
    connect(folderView.get(), &FolderViewProxy::sortingSelected, this, &MW::sortingSelected);
    connect(folderView.get(), &FolderViewProxy::directorySelected, this, &MW::opened);
    connect(folderView.get(), &FolderViewProxy::copyUrlsRequested, this, &MW::copyUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::moveUrlsRequested, this, &MW::moveUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::convertFormatRequested, this, &MW::convertFormatRequested);
    connect(folderView.get(), &FolderViewProxy::renameRequested, this, &MW::renameRequested);
    connect(folderView.get(), &FolderViewProxy::searchQueryChanged, this, &MW::searchQueryChanged);
    connect(folderView.get(), &FolderViewProxy::showFoldersChanged, this, &MW::showFoldersChanged);

    centralWidget.reset(new CentralWidget(docWidget, folderView, this));
    layout.addWidget(centralWidget.get());
    controlsOverlay = new ControlsOverlay(docWidget.get());
    infoBarFullscreen = new FullscreenInfoOverlayProxy(viewerWidget.get());
    sidePanel = new SidePanel(this);
    sidePanel->setWindowTitle(tr("Crop"));
    connect(sidePanel, &SidePanel::closed, this, &MW::hideCropPanel);
    // parented to the window: viewerWidget is hidden in folder view
    floatingMessage = new FloatingMessageProxy(this);
    connect(viewerWidget.get(), &ViewerWidget::scalingRequested, this, &MW::scalingRequested);
    connect(viewerWidget.get(), &ViewerWidget::draggedOut, this, qOverload<>(&MW::draggedOut));
    connect(viewerWidget.get(), &ViewerWidget::playbackFinished, this, &MW::playbackFinished);
    connect(viewerWidget.get(), &ViewerWidget::showScriptSettings, this, &MW::showScriptSettings);
    connect(viewerWidget.get(), &ViewerWidget::scaleChanged, this, &MW::updateStatusFooters);
    connect(this, &MW::zoomIn,        viewerWidget.get(), &ViewerWidget::zoomIn);
    connect(this, &MW::zoomOut,       viewerWidget.get(), &ViewerWidget::zoomOut);
    connect(this, &MW::zoomInCursor,  viewerWidget.get(), &ViewerWidget::zoomInCursor);
    connect(this, &MW::zoomOutCursor, viewerWidget.get(), &ViewerWidget::zoomOutCursor);
    connect(this, &MW::scrollUp,    viewerWidget.get(), &ViewerWidget::scrollUp);
    connect(this, &MW::scrollDown,  viewerWidget.get(), &ViewerWidget::scrollDown);
    connect(this, &MW::scrollLeft,  viewerWidget.get(), &ViewerWidget::scrollLeft);
    connect(this, &MW::scrollRight, viewerWidget.get(), &ViewerWidget::scrollRight);
    connect(this, &MW::scrollUpFine,    viewerWidget.get(), &ViewerWidget::scrollUpFine);
    connect(this, &MW::scrollDownFine,  viewerWidget.get(), &ViewerWidget::scrollDownFine);
    connect(this, &MW::scrollLeftFine,  viewerWidget.get(), &ViewerWidget::scrollLeftFine);
    connect(this, &MW::scrollRightFine, viewerWidget.get(), &ViewerWidget::scrollRightFine);
    connect(this, &MW::scrollOrPrevImage, viewerWidget.get(), &ViewerWidget::scrollOrPrevImage);
    connect(this, &MW::scrollOrNextImage, viewerWidget.get(), &ViewerWidget::scrollOrNextImage);
    connect(this, &MW::pauseVideo,     viewerWidget.get(), &ViewerWidget::pauseResumePlayback);
    connect(this, &MW::stopPlayback,   viewerWidget.get(), &ViewerWidget::stopPlayback);
    connect(this, &MW::seekVideoForward, viewerWidget.get(), &ViewerWidget::seekForward);
    connect(this, &MW::seekVideoBackward,  viewerWidget.get(), &ViewerWidget::seekBackward);
    connect(this, &MW::frameStep,      viewerWidget.get(), &ViewerWidget::frameStep);
    connect(this, &MW::frameStepBack,  viewerWidget.get(), &ViewerWidget::frameStepBack);
    connect(this, &MW::toggleMute,  viewerWidget.get(), &ViewerWidget::toggleMute);
    connect(this, &MW::toggleClip, viewerWidget.get(), &ViewerWidget::toggleClip);
    connect(this, &MW::clearClip, viewerWidget.get(), &ViewerWidget::clearClip);
    connect(this, &MW::setClipStart, viewerWidget.get(), &ViewerWidget::setClipStart);
    connect(this, &MW::setClipEnd, viewerWidget.get(), &ViewerWidget::setClipEnd);
    connect(this, &MW::toggleVideoLoop, viewerWidget.get(), &ViewerWidget::toggleVideoLoop);
    connect(this, &MW::volumeUp,  viewerWidget.get(), &ViewerWidget::volumeUp);
    connect(this, &MW::volumeDown,  viewerWidget.get(), &ViewerWidget::volumeDown);
    connect(this, &MW::toggleTransparencyGrid, viewerWidget.get(), &ViewerWidget::toggleTransparencyGrid);
    connect(this, &MW::setLoopPlayback,  viewerWidget.get(), &ViewerWidget::setLoopPlayback);
}

void MW::setupFullUi() {
    setupCropPanel();
    docWidget->allowPanelInit();
    docWidget->setupMainPanel();
    infoBarWindowed->init();
    infoBarFullscreen->init();
}

void MW::setupCropPanel() {
    if(cropPanel)
        return;
    cropOverlay = new CropOverlay(viewerWidget.get());
    cropPanel = new CropPanel(cropOverlay, this);
    connect(cropPanel, &CropPanel::cancel, this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::cropRequested);
}

void MW::setupCopyOverlay() {
    copyOverlay = new CopyOverlay(viewerWidget.get());
    connect(copyOverlay, &CopyOverlay::copyRequested, this, &MW::copyRequested);
    connect(copyOverlay, &CopyOverlay::moveRequested, this, &MW::moveRequested);
}

void MW::setupSaveOverlay() {
    saveOverlay = new SaveConfirmOverlay(viewerWidget.get());
    connect(saveOverlay, &SaveConfirmOverlay::saveClicked,    this, &MW::saveRequested);
    connect(saveOverlay, &SaveConfirmOverlay::saveAsClicked,  this, &MW::saveAsClicked);
    connect(saveOverlay, &SaveConfirmOverlay::discardClicked, this, &MW::discardEditsRequested);
}

void MW::setupRenameOverlay() {
    renameOverlay = new RenameOverlay(this);
    renameOverlay->setName(info.fileName);
    connect(renameOverlay, &RenameOverlay::renameRequested, this, &MW::renameRequested);
}

void MW::toggleFolderView() {
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    docWidget->hideFloatingPanel();
    centralWidget->toggleViewMode();
    onInfoUpdated();
}

void MW::enableFolderView() {
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    docWidget->hideFloatingPanel();
    centralWidget->showFolderView();
    // restore the grid's own window size; opening a small image shrank it
    if(settings->autoResizeWindow() && folderViewGeometry.isValid() &&
       windowState() == Qt::WindowNoState)
        setGeometry(folderViewGeometry);
    onInfoUpdated();
}

void MW::enableDocumentView() {
    centralWidget->showDocumentView();
    onInfoUpdated();
}

ViewMode MW::currentViewMode() {
    return centralWidget->currentViewMode();
}

// a video has nothing to fit, so the fit actions pause/resume it (Space is toggleFitMode)
bool MW::fitActionPausesVideo() {
    if(!viewerWidget->isShowingVideo())
        return false;
    viewerWidget->pauseResumePlayback();
    return true;
}

void MW::fitWindow() {
    if(fitActionPausesVideo())
        return;
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindow();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitWidth() {
    if(fitActionPausesVideo())
        return;
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWidth();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitOriginal() {
    if(fitActionPausesVideo())
        return;
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitOriginal();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitWindowStretch() {
    if(fitActionPausesVideo())
        return;
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindowStretch();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

// switch between 1:1 and Fit All
void MW::switchFitMode() {
    if(fitActionPausesVideo())
        return;
    if(viewerWidget->fitMode() == FIT_WINDOW)
        viewerWidget->setFitMode(FIT_ORIGINAL);
    else
        viewerWidget->setFitMode(FIT_WINDOW);
}

void MW::closeImage() {
    info.fileName = "";
    info.filePath = "";
    viewerWidget->closeImage();
}

void MW::prepareForLoad(bool nextIsVideo) {
    viewerWidget->prepareForLoad(nextIsVideo);
}

// An axis extent the way QLayout resolves it (hints, then min/max clamps), answerable before
// the widget was ever shown. QWIDGETSIZE_MAX means unconstrained.
static int resolvedExtent(const QWidget *w, Qt::Orientation axis) {
    const bool horizontal = (axis == Qt::Horizontal);
    const int hint    = horizontal ? w->sizeHint().width()        : w->sizeHint().height();
    const int minHint = horizontal ? w->minimumSizeHint().width() : w->minimumSizeHint().height();
    const int minimum = horizontal ? w->minimumWidth()            : w->minimumHeight();
    const int maximum = horizontal ? w->maximumWidth()            : w->maximumHeight();
    int extent = qMax(hint, minHint);
    if(minimum < QWIDGETSIZE_MAX)
        extent = qMax(extent, minimum);
    if(maximum < QWIDGETSIZE_MAX)
        extent = qMin(extent, maximum);
    return qMax(0, extent);
}

// Window size minus the viewer (footer, pinned panel). Estimated before the first show, since
// main.cpp loads a command-line file before showGui().
QSize MW::windowChromeSize() const {
    if(isVisible() && viewerWidget->size().isValid()) {
        QSize measured = size() - viewerWidget->size();
        return QSize(qMax(0, measured.width()), qMax(0, measured.height()));
    }
    QSize estimate(0, 0);
    // InfoBarProxy fixes its own height, so this is exact before init()
    if(showInfoBarWindowed && !isFullScreen())
        estimate.rheight() += resolvedExtent(infoBarWindowed.get(), Qt::Vertical);
    // Pinned panel on its axis. Its sizeHint() is (0,0) until setupFullUi() runs (50 ms after show),
    // so at startup only the minimum counts.
    if(settings->panelEnabled() && settings->panelPinned()) {
        if(auto panel = docWidget->findChild<MainPanel *>()) {
            const auto pos = settings->panelPosition();
            if(pos == PANEL_TOP || pos == PANEL_BOTTOM)
                estimate.rheight() += resolvedExtent(panel, Qt::Vertical);
            else
                estimate.rwidth() += resolvedExtent(panel, Qt::Horizontal);
        }
    }
    return estimate;
}

// todo: fix flicker; ideally change image and resize in one go
void MW::preShowResize(QSize sz) {
    auto screens = qApp->screens();
    if(this->windowState() != Qt::WindowNoState || !screens.count() || screens.count() <= currentDisplay)
        return;
    // Save the grid size now: loadPath() loads the image before switching views, so the window
    // is already shrunk by the time the view switch runs.
    if(isVisible() && currentViewMode() == MODE_FOLDERVIEW)
        folderViewGeometry = geometry();
    int decorationSize = frameGeometry().height() - height();
    float maxSzMulti = settings->autoResizeLimit() / 100.f;
    QRect availableGeom = screens.at(currentDisplay)->availableGeometry();
    QSize maxSz = availableGeom.size() * maxSzMulti;
    maxSz.setHeight(maxSz.height() - decorationSize);

    // size the viewport (not the window) to the image, then add the chrome back
    const QSize chrome = windowChromeSize();
    QSize maxContentSz = maxSz - chrome;
    maxContentSz.setWidth(qMax(1, maxContentSz.width()));
    maxContentSz.setHeight(qMax(1, maxContentSz.height()));

    if(!sz.isEmpty()) {
        if(sz.width() > maxContentSz.width() || sz.height() > maxContentSz.height())
            sz.scale(maxContentSz, Qt::KeepAspectRatio);
        sz += chrome;
    } else {
        sz = maxSz;
    }
    QRect newGeom(0,0, sz.width(), sz.height());
    newGeom.moveCenter(availableGeom.center());
    newGeom.translate(0, decorationSize / 2);

    if(this->isVisible())
        setGeometry(newGeom);
    else // setGeometry wont work on hidden windows, so we just save for it to be restored later
        settings->setWindowGeometry(newGeom);
}

void MW::showImage(std::unique_ptr<QPixmap> pixmap) {
    if(settings->autoResizeWindow())
        preShowResize(pixmap->size());
    viewerWidget->showImage(std::move(pixmap));
    updateCropPanelData();
}

void MW::showAnimation(const std::shared_ptr<QMovie>& movie) {
    if(settings->autoResizeWindow())
        preShowResize(movie->frameRect().size());
    viewerWidget->showAnimation(movie);
    updateCropPanelData();
}

void MW::showVideo(QString file) {
    if(settings->autoResizeWindow())
        preShowResize(QSize()); // tmp. find a way to get this though mpv BEFORE playback
    viewerWidget->showVideo(std::move(file));
}

void MW::showText(QString file) {
    viewerWidget->showText(std::move(file));
}

void MW::showContextMenu() {
    viewerWidget->showContextMenu();
}

void MW::onSortingChanged(SortingMode mode) {
    folderView.get()->onSortingChanged(mode);
    if(centralWidget.get()->currentViewMode() == ViewMode::MODE_DOCUMENT) {
        switch(mode) {
            case SortingMode::SORT_NAME:      showMessage("Sorting: By Name");              break;
            case SortingMode::SORT_NAME_DESC: showMessage("Sorting: By Name (desc.)");      break;
            case SortingMode::SORT_TIME:      showMessage("Sorting: By Time");              break;
            case SortingMode::SORT_TIME_DESC: showMessage("Sorting: By Time (desc.)");      break;
            case SortingMode::SORT_SIZE:      showMessage("Sorting: By File Size");         break;
            case SortingMode::SORT_SIZE_DESC: showMessage("Sorting: By File Size (desc.)"); break;
        }
    }
}

void MW::setDirectoryPath(QString path) {
    info.directoryPath = path;
    info.directoryName = path.split("/").last();
    folderView->setDirectoryPath(path);
    onInfoUpdated();
}

void MW::toggleLockZoom() {
    viewerWidget->toggleLockZoom();
    if(viewerWidget->lockZoomEnabled())
        showMessage("Zoom lock: ON");
    else
        showMessage("Zoom lock: OFF");
    onInfoUpdated();
}

void MW::toggleLockView() {
    viewerWidget->toggleLockView();
    if(viewerWidget->lockViewEnabled())
        showMessage("View lock: ON");
    else
        showMessage("View lock: OFF");
    onInfoUpdated();
}

void MW::toggleFullscreenInfoBar() {
    if(!this->isFullScreen())
        return;
    showInfoBarFullscreen = !showInfoBarFullscreen;
    if(showInfoBarFullscreen)
        infoBarFullscreen->showWhenReady();
    else
        infoBarFullscreen->hide();
}

// tells the folder view whether the prefix it last emitted matched anything
void MW::setFolderViewSearchMatched(bool matched) {
    folderView->setSearchMatched(matched);
}

void MW::startFolderSearch() {
    folderView->startSearch();
}

void MW::navigateFolderGrid(GridNavigation direction) {
    folderView->navigate(direction);
}

void MW::zoomFolderGrid(int steps) {
    folderView->zoomThumbnails(steps);
}

void MW::toggleRenameOverlay(QString currentName) {
    // in folder view, rename happens in place over the selected cell
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW) {
        folderView->startRename(currentName);
        return;
    }
    if(!renameOverlay)
        setupRenameOverlay();
    if(renameOverlay->isHidden()) {
        renameOverlay->setBackdropEnabled((centralWidget->currentViewMode() == MODE_FOLDERVIEW));
        renameOverlay->setName(std::move(currentName));
        renameOverlay->show();
    } else {
        renameOverlay->hide();
    }
}

void MW::toggleScalingFilter() {
    ScalingFilter configuredFilter = settings->scalingFilter();
    if(viewerWidget->scalingFilter() == configuredFilter) {
        setFilterNearest();
    }
    else {
        setFilter(configuredFilter);
    }
}

void MW::setFilterNearest() {
    showMessage("Filter: nearest", 600);
    viewerWidget->setFilterNearest();
}

void MW::setFilterBilinear() {
    showMessage("Filter: bilinear", 600);
    viewerWidget->setFilterBilinear();
}

void MW::setFilter(ScalingFilter filter) {
    QString filterName;
    switch (filter) {
        case QI_FILTER_NEAREST:
            filterName = "nearest";
            break;
        case ScalingFilter::QI_FILTER_BILINEAR:
            filterName = "smooth";
            break;
        case QI_FILTER_HIGH_QUALITY:
            filterName = "high quality";
            break;
        default:
            filterName = "configured " + QString::number(static_cast<int>(filter));
            break;
    }
    showMessage("Filter " + filterName, 600);
    viewerWidget->setScalingFilter(filter);
}

bool MW::isCropPanelActive() {
    return (activeSidePanel == SIDEPANEL_CROP);
}

void MW::onScalingFinished(std::unique_ptr<QPixmap> scaled) {
    viewerWidget->onScalingFinished(std::move(scaled));
}

void MW::saveWindowGeometry() {
    if(this->windowState() == Qt::WindowNoState)
        settings->setWindowGeometry(geometry());
    settings->setMaximizedWindow(maximized);
}

// does not apply fullscreen; window size / maximized state only
void MW::restoreWindowGeometry() {
    this->setGeometry(settings->windowGeometry());
    if(settings->maximizedWindow())
        this->setWindowState(Qt::WindowMaximized);
    updateCurrentDisplay();
}

void MW::updateCurrentDisplay() {
    auto screens = qApp->screens();
    currentDisplay = screens.indexOf(this->window()->screen());
}

void MW::onWindowGeometryChanged() {
    saveWindowGeometry();
    updateCurrentDisplay();
}

void MW::saveCurrentDisplay() {
    settings->setLastDisplay(qApp->screens().indexOf(this->window()->screen()));
}

void MW::mouseMoveEvent(QMouseEvent *event) {
    event->ignore();
}

bool MW::event(QEvent *event) {
    // only save maximized state once visible
    if(event->type() == QEvent::WindowStateChange && this->isVisible() && !this->isFullScreen())
        maximized = isMaximized();
    if(event->type() == QEvent::Move || event->type() == QEvent::Resize)
        windowGeometryChangeTimer.start();
    return QWidget::event(event);
}

// hook up to actionManager
void MW::keyPressEvent(QKeyEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::wheelEvent(QWheelEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mousePressEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseDoubleClickEvent(QMouseEvent *event) {
    event->accept();
    QMouseEvent fakePressEvent(
        QEvent::MouseButtonPress,
        event->position(),
        event->scenePosition(),
        event->globalPosition(),
        event->button(),
        event->buttons(),
        event->modifiers(),
        event->pointingDevice()
    );
    actionManager->processEvent(&fakePressEvent);
    actionManager->processEvent(event);
}

void MW::close() {
    saveWindowGeometry();
    saveCurrentDisplay();
    // QWidget::close() no longer works on hidden windows since Qt 6.3
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    this->hide();
#endif
    if(copyOverlay)
        copyOverlay->saveSettings();
    QWidget::close();
}

void MW::closeEvent(QCloseEvent *event) {
    // catch the close event when user presses X on the window itself
    event->accept();
    actionManager->invokeAction("exit");
}

void MW::dragEnterEvent(QDragEnterEvent *e) {
    if(e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MW::dropEvent(QDropEvent *event) {
    emit droppedIn(event->mimeData(), event->source());
}

void MW::resizeEvent(QResizeEvent *event) {
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
    }
    FloatingWidgetContainer::resizeEvent(event);
}

void MW::showDefault() {
    if(!this->isVisible()) {
        if(settings->fullscreenMode())
            showFullScreen();
        else
            showWindowed();
    }
}

void MW::showSaveDialog(QString filePath) {
    QString newFilePath = getSaveFileName(std::move(filePath));
    if(!newFilePath.isEmpty())
        emit saveAsRequested(newFilePath);
}

QString MW::getSaveFileName(QString filePath) {
    docWidget->hideFloatingPanel();
    QStringList filters;
    // writable image formats; todo: some may need to be blacklisted
    auto writerFormats = QImageWriter::supportedImageFormats();
    if(writerFormats.contains("jpg"))  filters.append("JPEG (*.jpg *.jpeg *jpe *jfif)");
    if(writerFormats.contains("png"))  filters.append("PNG (*.png)");
    if(writerFormats.contains("webp")) filters.append("WebP (*.webp)");
    // may not work..
    if(writerFormats.contains("jp2"))  filters.append("JPEG 2000 (*.jp2 *.j2k *.jpf *.jpx *.jpm *.jpgx)");
    if(writerFormats.contains("jxl"))  filters.append("JPEG-XL (*.jxl)");
    if(writerFormats.contains("avif")) filters.append("AVIF (*.avif *.avifs)");
    if(writerFormats.contains("heic")) filters.append("HEIF (*.heic *.heif *.hif)");
    if(writerFormats.contains("tif"))  filters.append("TIFF (*.tif *.tiff)");
    if(writerFormats.contains("bmp"))  filters.append("BMP (*.bmp)");
    if(PlatformDesktop::supportsIcoSaveFormat() && writerFormats.contains("ico"))
        filters.append("Icon Files (*.ico)");
    if(writerFormats.contains("ppm"))  filters.append("PPM (*.ppm)");
    if(writerFormats.contains("xbm"))  filters.append("XBM (*.xbm)");
    if(writerFormats.contains("xpm"))  filters.append("XPM (*.xpm)");
    if(writerFormats.contains("dds"))  filters.append("DDS (*.dds)");
    if(writerFormats.contains("wbmp")) filters.append("WBMP (*.wbmp)");
    // add everything else from imagewriter
    for(const auto& fmt : writerFormats) {
        if(filters.filter(fmt).isEmpty())
            filters.append(fmt.toUpper() + " (*." + fmt + ")");
    }
    QString filterString = filters.join(";; ");

    // find matching filter for the current image
    QString selectedFilter = "JPEG (*.jpg *.jpeg *jpe *jfif)";
    QFileInfo fi(filePath);
    for(const auto& filter : filters) {
        if(filter.contains(fi.suffix().toLower())) {
            selectedFilter = filter;
            break;
        }
    }
    QString newFilePath = QFileDialog::getSaveFileName(this, tr("Save File as..."), filePath, filterString, &selectedFilter);
    return newFilePath;
}

void MW::showOpenDialog(QString path) {
    docWidget->hideFloatingPanel();

    QFileDialog dialog(this);
    QStringList imageFilter;
    imageFilter.append(settings->supportedFormatsFilter());
    imageFilter.append("All Files (*)");
    dialog.setDirectory(path);
    dialog.setNameFilters(imageFilter);
    dialog.setWindowTitle("Open image");
    dialog.setWindowModality(Qt::ApplicationModal);
    connect(&dialog, &QFileDialog::fileSelected, this, &MW::opened);
    dialog.exec();
}

void MW::showResizeDialog(QSize initialSize) {
    ResizeDialog dialog(initialSize, this);
    connect(&dialog, &ResizeDialog::sizeSelected, this, &MW::resizeRequested);
    dialog.exec();
}

void MW::showResizeCopiesDialog(QSize seedSize, int imageCount) {
    ResizeDialog dialog(seedSize, this);
    dialog.setCopyMode(imageCount);
    connect(&dialog, &ResizeDialog::specSelected, this, &MW::resizeCopiesRequested);
    dialog.exec();
}

DialogResult MW::fileReplaceDialog(QString src, QString dst, FileReplaceMode mode, bool multiple) {
    FileReplaceDialog dialog(this);
    dialog.setModal(true);
    dialog.setSource(std::move(src));
    dialog.setDestination(std::move(dst));
    dialog.setMode(mode);
    dialog.setMulti(multiple);

    dialog.exec();

    return dialog.getResult();
}

void MW::showSettings() {
    docWidget->hideFloatingPanel();
    SettingsDialog settingsDialog(this);
    settingsDialog.exec();
}

void MW::showScriptSettings() {
    docWidget->hideFloatingPanel();
    SettingsDialog settingsDialog(this);
    settingsDialog.switchToPage(5);
    settingsDialog.exec();
}

void MW::triggerFullScreen() {
    if(!isFullScreen()) {
        showFullScreen();
    } else {
        showWindowed();
    }
}

void MW::showFullScreen() {
    //do not save immediately on application start
    if(!isHidden())
        saveWindowGeometry();
    auto screens = qApp->screens();
    // todo: why check the screen again?
    int _currentDisplay = screens.indexOf(this->window()->screen());
    //move to target screen
    if(screens.count() > currentDisplay && currentDisplay != _currentDisplay) {
        this->move(screens.at(currentDisplay)->geometry().x(),
                   screens.at(currentDisplay)->geometry().y());
    }
    QWidget::showFullScreen();
    // try to repaint sooner
    // FIXME: re-entrancy hazard (processEvents)
    qApp->processEvents();
    emit fullscreenStateChanged(true);
}

void MW::showWindowed() {
    if(isFullScreen())
        QWidget::showNormal();
    restoreWindowGeometry();
    QWidget::show();
    // try to repaint sooner
    // FIXME: re-entrancy hazard (processEvents)
    qApp->processEvents();
    emit fullscreenStateChanged(false);
}

void MW::updateCropPanelData() {
    if(cropPanel && activeSidePanel == SIDEPANEL_CROP) {
        cropPanel->setImageRealSize(viewerWidget->sourceSize());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageRealSize(viewerWidget->sourceSize());
        // start with the whole image: a visible handle, and accepting it is a no-op (doCrop cancels)
        cropOverlay->selectAll();
    }
}

void MW::showSaveOverlay() {
    if(!settings->showSaveOverlay())
        return;
    if(!saveOverlay)
        setupSaveOverlay();
    saveOverlay->show();
}

void MW::hideSaveOverlay() {
    if(!saveOverlay)
        return;
    saveOverlay->hide();
}

void MW::setSaveOverlayBusy(bool busy) {
    if(saveOverlay)
        saveOverlay->setBusy(busy);
}

void MW::triggerCropPanel() {
    if(activeSidePanel != SIDEPANEL_CROP) {
        showCropPanel();
    } else {
        hideCropPanel();
    }
}

void MW::showCropPanel() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;

    if(activeSidePanel != SIDEPANEL_CROP) {
        docWidget->hideFloatingPanel();
        sidePanel->setWidget(cropPanel);
        sidePanel->show();
        positionCropPanel();
        // frame margins are only known once the window is mapped
        QTimer::singleShot(0, this, &MW::positionCropPanel);
        cropOverlay->show();
        activeSidePanel = SIDEPANEL_CROP;
        // reset & lock zoom so CropOverlay won't go crazy
        viewerWidget->fitWindow();
        setInteractionEnabled(false);
        // feed the panel current image info
        updateCropPanelData();
    }
}

// park the crop panel outside the window's right edge, clamped to the screen
void MW::positionCropPanel() {
    if(!sidePanel || sidePanel->isHidden())
        return;
    QScreen *scr = screen();
    if(!scr)
        return;

    const QRect avail      = scr->availableGeometry();
    const QRect mainFrame  = frameGeometry();
    const QRect panelFrame = sidePanel->frameGeometry();
    const QSize panelSize  = panelFrame.size();
    // move() places the client area; compensate for window decorations
    const QPoint frameOffset = sidePanel->pos() - panelFrame.topLeft();

    int x = mainFrame.right() + 1;
    if(x + panelSize.width() > avail.right() + 1)
        x = avail.right() + 1 - panelSize.width();
    x = qMax(x, avail.left());

    int y = mainFrame.bottom() + 1 - panelSize.height();
    y = qMin(y, avail.bottom() + 1 - panelSize.height());
    y = qMax(y, avail.top());

    sidePanel->move(QPoint(x, y) + frameOffset);
}

void MW::setInteractionEnabled(bool mode) {
    docWidget->setInteractionEnabled(mode);
    viewerWidget->setInteractionEnabled(mode);
}

void MW::hideCropPanel() {
    sidePanel->hide();
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->hide();
        setInteractionEnabled(true);
    }
    activeSidePanel = SIDEPANEL_NONE;
}

void MW::sendToCopyFolder(int slot) {
    if(copyOverlay && copyOverlay->isVisible())
        copyOverlay->sendToFolder(slot);
}

void MW::triggerCopyOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_COPY) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_COPY);
        copyOverlay->show();
    }
}

void MW::triggerMoveOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_MOVE) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_MOVE);
        copyOverlay->show();
    }
}

// quit fullscreen or exit the program
void MW::closeFullScreenOrExit() {
    if(this->isFullScreen()) {
        this->showWindowed();
    } else {
        actionManager->invokeAction("exit");
    }
}

// todo: this is crap, use shared state object
void MW::setCurrentInfo(int _index, int _fileCount, QString _filePath, QString _fileName, QSize _imageSize, int _imageDepth, qint64 _fileSize, bool slideshow, bool shuffle, bool edited) {
    info.index = _index;
    info.fileCount = _fileCount;
    info.fileName = std::move(_fileName);
    info.filePath = std::move(_filePath);
    info.imageSize = _imageSize;
    info.imageDepth = _imageDepth;
    info.fileSize = _fileSize;
    info.slideshow = slideshow;
    info.shuffle = shuffle;
    info.edited = edited;
    onInfoUpdated();
}

void MW::setFolderStatusText(QString text) {
    folderStatusText = std::move(text);
    updateStatusFooters();
}

QString MW::statusDataSize(qint64 bytes) const {
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

QString MW::documentStatusText() const {
    if(info.fileName.isEmpty())
        return tr("No file opened.");

    // Only the current image: name, file size, dimensions, zoom (1.00x == 100%).
    QString text = info.fileName;
    if(info.fileSize)
        text += "  " + statusDataSize(info.fileSize);
    if(info.imageSize.width())
        text += "  " + QString::number(info.imageSize.width()) + "x" + QString::number(info.imageSize.height());
    if(info.imageSize.width())
        text += "  " + QString::number(viewerWidget->currentScale(), 'f', 2) + "x";

    return text;
}

void MW::updateStatusFooters() {
    bool visible = showInfoBarWindowed && !isFullScreen();
    infoBarWindowed->setVisible(visible);
    folderView->setStatusFooterVisible(visible);

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW) {
        folderView->setStatusText(folderStatusText);
    } else {
        infoBarWindowed->setStatusText(documentStatusText());
    }
}

// todo: nuke and rewrite
void MW::onInfoUpdated() {
    QString posString;
    if(info.fileCount)
        posString = "[ " + QString::number(info.index + 1) + "/" + QString::number(info.fileCount) + " ]";
    QString resString;
    if(info.imageSize.width())
        resString = QString::number(info.imageSize.width()) + " x " + QString::number(info.imageSize.height());
    QString sizeString;
    if(info.fileSize)
        sizeString = this->locale().formattedDataSize(info.fileSize, 1);

    if(renameOverlay)
        renameOverlay->setName(info.fileName);

    QString windowTitle;
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW) {
        windowTitle = info.directoryPath.isEmpty() ? tr("Folder view") : tildePath(info.directoryPath);
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
        folderView->setStatusText(folderStatusText);
    } else if(info.fileName.isEmpty()) {
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
        infoBarWindowed->setStatusText(tr("No file opened."));
    } else {
        windowTitle = tildePath(info.filePath);
        if(settings->windowTitleExtendedInfo()) {
            windowTitle.prepend(posString + "  ");
            if(!resString.isEmpty())
                windowTitle.append("  -  " + resString);
            if(!sizeString.isEmpty())
                windowTitle.append("  -  " + sizeString);
        }

        // toggleable states
        QString states;
        if(info.slideshow)
            states.append(" [slideshow]");
        if(info.shuffle)
            states.append(" [shuffle]");
        if(viewerWidget->lockZoomEnabled())
            states.append(" [zoom lock]");
        if(viewerWidget->lockViewEnabled())
            states.append(" [view lock]");

        if(!settings->infoBarWindowed() && !states.isEmpty())
            windowTitle.append(" -" + states);
        if(info.edited)
            windowTitle.prepend("* ");

        infoBarFullscreen->setInfo(posString, info.fileName + (info.edited ? "  *" : ""), resString + "  " + sizeString);
        infoBarWindowed->setStatusText(documentStatusText());
    }
    updateStatusFooters();
    setWindowTitle(windowTitle.isEmpty() ? appTitle() : windowTitle + " — " + appTitle());
}

std::shared_ptr<FolderViewProxy> MW::getFolderView() {
    return folderView;
}

std::shared_ptr<ThumbnailStripProxy> MW::getThumbnailPanel() {
    return docWidget->thumbPanel();
}

void MW::showMessageDirectory(QString dirName) {
    floatingMessage->showMessage(std::move(dirName), FloatingMessageIcon::ICON_DIRECTORY, 1700);
}

void MW::showMessage(QString text) {
    floatingMessage->showMessage(std::move(text),  FloatingMessageIcon::NO_ICON, 1500);
}

void MW::showMessage(QString text, int duration) {
    floatingMessage->showMessage(std::move(text), FloatingMessageIcon::NO_ICON, duration);
}

void MW::showMessageSuccess(QString text) {
    floatingMessage->showMessage(std::move(text),  FloatingMessageIcon::ICON_SUCCESS, 1500);
}

void MW::showWarning(QString text) {
    floatingMessage->showMessage(std::move(text),  FloatingMessageIcon::ICON_WARNING, 1500);
}

void MW::showError(QString text) {
    floatingMessage->showMessage(std::move(text),  FloatingMessageIcon::ICON_ERROR, 2800);
}

bool MW::showConfirmation(const QString& title, const QString& msg, bool danger) {
    return CustomMessageBox::confirm(this, title, msg, danger);
}

void MW::showErrorDialog(const QString& title, const QString& msg) {
    CustomMessageBox::message(this, title, msg);
}

void MW::readSettings() {
    showInfoBarFullscreen = settings->infoBarFullscreen();
    showInfoBarWindowed = settings->infoBarWindowed();
    adaptToWindowState();
}

// todo: remove/rename?
void MW::applyWindowedBackground() {
#ifdef USE_KDE_BLUR
    QWindow* window = this->windowHandle();
    if(window) {
        if(settings->backgroundOpacity() == 1.0)
            KWindowEffects::enableBlurBehind(window, false);
        else
            KWindowEffects::enableBlurBehind(window, settings->blurBackground());
    }
#endif
}

void MW::applyFullscreenBackground() {
#ifdef USE_KDE_BLUR
    QWindow* window = this->windowHandle();
    if(window)
        KWindowEffects::enableBlurBehind(window, false);
#endif
}

// changes ui elements according to fullscreen state
void MW::adaptToWindowState() {
    docWidget->hideFloatingPanel();
    if(isFullScreen()) { //-------------------------------------- fullscreen ---
        applyFullscreenBackground();
        updateStatusFooters();

        if(showInfoBarFullscreen)
            infoBarFullscreen->showWhenReady();
        else
            infoBarFullscreen->hide();    

        auto pos = settings->panelPosition();
        if(!settings->panelEnabled() || pos == PANEL_BOTTOM || pos == PANEL_LEFT)
            controlsOverlay->show();
        else
            controlsOverlay->hide();
    } else { //------------------------------------------------------ window ---
        applyWindowedBackground();
        infoBarFullscreen->hide();
        updateStatusFooters();

        controlsOverlay->hide();
    }
    folderView->onFullscreenModeChanged(isFullScreen());
    docWidget->onFullscreenModeChanged(isFullScreen());
    viewerWidget->onFullscreenModeChanged(isFullScreen());
}

void MW::paintEvent(QPaintEvent *event) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    FloatingWidgetContainer::paintEvent(event);
}

void MW::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    docWidget->hideFloatingPanel(true);
}

