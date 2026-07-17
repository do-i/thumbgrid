/*
 * This is sort-of a main controller of application.
 * It creates and initializes all components, then sets up gui and actions.
 * Most of communication between components go through here.
 *
 */

#include "core.h"

#include "platform/platformdesktop.h"
#include "utils/safesave.h"
#include "utils/logging.h"

#include <QInputDialog>
#include <QLineEdit>
#include <utility>

Core::Core()
    : QObject(),
      folderEndAction(FOLDER_END_NO_ACTION),
      loopSlideshow(false),
      mDrag(nullptr),
      slideshow(false),
      shuffle(false)
{
    loadTranslation();
    initGui();
    initComponents();
    connectComponents();
    initActions();
    readSettings();
    slideshowTimer.setSingleShot(true);
    connect(settings, &Settings::settingsChanged, this, &Core::readSettings);

    QVersionNumber lastVersion = settings->lastVersion();
    if(settings->firstRun())
        onFirstRun();
    else if(appVersion > lastVersion)
        onUpdate();
}

void Core::readSettings() {
    loopSlideshow = settings->loopSlideshow();
    folderEndAction = settings->folderEndAction();
    slideshowTimer.setInterval(settings->slideshowInterval());
    bool showDirs = (settings->folderViewMode() == FV_EXT_FOLDERS);
    if(folderViewPresenter.showDirs() != showDirs)
        folderViewPresenter.setShowDirs(showDirs);
    bool showOther = settings->showOtherFileTypes();
    if(showOther != mShowOtherFileTypes) {
        mShowOtherFileTypes = showOther;
        // rescan so the new filter takes effect; deferred because the model's own
        // settingsChanged slot (which updates the filter) may not have run yet
        if(model && !model->directoryPath().isEmpty()) {
            QTimer::singleShot(0, this, [this]() {
                model->setDirectory(model->directoryPath());
            });
        }
    }
    if(shuffle)
        syncRandomizer();
}

void Core::showGui() {
    if(mw && !mw->isVisible())
        mw->showDefault();
    // TODO: this is unreliable.
    // how to make it wait until a window is shown?
    // FIXME: re-entrancy hazard (processEvents)
    qApp->processEvents();
    QTimer::singleShot(50, mw, SLOT(setupFullUi()));
}

// create MainWindow and all widgets
void Core::initGui() {
    mw = new MW();
    mw->hide();
}

void Core::attachModel(DirectoryModel *_model) {
    model.reset(_model);
    fileOps->setModel(model);
    thumbPanelPresenter.setModel(model);
    folderViewPresenter.setShowParentDir(true);
    folderViewPresenter.setModel(model);
    bool showDirs = (settings->folderViewMode() == FV_EXT_FOLDERS);
    folderViewPresenter.setShowDirs(showDirs);
    if(shuffle)
        syncRandomizer();
}

void Core::initComponents() {
    fileOps = new FileOperationsController(mw, this);
    // single-file removal goes through Core, which closes playing media first
    fileOps->setRemoveFileHandler([this](QString path, bool trash) {
        return removeFile(std::move(path), trash);
    });
    attachModel(new DirectoryModel());
}

void Core::connectComponents() {
    thumbPanelPresenter.setView(mw->getThumbnailPanel());
    connect(&thumbPanelPresenter, &DirectoryPresenter::fileActivated,
            this, &Core::onDirectoryViewFileActivated);
    connect(&thumbPanelPresenter, &DirectoryPresenter::dirActivated,
            this, &Core::loadPath);

    folderViewPresenter.setView(mw->getFolderView());
    connect(&folderViewPresenter, &DirectoryPresenter::fileActivated,
            this, &Core::onDirectoryViewFileActivated);
    connect(&folderViewPresenter, &DirectoryPresenter::dirActivated,
            this, &Core::loadPath);
    connect(&folderViewPresenter, &DirectoryPresenter::parentDirActivated,
            this, &Core::loadParentDir);

    connect(&folderViewPresenter, &DirectoryPresenter::draggedOut,
            this, qOverload<QStringList>(&Core::onDraggedOut));

    connect(&folderViewPresenter, &DirectoryPresenter::droppedInto,
            fileOps, &FileOperationsController::movePathsTo);
    connect(&folderViewPresenter, &DirectoryPresenter::statusTextChanged,
            mw, &MW::setFolderStatusText);

    connect(scriptManager, &ScriptManager::error, mw, &MW::showError);

    connect(mw, &MW::opened,                this, &Core::loadPath);
    connect(mw, &MW::droppedIn,             this, &Core::onDropIn);
    connect(mw, &MW::copyRequested,         this, &Core::copyCurrentFile);
    connect(mw, &MW::moveRequested,         this, &Core::moveCurrentFile);
    connect(mw, &MW::copyUrlsRequested,     fileOps, &FileOperationsController::copyPathsTo);
    connect(mw, &MW::moveUrlsRequested,     fileOps, &FileOperationsController::movePathsTo);
    connect(mw, &MW::convertFormatRequested, this, &Core::convertSelectionToFormat);
    connect(mw, &MW::cropRequested,         this, &Core::crop);
    connect(mw, &MW::cropAndSaveRequested,  this, &Core::cropAndSave);
    connect(mw, &MW::saveAsClicked,         this, &Core::requestSavePath);
    connect(mw, &MW::saveRequested,         this, &Core::saveCurrentFile);
    connect(mw, &MW::saveAsRequested,       this, &Core::saveCurrentFileAs);
    connect(mw, &MW::resizeRequested,       this, &Core::resize);
    connect(mw, &MW::renameRequested,       this, &Core::renameCurrentSelection);
    connect(mw, &MW::sortingSelected,       this, &Core::sortBy);
    connect(mw, &MW::showFoldersChanged,    this, &Core::setFoldersDisplay);
    connect(mw, &MW::discardEditsRequested, this, &Core::discardEdits);
    connect(mw, &MW::draggedOut,            this, qOverload<>(&Core::onDraggedOut));

    connect(mw, &MW::playbackFinished, this, &Core::onPlaybackFinished);

    connect(mw, &MW::scalingRequested, this, &Core::scalingRequest);
    connect(model->scaler, &Scaler::scalingFinished, this, &Core::onScalingFinished);

    connect(model.get(), &DirectoryModel::fileAdded,      this, &Core::onFileAdded);
    connect(model.get(), &DirectoryModel::fileRemoved,    this, &Core::onFileRemoved);
    connect(model.get(), &DirectoryModel::fileRenamed,    this, &Core::onFileRenamed);
    connect(model.get(), &DirectoryModel::fileModified,   this, &Core::onFileModified);
    connect(model.get(), &DirectoryModel::loaded,         this, &Core::onModelLoaded);
    connect(model.get(), &DirectoryModel::imageReady,     this, &Core::onModelItemReady);
    connect(model.get(), &DirectoryModel::imageUpdated,   this, &Core::onModelItemUpdated);
    connect(model.get(), &DirectoryModel::sortingChanged, this, &Core::onModelSortingChanged);
    connect(model.get(), &DirectoryModel::loadFailed,     this, &Core::onLoadFailed);

    connect(&slideshowTimer, &QTimer::timeout, this, &Core::nextImageSlideshow);
}

void Core::initActions() {
    connect(actionManager, &ActionManager::nextImage, this, &Core::nextImage);
    connect(actionManager, &ActionManager::prevImage, this, &Core::prevImage);
    connect(actionManager, &ActionManager::fitWindow, mw, &MW::fitWindow);
    connect(actionManager, &ActionManager::fitWidth, mw, &MW::fitWidth);
    connect(actionManager, &ActionManager::fitNormal, mw, &MW::fitOriginal);
    connect(actionManager, &ActionManager::fitWindowStretch, mw, &MW::fitWindowStretch);
    connect(actionManager, &ActionManager::toggleFitMode, mw, &MW::switchFitMode);
    connect(actionManager, &ActionManager::toggleFullscreen, mw, &MW::triggerFullScreen);
    connect(actionManager, &ActionManager::lockZoom, mw, &MW::toggleLockZoom);
    connect(actionManager, &ActionManager::lockView, mw, &MW::toggleLockView);
    connect(actionManager, &ActionManager::zoomIn, mw, &MW::zoomIn);
    connect(actionManager, &ActionManager::zoomOut, mw, &MW::zoomOut);
    connect(actionManager, &ActionManager::zoomInCursor, mw, &MW::zoomInCursor);
    connect(actionManager, &ActionManager::zoomOutCursor, mw, &MW::zoomOutCursor);
    connect(actionManager, &ActionManager::scrollUp, mw, &MW::scrollUp);
    connect(actionManager, &ActionManager::scrollDown, mw, &MW::scrollDown);
    connect(actionManager, &ActionManager::scrollLeft, mw, &MW::scrollLeft);
    connect(actionManager, &ActionManager::scrollRight, mw, &MW::scrollRight);
    connect(actionManager, &ActionManager::resize, this, &Core::showResizeDialog);
    connect(actionManager, &ActionManager::flipH, this, &Core::flipH);
    connect(actionManager, &ActionManager::flipV, this, &Core::flipV);
    connect(actionManager, &ActionManager::rotateLeft, this, &Core::rotateLeft);
    connect(actionManager, &ActionManager::rotateRight, this, &Core::rotateRight);
    connect(actionManager, &ActionManager::openSettings, mw, &MW::showSettings);
    connect(actionManager, &ActionManager::crop, this, &Core::toggleCropPanel);
    connect(actionManager, &ActionManager::setWallpaper, this, &Core::setWallpaper);
    connect(actionManager, &ActionManager::open, this, &Core::showOpenDialog);
    connect(actionManager, &ActionManager::save, this, &Core::saveCurrentFile);
    connect(actionManager, &ActionManager::saveAs, this, &Core::requestSavePath);
    connect(actionManager, &ActionManager::exit, this, &Core::close);
    connect(actionManager, &ActionManager::closeFullScreenOrExit, mw, &MW::closeFullScreenOrExit);
    connect(actionManager, &ActionManager::removeFile, this, &Core::removePermanent);
    connect(actionManager, &ActionManager::moveToTrash, this, &Core::moveToTrash);
    connect(actionManager, &ActionManager::copyFile, mw, &MW::triggerCopyOverlay);
    connect(actionManager, &ActionManager::moveFile, mw, &MW::triggerMoveOverlay);
    connect(actionManager, &ActionManager::jumpToFirst, this, &Core::jumpToFirst);
    connect(actionManager, &ActionManager::jumpToLast, this, &Core::jumpToLast);
    connect(actionManager, &ActionManager::runScript, this, &Core::runScript);
    connect(actionManager, &ActionManager::pauseVideo, mw, &MW::pauseVideo);
    connect(actionManager, &ActionManager::seekVideoForward, mw, &MW::seekVideoForward);
    connect(actionManager, &ActionManager::seekVideoBackward, mw, &MW::seekVideoBackward);
    connect(actionManager, &ActionManager::frameStep, mw, &MW::frameStep);
    connect(actionManager, &ActionManager::frameStepBack, mw, &MW::frameStepBack);
    connect(actionManager, &ActionManager::folderView, this, &Core::enableFolderView);
    connect(actionManager, &ActionManager::documentView, this, &Core::enableDocumentView);
    connect(actionManager, &ActionManager::toggleFolderView, this, &Core::toggleFolderView);
    connect(actionManager, &ActionManager::reloadImage, this, qOverload<>(&Core::reloadImage));
    connect(actionManager, &ActionManager::copyFileClipboard, this, &Core::copyFileClipboard);
    connect(actionManager, &ActionManager::cutFile, this, &Core::cutFileClipboard);
    connect(actionManager, &ActionManager::copyPathClipboard, this, &Core::copyPathClipboard);
    connect(actionManager, &ActionManager::renameFile, this, &Core::showRenameDialog);
    connect(actionManager, &ActionManager::createDirectory, this, &Core::createDirectory);
    connect(actionManager, &ActionManager::contextMenu, mw, &MW::showContextMenu);
    connect(actionManager, &ActionManager::toggleTransparencyGrid, mw, &MW::toggleTransparencyGrid);
    connect(actionManager, &ActionManager::sortByName, this, &Core::sortByName);
    connect(actionManager, &ActionManager::sortByTime, this, &Core::sortByTime);
    connect(actionManager, &ActionManager::sortBySize, this, &Core::sortBySize);
    connect(actionManager, &ActionManager::toggleImageInfo, mw, &MW::toggleImageInfoOverlay);
    connect(actionManager, &ActionManager::stripMetadata, this, &Core::stripMetadata);
    connect(actionManager, &ActionManager::toggleShuffle, this, &Core::toggleShuffle);
    connect(actionManager, &ActionManager::toggleScalingFilter, mw, &MW::toggleScalingFilter);
    connect(actionManager, &ActionManager::showInDirectory, this, &Core::showInDirectory);
    connect(actionManager, &ActionManager::toggleMute, mw, &MW::toggleMute);
    connect(actionManager, &ActionManager::volumeUp, mw, &MW::volumeUp);
    connect(actionManager, &ActionManager::volumeDown, mw, &MW::volumeDown);
    connect(actionManager, &ActionManager::toggleSlideshow, this, &Core::toggleSlideshow);
    connect(actionManager, &ActionManager::goUp, this, &Core::loadParentDir);
    connect(actionManager, &ActionManager::discardEdits, this, &Core::discardEdits);
    connect(actionManager, &ActionManager::nextDirectory, this, &Core::nextDirectory);
    connect(actionManager, &ActionManager::prevDirectory, this, qOverload<>(&Core::prevDirectory));
    connect(actionManager, &ActionManager::print, this, &Core::print);
    connect(actionManager, &ActionManager::toggleFullscreenInfoBar, this, &Core::toggleFullscreenInfoBar);
    connect(actionManager, &ActionManager::toggleStatusFooter, this, &Core::toggleStatusFooter);
    connect(actionManager, &ActionManager::pasteFile, this, &Core::pasteFile);
    connect(actionManager, &ActionManager::toggleFolderViewTopBar, this, &Core::toggleFolderViewTopBar);
    connect(actionManager, &ActionManager::togglePlacesPanel, this, &Core::togglePlacesPanel);
}

void Core::loadTranslation() {
    if(!translator)
        translator = new QTranslator;
    QString trPathFallback = QCoreApplication::applicationDirPath() + "/translations";
#ifdef TRANSLATIONS_PATH
    QString trPath = QString(TRANSLATIONS_PATH);
#else
    QString trPath = trPathFallback;
#endif
    QString localeName = settings->language();
    if(localeName == "system")
        localeName = QLocale::system().name();
    if(localeName.isEmpty() || localeName == "en_US") {
        QApplication::removeTranslator(translator);
        return;
    }
    QString trFile = trPath + "/" + localeName;
    QString trFileFallback = trPathFallback + "/" + localeName;
    if(!translator->load(trFile)) {
        qCWarning(logCore) << "Could not load translation file: " << trFile;
        if(!translator->load(trFileFallback)) {
            qCWarning(logCore) << "Could not load translation file: " << trFileFallback;
            return;
        }
    }
    QApplication::installTranslator(translator);
}

void Core::onUpdate() {
    QVersionNumber lastVer = settings->lastVersion();


    if(lastVer < QVersionNumber(0,9,2)) {
        actionManager->resetDefaults("print");
        actionManager->resetDefaults("openSettings");
    }

#ifdef USE_OPENCV
    if(lastVer < QVersionNumber(0,9,0))
        settings->setScalingFilter(QI_FILTER_CV_CUBIC);
#endif

    actionManager->adjustFromVersion(lastVer);

    qCDebug(logCore) << "Updated: " << settings->lastVersion().toString() << ">" << appVersion.toString();
    mw->showMessage(tr("Updated: ") + settings->lastVersion().toString() + " > " + appVersion.toString(), 4000);
    settings->setLastVersion(appVersion);
}

void Core::onFirstRun() {
    mw->showMessage(tr("Welcome to ") + qApp->applicationName() + tr(" version ") + appVersion.toString() + "!", 4000);
    settings->setFirstRun(false);
    settings->setLastVersion(appVersion);
}

void Core::toggleShuffle() {
    if(shuffle) {
        mw->showMessage(tr("Shuffle mode: OFF"));
    } else {
        syncRandomizer();
        mw->showMessage(tr("Shuffle mode: ON"));
    }
    shuffle = !shuffle;
    updateInfoString();
}

void Core::toggleSlideshow() {
    if(slideshow) {
        stopSlideshow();
        mw->showMessage(tr("Slideshow: OFF"));

    } else {
        startSlideshow();
        mw->showMessage(tr("Slideshow: ON"));
    }
}

void Core::startSlideshow() {
    if(!slideshow) {
        slideshow = true;
        mw->setLoopPlayback(false);
        enableDocumentView();
        startSlideshowTimer();
        updateInfoString();
    }
}

void Core::stopSlideshow() {
    if(slideshow) {
        slideshow = false;
        mw->setLoopPlayback(true);
        slideshowTimer.stop();
        updateInfoString();
    }
}

void Core::onPlaybackFinished() {
    if(slideshow) {
        nextImageSlideshow();
    }
}

void Core::syncRandomizer() {
    if(model) {
        randomizer.setCount(model->fileCount());
        randomizer.shuffle();
        randomizer.setCurrent(model->indexOfFile(state.currentFilePath));
    }
}

void Core::onModelLoaded() {
    thumbPanelPresenter.reloadModel();
    folderViewPresenter.reloadModel();
    thumbPanelPresenter.selectAndFocus(state.currentFilePath);
    folderViewPresenter.selectAndFocus(state.currentFilePath);
    if(shuffle)
        syncRandomizer();
}

void Core::onDirectoryViewFileActivated(const QString& filePath) {
    // with showOtherFileTypes the grid can contain files thumbgrid cannot
    // render; keep the user in the folder view with a clear message instead of
    // switching to an empty document view
    if(settings->showOtherFileTypes()) {
        DocumentInfo info(filePath);
        if(info.type() == DocumentType::NONE) {
            mw->showMessage(tr("Cannot display this file type"));
            return;
        }
    }
    loadPath(filePath);
}

void Core::rotateLeft() {
    rotateByDegrees(-90);
}

void Core::rotateRight() {
    rotateByDegrees(90);
}

void Core::close() {
    mw->close();
}

void Core::removePermanent() {
    fileOps->removePaths(currentSelection(), false);
}

void Core::moveToTrash() {
    fileOps->removePaths(currentSelection(), true);
}


void Core::reloadImage() {
    reloadImage(selectedPath());
}

void Core::reloadImage(QString filePath) {
    if(model->isEmpty())
        return;
    model->reload(std::move(filePath));
}

// Removes all Exif/Iptc/Xmp metadata from the current file on disk (privacy).
void Core::stripMetadata() {
    if(model->isEmpty())
        return;
    QString path = selectedPath();
    auto img = model->getImage(path);
    if(!img)
        return;
    if(img->type() != STATIC && img->type() != ANIMATED) {
        mw->showMessage(tr("Cannot strip metadata from this file type"));
        return;
    }
    if(img->stripMetadata()) {
        reloadImage(path);
        mw->showMessageSuccess(tr("Metadata removed"));
    } else {
        mw->showMessage(tr("Could not remove metadata"));
    }
}

void Core::enableFolderView() {
    if(mw->currentViewMode() == MODE_FOLDERVIEW) {
        loadParentDir();
        return;
    }
    stopSlideshow();
    mw->enableFolderView();
}

void Core::enableDocumentView() {
    if(mw->currentViewMode() == MODE_DOCUMENT)
        return;
    mw->enableDocumentView();
    if(model && model->fileCount() && state.currentFilePath == "") {
        auto selection = folderViewPresenter.selectedPaths();
        auto selected = selection.isEmpty() ? "" : selection.first();
        // if it is a directory - ignore and just open the first file
        if(model->containsFile(selected))
            loadPath(selected);
        else
            loadPath(model->firstFile());
    }
}

void Core::toggleFolderView() {
    if(mw->currentViewMode() == MODE_FOLDERVIEW)
        enableDocumentView();
    else
        enableFolderView();
}

void Core::toggleFolderViewTopBar() {
    settings->setFolderViewTopBar(!settings->folderViewTopBar());
    settings->sendChangeNotification();
}

void Core::togglePlacesPanel() {
    settings->setPlacesPanel(!settings->placesPanel());
    settings->sendChangeNotification();
}

void Core::copyFileClipboard() {
    // In folder view copy the whole selection (files and/or directories) as a file
    // list, like a file manager. In image view keep copying the image's pixel data
    // too, so it can be pasted into image editors.
    if(mw->currentViewMode() == MODE_FOLDERVIEW) {
        copySelectionToClipboard(false);
        return;
    }
    if(model->isEmpty())
        return;

    QMimeData* mimeData = getMimeDataForImage(model->getImage(selectedPath()), TARGET_CLIPBOARD);

    // mimeData->text() should already contain an url
    QByteArray gnomeFormat = QByteArray("copy\n").append(QUrl(mimeData->text()).toEncoded());
    mimeData->setData("x-special/gnome-copied-files", gnomeFormat);
    mimeData->setData("application/x-kde-cutselection", "0");

    QApplication::clipboard()->setMimeData(mimeData);
    mw->showMessage(tr("File copied"));
}

void Core::cutFileClipboard() {
    copySelectionToClipboard(true);
}

// Puts the current selection (files and/or directories) onto the clipboard as a
// file list, using the cross-desktop "x-special/gnome-copied-files" + KDE markers
// so the operation interoperates with file managers (and our own pasteFile()).
void Core::copySelectionToClipboard(bool cut) {
    QStringList paths = currentSelection();
    if(paths.isEmpty())
        return;

    QMimeData *mimeData = new QMimeData();
    QList<QUrl> urls;
    QByteArray gnomeFormat = cut ? QByteArray("cut\n") : QByteArray("copy\n");
    for(int i = 0; i < paths.size(); i++) {
        QUrl url = QUrl::fromLocalFile(paths.at(i));
        urls << url;
        if(i)
            gnomeFormat.append('\n');
        gnomeFormat.append(url.toEncoded());
    }
    mimeData->setUrls(urls);
    mimeData->setData("x-special/gnome-copied-files", gnomeFormat);
    mimeData->setData("application/x-kde-cutselection", cut ? "1" : "0");

    QApplication::clipboard()->setMimeData(mimeData);
    if(cut)
        mw->showMessage(paths.count() > 1 ? tr("%1 items cut").arg(paths.count()) : tr("Item cut"));
    else
        mw->showMessage(paths.count() > 1 ? tr("%1 items copied").arg(paths.count()) : tr("Item copied"));
}

// Paste files/dirs from the clipboard into the currently open directory.
// Copy vs move (cut) is decided from the clipboard markers. Falls back to the
// old image-buffer / url open behaviour when there are no local files to paste.
void Core::pasteFile() {
    auto cb = QApplication::clipboard();
    auto mimeData = cb->mimeData();
    if(!mimeData)
        return;

    // The paste destination is the currently open folder, whether or not it
    // contains any image files. model->isEmpty() only counts image files, so
    // gating on it made pasting into an image-less folder fall back to
    // openFromClipboard() (which opens the source file in image view instead).
    QString destDirectory = model ? model->directoryPath() : "";

    // gather local file paths from the clipboard
    QStringList paths;
    if(mimeData->hasUrls()) {
        for(auto &url : mimeData->urls()) {
            QString localPath = url.toLocalFile();
            if(!localPath.isEmpty())
                paths << localPath;
        }
    }

    // No real files to paste, or no folder to paste into: keep the legacy behaviour
    // (open a url / save a clipboard image).
    if(paths.isEmpty() || destDirectory.isEmpty()) {
        openFromClipboard();
        return;
    }

    // detect a "cut" operation (move) via the gnome / kde clipboard markers
    bool cut = false;
    if(mimeData->hasFormat("x-special/gnome-copied-files")) {
        QByteArray data = mimeData->data("x-special/gnome-copied-files");
        cut = data.startsWith("cut");
    } else if(mimeData->hasFormat("application/x-kde-cutselection")) {
        cut = mimeData->data("application/x-kde-cutselection").trimmed() == "1";
    }

    // drop entries that are already in the destination (nothing to do there)
    QStringList toPaste;
    QDir destDir(destDirectory);
    for(auto &path : paths) {
        QFileInfo fi(path);
        if(!fi.exists() && !fi.isSymLink())
            continue;
        if(QDir(fi.absolutePath()) == destDir)
            continue;
        toPaste << path;
    }
    if(toPaste.isEmpty()) {
        mw->showMessage(tr("Nothing to paste"));
        return;
    }

    if(cut) {
        // A cut+paste moves the files, deleting them from the source folder, so
        // confirm it first like the other move path (movePathsTo) does. Returning
        // early keeps the clipboard intact so the cut selection survives a decline.
        if(!fileOps->movePathsTo(toPaste, destDirectory))
            return;
        mw->showMessage(toPaste.count() > 1 ? tr("%1 items moved").arg(toPaste.count()) : tr("Item moved"));
    } else {
        fileOps->interactiveCopy(toPaste, destDirectory);
        mw->showMessage(toPaste.count() > 1 ? tr("%1 items pasted").arg(toPaste.count()) : tr("Item pasted"));
    }
    // Clear the clipboard once the paste is done. For a move this stops a second
    // paste from failing on already-moved files; for a copy it gives the same
    // one-shot paste behaviour, so the same files aren't accidentally re-pasted.
    QApplication::clipboard()->clear();
}

void Core::copyPathClipboard() {
    if(model->isEmpty())
        return;
    // copy every selected path (one per line) so it covers a multi-selection in folder view
    QStringList paths = currentSelection();
    if(paths.isEmpty())
        return;
    QApplication::clipboard()->setText(paths.join("\n"));
    mw->showMessage(paths.count() > 1 ? tr("%1 paths copied").arg(paths.count()) : tr("Path copied"));
}

// open from clipboard
// todo: actual file paste into folderview (like filemanager)
void Core::openFromClipboard() {
    auto cb = QApplication::clipboard();
    auto mimeData = cb->mimeData();
    if(!mimeData)
        return;
    qCDebug(logCore) << "=====================================";
    qCDebug(logCore) << "hasUrls:" << mimeData->hasUrls();
    qCDebug(logCore) << "hasImage:" << mimeData->hasImage();
    qCDebug(logCore) << "hasText:" << mimeData->hasText();

    qCDebug(logCore) << "TEXT:" << cb->text();

    // try opening url
    if(mimeData->hasUrls()) {
        auto url = mimeData->urls().first();
        QString path = url.toLocalFile();
        if(path.isEmpty()) {
            qCWarning(logCore) << "Could not load url:" << url;
            qCWarning(logCore) << "Currently only local files are supported.";
        } else if(loadPath(path)) {
            return;
        }
    }
    // try to save buffer image then open
    if(mimeData->hasImage()) {
        auto image = cb->image();
        if(image.isNull())
            return;
        QString destPath;
        if(!model->isEmpty())
            destPath = model->directoryPath() + "/";
        else
            destPath = QDir::homePath() + "/";
        destPath.append("clipboard.png");
        destPath = mw->getSaveFileName(destPath);
        if(destPath.isEmpty())
            return;

        QFileInfo fi(destPath);
        QString ext = fi.suffix();
        int quality = 95;
        if(ext.compare("png", Qt::CaseInsensitive) == 0)
            quality = 30;
        else if(ext.compare("jpg", Qt::CaseInsensitive) == 0 || ext.compare("jpeg", Qt::CaseInsensitive) == 0)
            quality = settings->JPEGSaveQuality();

        bool success = SafeSave::withBackup(destPath, destPath, [&]() {
            return image.save(destPath, ext.toStdString().c_str(), quality);
        });
        if(success)
            loadPath(destPath);
    }
}

void Core::onDropIn(const QMimeData *mimeData, QObject* source) {
    // ignore self
    if(source == this)
        return;
    // check for our needed mime type, here a file or a list of files
    if(mimeData->hasUrls()) {
        QStringList pathList;
        QList<QUrl> urlList = mimeData->urls();
        // extract the local paths of the files
        for(int i = 0; i < urlList.size(); ++i)
            pathList.append(urlList.at(i).toLocalFile());
        // try to open first file in the list
        loadPath(pathList.first());
    }
}

// drag'n'drop
// drag image out of the program
void Core::onDraggedOut() {
    onDraggedOut(currentSelection());
}

void Core::onDraggedOut(QStringList paths) {
    if(paths.isEmpty())
        return;
    QMimeData *mimeData;
    // single selection, image
    if(paths.count() == 1 && model->containsFile(paths.first())) {
        mimeData = getMimeDataForImage(model->getImage(paths.last()), TARGET_DROP);
    } else { // multi-selection, or single directory. drag urls
        mimeData = new QMimeData();
        QList<QUrl> urlList;
        for(const auto& path : paths)
            urlList << QUrl::fromLocalFile(path);
        mimeData->setUrls(urlList);
    }
    mDrag = new QDrag(this);
    mDrag->setMimeData(mimeData);
    mDrag->exec(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, Qt::CopyAction);
}

QMimeData *Core::getMimeDataForImage(const std::shared_ptr<Image>& img, MimeDataTarget target) {
    QMimeData* mimeData = new QMimeData();
    if(!img)
        return mimeData;
    QString path = img->filePath();
    if(img->type() == STATIC) {
        if(img->isEdited()) {
            // TODO: cleanup temp files
            // meanwhile use generic name
            path = settings->tmpDir() + "image.png";
            // use faster compression for drag'n'drop
            int pngQuality = (target == TARGET_DROP) ? 80 : 30;
            img->getImage()->save(path, nullptr, pngQuality);
        }
    }
    // !!! using setImageData() while doing drag'n'drop hangs Xorg !!!
    // clipboard only!
    if(img->type() != VIDEO && target == TARGET_CLIPBOARD)
        mimeData->setImageData(*img->getImage().get());
    mimeData->setUrls({QUrl::fromLocalFile(path)});
    return mimeData;
}

void Core::sortBy(SortingMode mode) {
    model->setSortingMode(mode);
}

void Core::setFoldersDisplay(bool mode) {
    if(folderViewPresenter.showDirs() != mode)
        folderViewPresenter.setShowDirs(mode);
}

void Core::renameCurrentSelection(const QString& newName) {
    // Guard on the actual selection rather than the image file count: in folder
    // view the selection may be a directory, and the open directory can hold
    // zero images (fileCount()==0) while still containing folders to rename.
    if(selectedPath().isEmpty() || newName.isEmpty())
        return;
    FileOpResult result;
    model->renameEntry(selectedPath(), newName, false, result);
    if(result == FileOpResult::DESTINATION_DIR_EXISTS) {
        mw->toggleRenameOverlay(newName);
    } else if(result == FileOpResult::DESTINATION_FILE_EXISTS) {
        if(mw->showConfirmation(tr("File exists"), tr("Overwrite file?"))) {
            model->renameEntry(selectedPath(), newName, true, result);
        } else {
            // show rename dialog again
            mw->toggleRenameOverlay(newName);
        }
    }
    outputError(result);
}

FileOpResult Core::removeFile(const QString& filePath, bool trash) {
    if(model->isEmpty())
        return FileOpResult::NOTHING_TO_DO;

    bool reopen = false;
    std::shared_ptr<Image> img;
    if(state.currentFilePath == filePath) {
        img = model->getImage(filePath);
        if(img->type() == ANIMATED || img->type() == VIDEO) {
            mw->closeImage();
            reopen = true;
        }
    }
    FileOpResult result;
    model->removeFile(filePath, trash, result);
    if(result != FileOpResult::SUCCESS && reopen)
        guiSetImage(img);
    return result;
}

void Core::onFileRemoved(const QString& filePath, int index) {
    // no files left
    if(model->isEmpty()) {
        mw->closeImage();
        state.hasActiveImage = false;
        state.currentFilePath = "";
    }
    // image mode && removed current file
    if(state.currentFilePath == filePath) {
        if(mw->currentViewMode() == MODE_DOCUMENT) {
            if(!loadFileIndex(index, true, settings->usePreloader()))
                loadFileIndex(--index, true, settings->usePreloader());
        } else {
            state.hasActiveImage = false;
            state.currentFilePath = "";
        }
    }
    updateInfoString();
}

void Core::onFileRenamed(const QString& fromPath, int /*indexFrom*/, const QString& /*toPath*/, int indexTo) {
    if(state.currentFilePath == fromPath) {
        loadFileIndex(indexTo, true, settings->usePreloader());
    }
}

void Core::onFileAdded(const QString& filePath) {
    Q_UNUSED(filePath)
    // update file count
    updateInfoString();
    if(model->fileCount() == 1 && state.currentFilePath == "")
        loadFileIndex(0, false, settings->usePreloader());
}

// !! fixme
void Core::onFileModified(const QString& filePath) {
    Q_UNUSED(filePath)
}

void Core::outputError(const FileOpResult &error) const {
    if(error == FileOpResult::SUCCESS || error == FileOpResult::NOTHING_TO_DO)
        return;
    mw->showError(FileOperations::decodeResult(error));
    qCWarning(logCore) << FileOperations::decodeResult(error);
}

void Core::showOpenDialog() {
    mw->showOpenDialog(model->directoryPath());
}

void Core::showInDirectory() {
    if(!model)
        return;
    PlatformDesktop::showInDirectory(selectedPath(), model->directoryPath());
}

// Converts the currently selected file(s) to another image format.
// The converted copy is written next to the original (same base name, new extension).
// Only static images are supported; animated images and video are skipped.
void Core::convertSelectionToFormat(QString format) {
    if(!model || model->isEmpty())
        return;
    fileOps->convertToFormat(currentSelection(), std::move(format));
}

void Core::moveCurrentFile(const QString& destDirectory) {
    if(model->isEmpty())
        return;
    // pause updates to avoid flicker
    mw->setUpdatesEnabled(false);
    // move fails during file playback, so we close it temporarily
    mw->closeImage();
    FileOpResult result = fileOps->copyOrMoveFile(selectedPath(), destDirectory, true);
    if(result != FileOpResult::SUCCESS) {
        guiSetImage(model->getImage(selectedPath()));
        updateInfoString();
        if(result != FileOpResult::DESTINATION_FILE_EXISTS)
            outputError(result);
    }
    mw->setUpdatesEnabled(true);
    mw->repaint();
}

void Core::copyCurrentFile(const QString& destDirectory) {
    if(model->isEmpty())
        return;
    FileOpResult result = fileOps->copyOrMoveFile(selectedPath(), destDirectory, false);
    if(result != FileOpResult::SUCCESS && result != FileOpResult::DESTINATION_FILE_EXISTS)
        outputError(result);
}

void Core::toggleCropPanel() {
    if(model->isEmpty())
        return;
    if(mw->isCropPanelActive()) {
        mw->triggerCropPanel();
    } else if(state.hasActiveImage) {
        mw->triggerCropPanel();
    }
}

void Core::toggleFullscreenInfoBar() {
    mw->toggleFullscreenInfoBar();
}

void Core::toggleStatusFooter() {
    settings->setInfoBarWindowed(!settings->infoBarWindowed());
    settings->sendChangeNotification();
}

void Core::requestSavePath() {
    if(model->isEmpty())
        return;
    mw->showSaveDialog(selectedPath());
}

void Core::showResizeDialog() {
    if(model->isEmpty())
        return;
    auto img = model->getImage(selectedPath());
    if(img)
        mw->showResizeDialog(img->size());
}

// ---------------------------------------------------------------- image operations

std::shared_ptr<ImageStatic> Core::getEditableImage(const QString &filePath) {
    return std::dynamic_pointer_cast<ImageStatic>(model->getImage(filePath));
}

template<typename... Args>
void Core::edit_template(bool save, QString action, const std::function<QImage*(std::shared_ptr<const QImage>, Args...)>& editFunc, Args&&... as) {
    if(model->isEmpty())
        return;
    if(save && !mw->showConfirmation(action, tr("Perform action \"") + action + "\"? \n\n" + tr("Changes will be saved immediately.")))
        return;
    for(const auto& path : currentSelection()) {
        auto img = getEditableImage(path);
        if(!img)
            continue;
        img->setEditedImage(std::unique_ptr<const QImage>( editFunc(img->getImage(), std::forward<Args>(as)...) ));
        model->updateImage(path, std::static_pointer_cast<Image>(img));
        if(save) {
            saveFile(path);
            if(state.currentFilePath != path)
                model->unload(path);
        }
    }
    updateInfoString();
}

void Core::flipH() {
    edit_template((mw->currentViewMode() == MODE_FOLDERVIEW), tr("Flip horizontal"), { ImageLib::flippedH });
}

void Core::flipV() {
    edit_template((mw->currentViewMode() == MODE_FOLDERVIEW), tr("Flip vertical"), { ImageLib::flippedV });
}

void Core::rotateByDegrees(int degrees) {
    edit_template((mw->currentViewMode() == MODE_FOLDERVIEW), tr("Rotate"), { ImageLib::rotated }, degrees);
}

void Core::resize(QSize size) {
    edit_template(false, tr("Resize"), { ImageLib::scaled }, size, QI_FILTER_BILINEAR);
}

void Core::crop(QRect rect) {
    if(mw->currentViewMode() == MODE_FOLDERVIEW)
        return;
    edit_template(false, tr("Crop"), { ImageLib::cropped }, rect);
}

void Core::cropAndSave(QRect rect) {
    if(mw->currentViewMode() == MODE_FOLDERVIEW)
        return;
    edit_template(false, tr("Crop"), { ImageLib::cropped }, rect);
    saveFile(selectedPath());
    updateInfoString();
}

// ---------------------------------------------------------------- image operations ^

bool Core::saveFile(const QString &filePath) {
    return saveFile(filePath, filePath);
}

bool Core::saveFile(const QString &filePath, const QString &newPath) {
    if(!model->saveFile(filePath, newPath))
        return false;
    mw->hideSaveOverlay();
    // switch to the new file
    if(model->containsFile(newPath) && state.currentFilePath != newPath) {
        discardEdits();
        if(mw->currentViewMode() == MODE_DOCUMENT)
            loadPath(newPath);
    }
    return true;
}

void Core::saveCurrentFile() {
    saveCurrentFileAs(selectedPath());
}

void Core::saveCurrentFileAs(const QString& destPath) {
    if(model->isEmpty())
        return;
    if(saveFile(selectedPath(), destPath)) {
        mw->showMessageSuccess(tr("File saved"));
        updateInfoString();
    } else {
        mw->showError(tr("Could not save file"));
    }
}

void Core::discardEdits() {
    if(model->isEmpty())
        return;

    std::shared_ptr<Image> img = model->getImage(selectedPath());
    if(img && img->type() == STATIC) {
        auto imgStatic = dynamic_cast<ImageStatic *>(img.get());
        imgStatic->discardEditedImage();
        model->updateImage(selectedPath(), img);
    }
    mw->hideSaveOverlay();
}

// todo: remove?
QString Core::selectedPath() {
    if(!model)
        return "";
    else if(mw->currentViewMode() == MODE_FOLDERVIEW) {
        auto selection = folderViewPresenter.selectedPaths();
        return selection.isEmpty() ? "" : selection.last();
    } else
        return state.currentFilePath;
}

QStringList Core::currentSelection() {
    if(!model)
        return QStringList();
    else if(mw->currentViewMode() == MODE_FOLDERVIEW)
        return folderViewPresenter.selectedPaths();
    else
        return QStringList() << state.currentFilePath;
}

//------------------------

void Core::sortByName() {
    auto mode = SortingMode::SORT_NAME;
    if(model->sortingMode() == mode)
        mode = SortingMode::SORT_NAME_DESC;
    model->setSortingMode(mode);
}

void Core::sortByTime() {
    auto mode = SortingMode::SORT_TIME;
    if(model->sortingMode() == mode)
        mode = SortingMode::SORT_TIME_DESC;
    model->setSortingMode(mode);
}

void Core::sortBySize() {
    auto mode = SortingMode::SORT_SIZE;
    if(model->sortingMode() == mode)
        mode = SortingMode::SORT_SIZE_DESC;
    model->setSortingMode(mode);
}

void Core::showRenameDialog() {
    // isEmpty() only counts image files, so it wrongly blocks renaming a folder
    // when the open directory has no images. Gate on the selection instead.
    if(selectedPath().isEmpty())
        return;
    QFileInfo fi(selectedPath());
    mw->toggleRenameOverlay(fi.fileName());
}

void Core::createDirectory() {
    if(!model)
        return;
    QString parentDir = model->directoryPath();
    if(parentDir.isEmpty())
        return;
    bool ok = false;
    QString name = QInputDialog::getText(mw, tr("New Folder"), tr("Folder name:"),
                                         QLineEdit::Normal, tr("New Folder"), &ok);
    if(!ok)
        return;
    name = name.trimmed();
    if(name.isEmpty())
        return;
    if(name.contains('/') || name.contains('\\')) {
        mw->showError(tr("Folder name cannot contain path separators."));
        return;
    }
    FileOpResult result;
    model->createDirectory(parentDir + "/" + name, result);
    outputError(result);
}

void Core::runScript(const QString &scriptName) {
    if(model->isEmpty())
        return;
    scriptManager->runScript(scriptName, model->getImage(selectedPath()));
}

void Core::setWallpaper() {
    if(model->isEmpty() || selectedPath().isEmpty())
        return;
    auto img = model->getImage(selectedPath());
    if(img->type() != DocumentType::STATIC) {
        mw->showMessage("Set wallpaper: file not supported");
        return;
    }

    QString errorMessage;
    if(!PlatformDesktop::setWallpaper(selectedPath(), &errorMessage))
        mw->showMessage(errorMessage.isEmpty() ? "Action is not supported on this platform" : errorMessage, 3000);
}

void Core::print() {
    if(model->isEmpty())
        return;
    PrintDialog p(mw);
    auto img = model->getImage(selectedPath());
    if(!img) {
        mw->showError(tr("Could not open image"));
        return;
    }
    if(img->type() != DocumentType::STATIC) {
        mw->showError(tr("Can only print static images"));
        return;
    }
    QString pdfPath = model->directoryPath() + "/" + img->baseName() + ".pdf";
    p.setImage(img->getImage());
    p.setOutputPath(pdfPath);
    p.exec();
}

void Core::scalingRequest(QSize size, ScalingFilter filter) {
    // filter out an unnecessary scale request at statup
    if(mw->isVisible() && state.hasActiveImage) {
        std::shared_ptr<Image> forScale = model->getImage(state.currentFilePath);
        if(forScale) {
            model->scaler->requestScaled(ScalerRequest(forScale, size, state.currentFilePath, filter));
        }
    }
}

// TODO: don't use connect? otherwise there is no point using unique_ptr
void Core::onScalingFinished(QPixmap *scaled, const ScalerRequest& req) {
    if(state.hasActiveImage /* TODO: a better fix > */ && req.path == state.currentFilePath) {
        mw->onScalingFinished(std::unique_ptr<QPixmap>(scaled));
    } else {
        delete scaled;
    }
}

// reset state; clear cache; etc
void Core::reset() {
    state.hasActiveImage = false;
    state.currentFilePath = "";
    model->setDirectory("");
}

bool Core::loadPath(QString path) {
    if(path.isEmpty())
        return false;
    if(path.startsWith("file://", Qt::CaseInsensitive))
        path.remove(0, 7);

    stopSlideshow();
    state.delayModel = false;
    QFileInfo fileInfo(path);
    if(fileInfo.isDir()) {
        state.directoryPath = QDir(path).absolutePath();
        if(!settings->allowBrowseRoot() &&
           QDir::cleanPath(state.directoryPath) == QDir::cleanPath(QDir::rootPath())) {
            mw->showMessage(tr("Cannot view root folder."), 2200);
            return false;
        }
    } else if(fileInfo.isFile()) {
        state.directoryPath = fileInfo.absolutePath();
        if(model->directoryPath() != state.directoryPath)
            state.delayModel = true;
    } else {
        mw->showError(tr("Could not open path: ") + path);
        qCWarning(logCore) << "Could not open path: " << path;
        return false;
    }
    if(!state.delayModel && !setDirectory(state.directoryPath))
        return false;

    // load file / folderview
    if(fileInfo.isFile()) {
        int index = model->indexOfFile(fileInfo.absoluteFilePath());
        // DirectoryManager only checks file extensions via regex (performance reasons)
        // But in this case we force check mimetype
        if(index == -1) {
            QStringList types = settings->supportedMimeTypes();
            QMimeDatabase db;
            QMimeType type = db.mimeTypeForFile(fileInfo.absoluteFilePath());
            if(types.contains(type.name())) {
                if(model->forceInsert(fileInfo.absoluteFilePath())) {
                    index = model->indexOfFile(fileInfo.absoluteFilePath());
                }
            }
        }
        if(mw->currentViewMode() == MODE_FOLDERVIEW) {
            const bool loaded = loadFileIndex(index, false, settings->usePreloader());
            if(loaded)
                mw->enableDocumentView();
            return loaded;
        }
        mw->enableDocumentView();
        return loadFileIndex(index, false, settings->usePreloader());
    } else {
        mw->enableFolderView();
        return true;
    }
}

bool Core::setDirectory(const QString& path) {
    if(model->directoryPath() != path) {
        this->reset();
        if(!model->setDirectory(path)) {
            mw->showError(tr("Could not load folder: ") + path);
            return false;
        }
        mw->setDirectoryPath(path);
    }
    return true;
}

// Cheap, extension-based "is this a video?" check. Used at navigation time
// (before the file is loaded), so it must not touch file contents the way
// DocumentInfo's mime detection does.
static bool isVideoFile(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if(suffix.isEmpty())
        return false;
    const auto extensions = settings->videoFormats().values();
    for(const QByteArray &ext : extensions)
        if(suffix == QString::fromLatin1(ext))
            return true;
    return false;
}

bool Core::loadFileIndex(int index, bool async, bool preload) {
    if(!model)
        return false;
    auto entry = model->fileEntryAt(index);
    if(entry.path.isEmpty())
        return false;
    // We're navigating to another item. Let the viewer pause/blank the outgoing
    // item as needed for the upcoming type (determined cheaply from the
    // extension, since the file isn't loaded yet).
    bool nextIsVideo = isVideoFile(entry.path);
    bool currentIsVideo = state.currentImg && state.currentImg->type() == DocumentType::VIDEO;
    // Switching from a video to an image: load synchronously so the decoded
    // image is shown the instant the video is hidden. With an async load there
    // is a gap where neither is ready and the window background flashes through.
    if(currentIsVideo && !nextIsVideo)
        async = false;
    mw->prepareForLoad(nextIsVideo);
    state.currentFilePath = entry.path;
    model->unloadExcept(entry.path, preload);
    model->load(entry.path, async);
    if(preload) {
        model->preload(model->nextOf(entry.path));
        model->preload(model->prevOf(entry.path));
    }
    thumbPanelPresenter.selectAndFocus(entry.path);
    folderViewPresenter.selectAndFocus(entry.path);
    updateInfoString();
    return true;
}

void Core::loadParentDir() {
    if(model->directoryPath().isEmpty() || mw->currentViewMode() != MODE_FOLDERVIEW)
        return;
    stopSlideshow();
    QFileInfo currentDir(model->directoryPath());
    if(QDir::cleanPath(currentDir.absoluteFilePath()) == QDir::cleanPath(QDir::rootPath())) {
        mw->showMessage(tr("Already at root folder."), 2200);
        return;
    }
    QFileInfo parentDir(currentDir.absolutePath());
    // With root browsing disabled, the parent of e.g. /home is the unviewable
    // root - treat it as the ceiling and no-op instead of routing into
    // loadPath() and flashing a misleading "Cannot view root folder" toast.
    if(!settings->allowBrowseRoot() &&
       QDir::cleanPath(parentDir.absoluteFilePath()) == QDir::cleanPath(QDir::rootPath()))
        return;
    if(parentDir.exists() && parentDir.isReadable()) {
        QString childPath = currentDir.absoluteFilePath();
        loadPath(parentDir.absoluteFilePath());
        // Loading the parent repopulates the folder view, which resets the
        // selection to the first item (populate() -> selectAndFocus(0)) and
        // queues layout events. Defer selecting the subdir we came from until
        // those settle, otherwise our selection is immediately overwritten.
        QTimer::singleShot(0, this, [this, childPath]() {
            if(mw->currentViewMode() == MODE_FOLDERVIEW)
                folderViewPresenter.selectAndFocus(childPath);
        });
    }
}

void Core::nextDirectory() {
    if(model->directoryPath().isEmpty() || mw->currentViewMode() != MODE_DOCUMENT)
        return;
    stopSlideshow();
    QFileInfo currentDir(model->directoryPath());
    QFileInfo parentDir(currentDir.absolutePath());
    if(parentDir.exists() && parentDir.isReadable()) {
        DirectoryManager dm;
        if(!dm.setDirectory(parentDir.absoluteFilePath()))
            return;
        QString next = dm.nextOfDir(model->directoryPath());
        if(!next.isEmpty()) {
            if(!setDirectory(next))
                return;
            QFileInfo fi(next);
            mw->showMessageDirectory(fi.baseName());
            int index = nearestViewableIndex(0, 1);
            if(index >= 0)
                loadFileIndex(index, false, true);
        } else {
            mw->showMessageDirectoryEnd();
        }
    }
}

void Core::prevDirectory(bool selectLast) {
    if(model->directoryPath().isEmpty() || mw->currentViewMode() != MODE_DOCUMENT)
        return;
    QFileInfo currentDir(model->directoryPath());
    QFileInfo parentDir(currentDir.absolutePath());
    if(parentDir.exists() && parentDir.isReadable()) {
        DirectoryManager dm;
        dm.setDirectory(parentDir.absoluteFilePath());
        QString prev = dm.prevOfDir(model->directoryPath());
        if(!prev.isEmpty()) {
            if(!setDirectory(prev))
                return;
            QFileInfo fi(prev);
            mw->showMessageDirectory(fi.baseName());
            int index = selectLast ? nearestViewableIndex(model->fileCount() - 1, -1)
                                   : nearestViewableIndex(0, 1);
            if(index >= 0)
                loadFileIndex(index, false, true);
        } else {
            mw->showMessageDirectoryStart();
        }
    }
}

void Core::prevDirectory() {
    prevDirectory(false);
}

// With showOtherFileTypes the file list can contain entries thumbgrid cannot
// render; everything else in the model already matches the supported-formats
// filter, so the DocumentInfo probe is only needed when that setting is on.
bool Core::canDisplayFile(const QString &path) const {
    return !settings->showOtherFileTypes() || DocumentInfo(path).type() != DocumentType::NONE;
}

// Nearest model index at/after (step = 1) or at/before (step = -1) `from`
// whose file can be displayed. Returns -1 if there is none in that direction.
int Core::nearestViewableIndex(int from, int step) const {
    for(int i = from; i >= 0 && i < model->fileCount(); i += step) {
        if(canDisplayFile(model->filePathAt(i)))
            return i;
    }
    return -1;
}

// Advances the randomizer past unviewable entries. Bounded so a folder with
// no viewable files left cannot spin forever.
int Core::nextShuffledViewable(bool forward) {
    int index = forward ? randomizer.next() : randomizer.prev();
    for(int tries = 0; tries < model->fileCount() && !canDisplayFile(model->filePathAt(index)); tries++)
        index = forward ? randomizer.next() : randomizer.prev();
    return index;
}

void Core::nextImage() {
    if(mw->currentViewMode() == MODE_FOLDERVIEW || (model->isEmpty() && folderEndAction != FOLDER_END_GOTO_ADJACENT))
        return;
    stopSlideshow();
    if(shuffle) {
        loadFileIndex(nextShuffledViewable(true), true, false);
        return;
    }
    int newIndex = nearestViewableIndex(model->indexOfFile(state.currentFilePath) + 1, 1);
    if(newIndex < 0) {
        if(folderEndAction == FOLDER_END_LOOP) {
            newIndex = nearestViewableIndex(0, 1);
            if(newIndex < 0)
                return;
        } else if (folderEndAction == FOLDER_END_GOTO_ADJACENT) {
            nextDirectory();
            return;
        } else {
            if(!model->loaderBusy())
                mw->showMessageDirectoryEnd();
            return;
        }
    }
    loadFileIndex(newIndex, true, settings->usePreloader());
}

void Core::prevImage() {
    if(mw->currentViewMode() == MODE_FOLDERVIEW || (model->isEmpty() && folderEndAction != FOLDER_END_GOTO_ADJACENT))
        return;
    stopSlideshow();
    if(shuffle) {
        loadFileIndex(nextShuffledViewable(false), true, false);
        return;
    }

    int newIndex = nearestViewableIndex(model->indexOfFile(state.currentFilePath) - 1, -1);
    if(newIndex < 0) {
        if(folderEndAction == FOLDER_END_LOOP) {
            newIndex = nearestViewableIndex(model->fileCount() - 1, -1);
            if(newIndex < 0)
                return;
        } else if (folderEndAction == FOLDER_END_GOTO_ADJACENT) {
            prevDirectory(true);
            return;
        } else {
            if(!model->loaderBusy())
                mw->showMessageDirectoryStart();
            return;
        }
    }
    loadFileIndex(newIndex, true, settings->usePreloader());
}

void Core::nextImageSlideshow() {
    if(model->isEmpty() || mw->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(shuffle) {
        loadFileIndex(nextShuffledViewable(true), false, false);
    } else {
        int newIndex = nearestViewableIndex(model->indexOfFile(state.currentFilePath) + 1, 1);
        if(newIndex < 0 && loopSlideshow)
            newIndex = nearestViewableIndex(0, 1);
        if(newIndex < 0) {
            stopSlideshow();
            mw->showMessage(tr("End of directory."));
            return;
        }
        loadFileIndex(newIndex, false, true);
    }
    startSlideshowTimer();
}

void Core::startSlideshowTimer() {
    // start timer only for static images or single frame gifs
    // for proper gifs and video we get a playbackFinished() signal
    auto img = model->getImage(state.currentFilePath);
    if(!img) // unviewable file (e.g. with showOtherFileTypes enabled)
        return;
    if(img->type() == STATIC) {
        slideshowTimer.start();
    } else if(img->type() == ANIMATED) {
        auto anim = dynamic_cast<ImageAnimated *>(img.get());
        if(anim && anim->frameCount() <= 1)
            slideshowTimer.start();
    }
}

void Core::jumpToFirst() {
    if(model->isEmpty())
        return;
    stopSlideshow();
    int index = nearestViewableIndex(0, 1);
    if(index < 0)
        return;
    loadFileIndex(index, true, settings->usePreloader());
    mw->showMessageDirectoryStart();
}

void Core::jumpToLast() {
    if(model->isEmpty())
        return;
    stopSlideshow();
    int index = nearestViewableIndex(model->fileCount() - 1, -1);
    if(index < 0)
        return;
    loadFileIndex(index, true, settings->usePreloader());
    mw->showMessageDirectoryEnd();
}

void Core::onLoadFailed(const QString &path) {
    // background preloads of unviewable neighbors also fail; only report
    // failures for the file the user is actually looking at
    if(path != state.currentFilePath)
        return;
    mw->showMessage(tr("Cannot display file: ") + path);
    mw->closeImage();
}

void Core::onModelItemReady(const std::shared_ptr<Image>& img, const QString &path) {
    if(path == state.currentFilePath) {
        state.currentImg = img;
        guiSetImage(img);
        updateInfoString();
        if(state.delayModel) {
            this->showGui();
            state.delayModel = false;
            QTimer::singleShot(40, this, SLOT(modelDelayLoad()));
        }
        model->unloadExcept(state.currentFilePath, settings->usePreloader());
    }
}

void Core::modelDelayLoad() {
    model->setDirectory(state.directoryPath);
    mw->setDirectoryPath(state.directoryPath);
    model->updateImage(state.currentFilePath, state.currentImg);
    updateInfoString();
}

void Core::onModelItemUpdated(const QString& filePath) {
    if(filePath == state.currentFilePath) {
        guiSetImage(model->getImage(filePath));
        updateInfoString();
    }
}

void Core::onModelSortingChanged(SortingMode mode) {
    mw->onSortingChanged(mode);
    thumbPanelPresenter.reloadModel();
    thumbPanelPresenter.selectAndFocus(state.currentFilePath);
    folderViewPresenter.reloadModel();
    folderViewPresenter.selectAndFocus(state.currentFilePath);
}

void Core::guiSetImage(const std::shared_ptr<Image>& img) {
    state.hasActiveImage = true;
    if(!img) {
        mw->showMessage(tr("Error: could not load image."));
        return;
    }
    DocumentType type = img->type();
    if(type == STATIC) {
        mw->showImage(img->getPixmap());
    } else if(type == ANIMATED) {
        auto animated = dynamic_cast<ImageAnimated *>(img.get());
        mw->showAnimation(animated->getMovie());
    } else if(type == VIDEO) {
        auto video = dynamic_cast<Video *>(img.get());
        // workaround for mpv. If we play video while mainwindow is hidden we get black screen.
        // affects only initial startup (e.g. we open webm from file manager)
        showGui();
        mw->showVideo(video->filePath());
    } else if(type == TEXT) {
        mw->showText(img->filePath());
    }
    img->isEdited() ? mw->showSaveOverlay() : mw->hideSaveOverlay();
    mw->setExifInfo(settings->showFullMetadata() ? img->getAllTags() : img->getExifTags());
}

void Core::updateInfoString() {
    QSize imageSize(0,0);
    int imageDepth = 0;
    qint64 fileSize = 0;
    bool edited = false;

    if(model->isLoaded(state.currentFilePath)) {
        auto img = model->getImage(state.currentFilePath);
        imageSize = img->size();
        auto sourceImage = img->getImage();
        if(sourceImage)
            imageDepth = sourceImage->depth();
        fileSize  = img->fileSize();
        edited = img->isEdited();
    }
    int index = model->indexOfFile(state.currentFilePath);
    mw->setCurrentInfo(index,
                       model->fileCount(),
                       model->filePathAt(index),
                       model->fileNameAt(index),
                       imageSize,
                       imageDepth,
                       fileSize,
                       slideshow,
                       shuffle,
                       edited);
}
