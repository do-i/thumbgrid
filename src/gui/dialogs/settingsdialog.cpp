#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QSignalBlocker>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    this->setWindowTitle(tr("Preferences — ") + qApp->applicationName());

    ui->shortcutsTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);   
    ui->aboutAppTextBrowser->viewport()->setAutoFillBackground(false);
    ui->versionLabel->setText("" + QApplication::applicationVersion());
    ui->qtVersionLabel->setText(qVersion());
    ui->appIconLabel->setPixmap(QIcon(":/res/icons/common/logo/app/22.png").pixmap(22,22));
    ui->qtIconLabel->setPixmap(QIcon(":/res/icons/common/logo/3rdparty/qt22.png").pixmap(22,16));

    // fake combobox that acts as a menu button
    // less code than using pushbutton with menu
    // will be replaced with something custom later
    connect(ui->themeSelectorComboBox, qOverload<int>(&QComboBox::currentIndexChanged), [this](int index) {
        // leaving the custom theme: stash the current palette so previewing a
        // preset doesn't discard the user's custom colors
        if(mPrevThemeIndex == 4 && index != 4) {
            mCustomColors = collectColorScheme();
            mCustomColors.tid = COLORS_CUSTOM;
        }
        switch(index) {
            case 0: setColorScheme(ThemeStore::colorScheme(COLORS_BLACK));    break;
            case 1: setColorScheme(ThemeStore::colorScheme(COLORS_DARK));     break;
            case 2: setColorScheme(ThemeStore::colorScheme(COLORS_DARKBLUE)); break;
            case 3: setColorScheme(ThemeStore::colorScheme(COLORS_LIGHT));    break;
            case 4: setColorScheme(mCustomColors);                            break;
            case 5: setColorScheme(ThemeStore::colorScheme(COLORS_YELLOW));   break;
        }
        mPrevThemeIndex = index;
    });

    connect(ui->useSystemColorsCheckBox, &QCheckBox::toggled, [this](bool useSystemTheme) {
        if(useSystemTheme) {
            ui->themeSelectorComboBox->setCurrentIndex(-1);
            setColorScheme(ThemeStore::colorScheme(COLORS_SYSTEM));
        }
        else {
            readColorScheme();
        }
        ui->themeSelectorComboBox->setEnabled(!useSystemTheme);
        ui->colorConfigSubgroup->setEnabled(!useSystemTheme);
        ui->modifySystemSchemeLabel->setVisible(useSystemTheme);
    });

    connect(ui->modifySystemSchemeLabel, &ClickableLabel::clicked, [this]() {
        ui->useSystemColorsCheckBox->setChecked(false);
        ColorScheme custom = ThemeStore::colorScheme(COLORS_SYSTEM);
        custom.tid = COLORS_CUSTOM;
        setColorScheme(custom);
    });

    // App-wide
    ui->colorSelectorAccent->setDescription(tr("Accent"));
    ui->colorSelectorText->setDescription(tr("Text"));
    ui->colorSelectorIcons->setDescription(tr("Icons"));
    ui->colorSelectorBackground->setDescription(tr("Window background"));
    ui->colorSelectorWidget->setDescription(tr("Control background"));
    ui->colorSelectorWidgetBorder->setDescription(tr("Control border"));
    ui->colorSelectorScrollbar->setDescription(tr("Scrollbar"));
    // Grid
    ui->colorSelectorFolderview->setDescription(tr("Grid background"));
    ui->colorSelectorFolderviewPanel->setDescription(tr("Grid top bar"));
    ui->folderViewCellBackgroundColorSelector->setDescription(tr("Thumbnail cell"));
    ui->folderViewLabelBackgroundColorSelector->setDescription(tr("Filename label"));
    ui->folderViewSelectedLabelBackgroundColorSelector->setDescription(tr("Selected filename label"));
    ui->folderViewSelectionColorSelector->setDescription(tr("Selection highlight"));
    ui->folderViewParentIconColorSelector->setDescription(tr("Folder-icon tint"));
    // Content view
    ui->colorSelectorFullscreen->setDescription(tr("Fullscreen background"));
    ui->colorSelectorOverlay->setDescription(tr("Overlay background"));
    ui->colorSelectorOverlayText->setDescription(tr("Overlay text"));

    auto switchToCustom = [this](QColor) { markThemeCustom(); };
    connect(ui->colorSelectorAccent,                        &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorBackground,                    &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorFullscreen,                    &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorFolderview,                    &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorFolderviewPanel,               &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorText,                          &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorIcons,                         &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorWidget,                        &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorWidgetBorder,                  &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorScrollbar,                     &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorOverlay,                       &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->colorSelectorOverlayText,                   &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->folderViewCellBackgroundColorSelector,      &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->folderViewLabelBackgroundColorSelector,     &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->folderViewSelectedLabelBackgroundColorSelector, &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->folderViewSelectionColorSelector,           &ColorSelectorButton::colorChanged, this, switchToCustom);
    connect(ui->folderViewParentIconColorSelector,          &ColorSelectorButton::colorChanged, this, switchToCustom);

#ifndef USE_KDE_BLUR
    ui->blurBackgroundCheckBox->setEnabled(false);
#endif

#ifndef USE_MPV
    ui->videoPlaybackGroup->setEnabled(false);
    //ui->novideoInfoLabel->setHidden(false);
#else
    //ui->novideoInfoLabel->setHidden(true);
#endif

#ifdef USE_OPENCV
    ui->scalingQualityComboBox->addItem("Bilinear+sharpen (OpenCV)");
    ui->scalingQualityComboBox->addItem("Bicubic (OpenCV)");
    ui->scalingQualityComboBox->addItem("Bicubic+sharpen (OpenCV)");
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ui->memoryLimitSpinBox->setEnabled(false);
    ui->memoryLimitLabel->setEnabled(false);
#endif

    if(!settings->supportedFormats().contains("jxl"))
        ui->animatedJxlCheckBox->hide();

    setupSidebar();

    // setup radioBtn groups
    fitModeGrp.addButton(ui->fitModeWindow);
    fitModeGrp.addButton(ui->fitModeWidth);
    fitModeGrp.addButton(ui->fitMode1to1);
    fitModeGrp.addButton(ui->fitModeWindowStretch);
    folderEndGrp.addButton(ui->folderEndSwitchFolder);
    folderEndGrp.addButton(ui->folderEndNoAction);
    folderEndGrp.addButton(ui->folderEndLoop);
    zoomIndGrp.addButton(ui->zoomIndicatorAuto);
    zoomIndGrp.addButton(ui->zoomIndicatorOff);
    zoomIndGrp.addButton(ui->zoomIndicatorOn);

    // readable language names
    langs.insert("de_DE", "Deutsch");
    langs.insert("en_US", "English");
    langs.insert("es_ES", "Español");
    langs.insert("fr_FR", "Français");
    langs.insert("ja_JP", "日本語");
    langs.insert("tr_TR", "Türkçe");
    langs.insert("uk_UA", "Українська");
    langs.insert("zh_CN", "简体中文");
    // fill langs combobox, sorted by locale
    ui->langComboBox->addItems(langs.values());
    // insert system language entry manually at the beginning
    langs.insert("system", "System language");
    ui->langComboBox->insertItem(0, "System language");

    connect(this, &SettingsDialog::settingsChanged, settings, &Settings::sendChangeNotification);
    readSettings();

    adjustSizeToContents();
}
//------------------------------------------------------------------------------
SettingsDialog::~SettingsDialog() {
    delete ui;
}
//------------------------------------------------------------------------------
// an attempt to force minimum width to fit contents
void SettingsDialog::adjustSizeToContents() {
    // general tab
    ui->gridLayout->activate();
    ui->horizontalLayout_28->activate();
    ui->horizontalLayout_19->activate();
    ui->gridLayout_3->activate();
    ui->horizontalLayout_18->activate();
    ui->gridLayout_4->activate();
    ui->horizontalLayout_24->activate();
    ui->gridLayout_5->activate();
    ui->slideshowGroupContents->activate();
    ui->scrollAreaWidgetContents->layout()->activate();
    ui->scrollArea->setMinimumWidth(ui->scrollAreaWidgetContents->minimumSizeHint().width());
    // view tab
    ui->horizontalLayout_29->activate();
    ui->horizontalLayout_31->activate();
    ui->widget->layout()->activate();
    ui->scrollAreaWidgetContents_3->layout()->activate();
    ui->scrollArea_3->setMinimumWidth(ui->scrollAreaWidgetContents_3->minimumSizeHint().width());
    // container
    //ui->stackedWidget->layout()->activate();
    this->setMinimumWidth(sizeHint().width() + 22);

    //qDebug() << "window:" << this->sizeHint() << this->minimumSizeHint() << this->size();
    //qDebug() << "stackedwidget:" << ui->stackedWidget->sizeHint() << ui->stackedWidget->minimumSizeHint() << ui->stackedWidget->size();
    //qDebug() << "scrollarea:" << ui->scrollArea->sizeHint() << ui->scrollArea->minimumSizeHint() << ui->scrollArea->size();
    //qDebug() << "scrollareawidget:" << ui->scrollAreaWidgetContents->sizeHint() << ui->scrollAreaWidgetContents->minimumSizeHint() << ui->scrollAreaWidgetContents->size();
    //qDebug() << "grid" << ui->gridLayout_15->sizeHint();
    //qDebug() << "wtf" << ui->startInFolderViewCheckBox->sizeHint() << ui->startInFolderViewCheckBox->minimumSizeHint();
}
//------------------------------------------------------------------------------
void SettingsDialog::resetToDesktopTheme() {
    settings->setColorScheme(ThemeStore::colorScheme(ColorSchemes::COLORS_SYSTEM));
    this->readColorScheme();
}
//------------------------------------------------------------------------------
void SettingsDialog::setupSidebar() {

}
//------------------------------------------------------------------------------
void SettingsDialog::readSettings() {
    ui->loopSlideshowCheckBox->setChecked(settings->loopSlideshow());
    ui->videoPlaybackCheckBox->setChecked(settings->videoPlayback());
    ui->videoPlaybackGroupContents->setEnabled(settings->videoPlayback());
    ui->playSoundsCheckBox->setChecked(settings->playVideoSounds());
    ui->enablePanelCheckBox->setChecked(settings->panelEnabled());
    ui->thumbnailPanelGroupContents->setEnabled(settings->panelEnabled());
    ui->panelFullscreenOnlyCheckBox->setChecked(settings->panelFullscreenOnly());
    ui->squareThumbnailsCheckBox->setChecked(settings->squareThumbnails());
    ui->transparencyGridCheckBox->setChecked(settings->transparencyGrid());
    ui->enableSmoothScrollCheckBox->setChecked(settings->enableSmoothScroll());
    ui->usePreloaderCheckBox->setChecked(settings->usePreloader());
    ui->useThumbnailCacheCheckBox->setChecked(settings->useThumbnailCache());
    ui->smoothUpscalingCheckBox->setChecked(settings->smoothUpscaling());
    ui->expandImageCheckBox->setChecked(settings->expandImage());
    ui->expandImagesGroupContents->setEnabled(settings->expandImage());
    ui->smoothAnimatedImagesCheckBox->setChecked(settings->smoothAnimatedImages());
    ui->bgOpacitySlider->setValue(static_cast<int>(settings->backgroundOpacity() * 100));
    ui->blurBackgroundCheckBox->setChecked(settings->blurBackground());
    ui->sortingComboBox->setCurrentIndex(settings->sortingMode());
    ui->confirmDeleteCheckBox->setChecked(settings->confirmDelete());
    ui->confirmTrashCheckBox->setChecked(settings->confirmTrash());
    ui->unlockMinZoomCheckBox->setChecked(settings->unlockMinZoom());
    ui->sortFoldersCheckBox->setChecked(settings->sortFolders());
    ui->allowBrowseRootCheckBox->setChecked(settings->allowBrowseRoot());
    ui->trackpadDetectionCheckBox->setChecked(settings->trackpadDetection());
    ui->clickableEdgesCheckBox->setChecked(settings->clickableEdges());
    ui->clickableEdgesVisibleCheckBox->setChecked(settings->clickableEdgesVisible());
    ui->clickableEdgesVisibleCheckBox->setEnabled(settings->clickableEdges());
    ui->showHiddenFilesCheckBox->setChecked(settings->showHiddenFiles());
    ui->showOtherFileTypesCheckBox->setChecked(settings->showOtherFileTypes());

    if(settings->zoomIndicatorMode() == INDICATOR_ENABLED)
        ui->zoomIndicatorOn->setChecked(true);
    else if(settings->zoomIndicatorMode() == INDICATOR_AUTO)
        ui->zoomIndicatorAuto->setChecked(true);
    else
        ui->zoomIndicatorOff->setChecked(true);
    ui->showInfoBarFullscreen->setChecked(settings->infoBarFullscreen());
    ui->showInfoBarWindowed->setChecked(settings->infoBarWindowed());
    ui->showExtendedInfoTitle->setChecked(settings->windowTitleExtendedInfo());
    ui->showFullMetadataCheckBox->setChecked(settings->showFullMetadata());
    ui->cursorAutohideCheckBox->setChecked(settings->cursorAutohide());
    ui->keepFitModeCheckBox->setChecked(settings->keepFitMode());
    if(settings->focusPointIn1to1Mode() == FOCUS_TOP)
        ui->focus1to1Top->setChecked(true);
    else if(settings->focusPointIn1to1Mode() == FOCUS_CENTER)
        ui->focus1to1Center->setChecked(true);
    else
        ui->focus1to1Cursor->setChecked(true);
    ui->slideshowIntervalSpinBox->setValue(settings->slideshowInterval());
    ui->imageScrollingComboBox->setCurrentIndex(settings->imageScrolling());
    ui->saveOverlayCheckBox->setChecked(settings->showSaveOverlay());
    ui->unloadThumbsCheckBox->setChecked(settings->unloadThumbs());
    if(settings->thumbPanelStyle() == TH_PANEL_SIMPLE)
        ui->thumbStyleSimple->setChecked(true);
    else
        ui->thumbStyleExtended->setChecked(true);
    ui->animatedJxlCheckBox->setChecked(settings->jxlAnimation());
    ui->autoResizeWindowCheckBox->setChecked(settings->autoResizeWindow());
    ui->panelCenterSelectionCheckBox->setChecked(settings->panelCenterSelection());
    ui->useFixedZoomLevelsCheckBox->setChecked(settings->useFixedZoomLevels());
    ui->zoomLevels->setText(settings->zoomLevels());

    if(settings->defaultViewMode() == MODE_FOLDERVIEW)
        ui->startInFolderViewCheckBox->setChecked(true);
    else
        ui->startInFolderViewCheckBox->setChecked(false);
    ui->folderViewTopBarCheckBox->setChecked(settings->folderViewTopBar());
    ui->folderViewFontSizeSpinBox->setValue(settings->folderViewFontPointSize());

    if(settings->folderEndAction() == FOLDER_END_NO_ACTION)
        ui->folderEndNoAction->setChecked(true);
    else if(settings->folderEndAction() == FOLDER_END_LOOP)
        ui->folderEndLoop->setChecked(true);
    else
        ui->folderEndSwitchFolder->setChecked(true);

    ui->mpvLineEdit->setText(settings->mpvBinary());

    ui->zoomStepSlider->setValue(static_cast<int>(settings->zoomStep() * 100.f));
    onZoomStepSliderChanged(ui->zoomStepSlider->value());

    ui->mouseScrollingSpeedSlider->setValue(static_cast<int>((settings->mouseScrollingSpeed() - 0.5f) / 0.25f));
    onMouseScrollingSpeedSliderChanged(ui->mouseScrollingSpeedSlider->value());

    ui->autoResizeLimitSlider->setValue(static_cast<int>(settings->autoResizeLimit() / 5.f));
    onAutoResizeLimitSliderChanged(ui->autoResizeLimitSlider->value());

    ui->JPEGQualitySlider->setValue(settings->JPEGSaveQuality());
    onJPEGQualitySliderChanged(ui->JPEGQualitySlider->value());

    ui->expandLimitSlider->setValue(settings->expandLimit());
    onExpandLimitSliderChanged(ui->expandLimitSlider->value());

    // thumbnailer threads
    ui->thumbnailerThreadsSlider->setValue(settings->thumbnailerThreadCount());
    onThumbnailerThreadsSliderChanged(ui->thumbnailerThreadsSlider->value());

    ui->memoryLimitSpinBox->setValue(settings->memoryAllocationLimit());

    // language
    QString langName = langs.value(settings->language());
    if(langName.isEmpty() || ui->langComboBox->findText(langName) == -1)
        ui->langComboBox->setCurrentText("en_US");
    else
        ui->langComboBox->setCurrentText(langName);

    // ##### fit mode #####
    if(settings->imageFitMode() == FIT_WINDOW)
        ui->fitModeWindow->setChecked(true);
    else if(settings->imageFitMode() == FIT_WIDTH)
        ui->fitModeWidth->setChecked(true);
    else if(settings->imageFitMode() == FIT_WINDOW_STRETCH)
        ui->fitModeWindowStretch->setChecked(true);
    else
        ui->fitMode1to1->setChecked(true);

    // ##### UI #####
    ui->scalingQualityComboBox->setCurrentIndex(settings->scalingFilter());
    ui->fullscreenCheckBox->setChecked(settings->fullscreenMode());
    ui->pinPanelCheckBox->setChecked(settings->panelPinned());
    ui->panelPositionComboBox->setCurrentIndex(settings->panelPosition());

    // reduce by 10x to have nice granular control in qslider
    ui->panelSizeSlider->setValue(settings->panelPreviewsSize() / 10);

    ui->useSystemColorsCheckBox->setChecked(settings->useSystemColorScheme());
    ui->modifySystemSchemeLabel->setVisible(settings->useSystemColorScheme());
    ui->themeSelectorComboBox->setEnabled(!settings->useSystemColorScheme());
    ui->colorConfigSubgroup->setEnabled(!settings->useSystemColorScheme());
    
    readColorScheme();
    readShortcuts();
    readScripts();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettings() {
    // wait for all background stuff to finish
    if(QThreadPool::globalInstance()->activeThreadCount()) {
        QThreadPool::globalInstance()->waitForDone();
    }

    settings->setLoopSlideshow(ui->loopSlideshowCheckBox->isChecked());
    settings->setFullscreenMode(ui->fullscreenCheckBox->isChecked());
    if(ui->fitModeWindow->isChecked())
        settings->setImageFitMode(FIT_WINDOW);
    else if(ui->fitModeWidth->isChecked())
        settings->setImageFitMode(FIT_WIDTH);
    else if(ui->fitModeWindowStretch->isChecked())
        settings->setImageFitMode(FIT_WINDOW_STRETCH);
    else
        settings->setImageFitMode(FIT_ORIGINAL);

    settings->setLanguage(langs.key(ui->langComboBox->currentText()));

    settings->setVideoPlayback(ui->videoPlaybackCheckBox->isChecked());
    settings->setPlayVideoSounds(ui->playSoundsCheckBox->isChecked());
    settings->setPanelEnabled(ui->enablePanelCheckBox->isChecked());
    settings->setPanelFullscreenOnly(ui->panelFullscreenOnlyCheckBox->isChecked());
    settings->setSquareThumbnails(ui->squareThumbnailsCheckBox->isChecked());
    settings->setTransparencyGrid(ui->transparencyGridCheckBox->isChecked());
    settings->setShowHiddenFiles(ui->showHiddenFilesCheckBox->isChecked());
    settings->setShowOtherFileTypes(ui->showOtherFileTypesCheckBox->isChecked());
    settings->setEnableSmoothScroll(ui->enableSmoothScrollCheckBox->isChecked());
    settings->setUsePreloader(ui->usePreloaderCheckBox->isChecked());
    settings->setUseThumbnailCache(ui->useThumbnailCacheCheckBox->isChecked());
    settings->setSmoothUpscaling(ui->smoothUpscalingCheckBox->isChecked());
    settings->setExpandImage(ui->expandImageCheckBox->isChecked());
    settings->setSmoothAnimatedImages(ui->smoothAnimatedImagesCheckBox->isChecked());

    settings->setBackgroundOpacity(static_cast<qreal>(ui->bgOpacitySlider->value()) / 100.f);
    settings->setBlurBackground(ui->blurBackgroundCheckBox->isChecked());
    settings->setSortingMode(static_cast<SortingMode>(ui->sortingComboBox->currentIndex()));
    settings->setConfirmDelete(ui->confirmDeleteCheckBox->isChecked());
    settings->setConfirmTrash(ui->confirmTrashCheckBox->isChecked());
    settings->setUnlockMinZoom(ui->unlockMinZoomCheckBox->isChecked());
    settings->setSortFolders(ui->sortFoldersCheckBox->isChecked());
    settings->setAllowBrowseRoot(ui->allowBrowseRootCheckBox->isChecked());
    settings->setTrackpadDetection(ui->trackpadDetectionCheckBox->isChecked());
    settings->setClickableEdges(ui->clickableEdgesCheckBox->isChecked());
    settings->setClickableEdgesVisible(ui->clickableEdgesVisibleCheckBox->isChecked());

    if(ui->zoomIndicatorOn->isChecked())
        settings->setZoomIndicatorMode(INDICATOR_ENABLED);
    else if(ui->zoomIndicatorAuto->isChecked())
        settings->setZoomIndicatorMode(INDICATOR_AUTO);
    else
        settings->setZoomIndicatorMode(INDICATOR_DISABLED);
    settings->setInfoBarFullscreen(ui->showInfoBarFullscreen->isChecked());
    settings->setInfoBarWindowed(ui->showInfoBarWindowed->isChecked());
    settings->setWindowTitleExtendedInfo(ui->showExtendedInfoTitle->isChecked());
    settings->setShowFullMetadata(ui->showFullMetadataCheckBox->isChecked());
    settings->setCursorAutohide(ui->cursorAutohideCheckBox->isChecked());
    settings->setKeepFitMode(ui->keepFitModeCheckBox->isChecked());
    if(ui->focus1to1Top->isChecked())
        settings->setFocusPointIn1to1Mode(FOCUS_TOP);
    else if(ui->focus1to1Center->isChecked())
        settings->setFocusPointIn1to1Mode(FOCUS_CENTER);
    else
        settings->setFocusPointIn1to1Mode(FOCUS_CURSOR);

    settings->setSlideshowInterval(ui->slideshowIntervalSpinBox->value());

    if(ui->startInFolderViewCheckBox->isChecked())
        settings->setDefaultViewMode(MODE_FOLDERVIEW);
    else
        settings->setDefaultViewMode(MODE_DOCUMENT);
    settings->setFolderViewTopBar(ui->folderViewTopBarCheckBox->isChecked());
    settings->setFolderViewFontPointSize(ui->folderViewFontSizeSpinBox->value());

    if(ui->folderEndNoAction->isChecked())
        settings->setFolderEndAction(FOLDER_END_NO_ACTION);
    else if(ui->folderEndLoop->isChecked())
        settings->setFolderEndAction(FOLDER_END_LOOP);
    else
        settings->setFolderEndAction(FOLDER_END_GOTO_ADJACENT);

    settings->setMpvBinary(ui->mpvLineEdit->text());
    settings->setScalingFilter(static_cast<ScalingFilter>(ui->scalingQualityComboBox->currentIndex()));
    settings->setImageScrolling(static_cast<ImageScrolling>(ui->imageScrollingComboBox->currentIndex()));
    settings->setShowSaveOverlay(ui->saveOverlayCheckBox->isChecked());
    settings->setUnloadThumbs(ui->unloadThumbsCheckBox->isChecked());
    if(ui->thumbStyleSimple->isChecked())
        settings->setThumbPanelStyle(TH_PANEL_SIMPLE);
    else
        settings->setThumbPanelStyle(TH_PANEL_EXTENDED);
    settings->setJxlAnimation(ui->animatedJxlCheckBox->isChecked());
    settings->setAutoResizeWindow(ui->autoResizeWindowCheckBox->isChecked());
    settings->setPanelCenterSelection(ui->panelCenterSelectionCheckBox->isChecked());
    settings->setUseFixedZoomLevels(ui->useFixedZoomLevelsCheckBox->isChecked());
    settings->setZoomLevels(ui->zoomLevels->text());

    settings->setPanelPinned(ui->pinPanelCheckBox->isChecked());
    int panelPos = ui->panelPositionComboBox->currentIndex();
    settings->setPanelPosition(static_cast<PanelPosition>(panelPos));

    settings->setPanelPreviewsSize(ui->panelSizeSlider->value() * 10);

    settings->setJPEGSaveQuality(ui->JPEGQualitySlider->value());
    settings->setZoomStep(static_cast<qreal>(ui->zoomStepSlider->value() / 100.f));
    settings->setMouseScrollingSpeed(static_cast<qreal>(0.5f + (ui->mouseScrollingSpeedSlider->value() * 0.25f)));
    settings->setAutoResizeLimit(ui->autoResizeLimitSlider->value() * 5);
    settings->setExpandLimit(ui->expandLimitSlider->value());
    settings->setThumbnailerThreadCount(ui->thumbnailerThreadsSlider->value());
    settings->setMemoryAllocationLimit(ui->memoryLimitSpinBox->value());

    bool useSystemColors = ui->useSystemColorsCheckBox->isChecked();
    settings->setUseSystemColorScheme(useSystemColors);

    if(useSystemColors)
        settings->setColorScheme(ThemeStore::colorScheme(COLORS_SYSTEM));
    else
        saveColorScheme();
    saveShortcuts();

    scriptManager->saveScripts();
    actionManager->saveShortcuts();
    emit settingsChanged();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettingsAndClose() {
    saveSettings();
    this->close();
}
//------------------------------------------------------------------------------
void SettingsDialog::readColorScheme() {
    auto colors = settings->colorScheme();
    // seed the custom cache: the active palette if it's custom, otherwise the
    // separately-stored custom palette that presets don't overwrite
    mCustomColors = (colors.tid == COLORS_CUSTOM) ? colors : settings->customColorScheme();
    mCustomColors.tid = COLORS_CUSTOM;
    setColorScheme(colors);
    mPrevThemeIndex = ui->themeSelectorComboBox->currentIndex();
}

void SettingsDialog::setColorScheme(ColorScheme colors) {
    QSignalBlocker blocker(ui->themeSelectorComboBox);
    switch (colors.tid) {
        case COLORS_LIGHT:     ui->themeSelectorComboBox->setCurrentIndex(3); break;
        case COLORS_BLACK:     ui->themeSelectorComboBox->setCurrentIndex(0); break;
        case COLORS_DARK:      ui->themeSelectorComboBox->setCurrentIndex(1); break;
        case COLORS_DARKBLUE:  ui->themeSelectorComboBox->setCurrentIndex(2); break;
        case COLORS_CUSTOM:    ui->themeSelectorComboBox->setCurrentIndex(4); break;
        case COLORS_YELLOW:    ui->themeSelectorComboBox->setCurrentIndex(5); break;
        default:               ui->themeSelectorComboBox->setCurrentIndex(-1); break;
    }
    ui->colorSelectorAccent->setColor(colors.accent);
    ui->colorSelectorBackground->setColor(colors.background);
    ui->colorSelectorFullscreen->setColor(colors.background_fullscreen);
    ui->colorSelectorFolderview->setColor(colors.folderview);
    ui->colorSelectorFolderviewPanel->setColor(colors.folderview_topbar);
    ui->colorSelectorText->setColor(colors.text);
    ui->colorSelectorIcons->setColor(colors.icons);
    ui->colorSelectorWidget->setColor(colors.widget);
    ui->colorSelectorWidgetBorder->setColor(colors.widget_border);
    ui->colorSelectorOverlay->setColor(colors.overlay);
    ui->colorSelectorOverlayText->setColor(colors.overlay_text);
    ui->colorSelectorScrollbar->setColor(colors.scrollbar);
    ui->folderViewCellBackgroundColorSelector->setColor(colors.folderview_cell_bg);
    ui->folderViewLabelBackgroundColorSelector->setColor(colors.folderview_label_bg);
    ui->folderViewSelectedLabelBackgroundColorSelector->setColor(colors.folderview_selected_label_bg);
    ui->folderViewSelectionColorSelector->setColor(colors.folderview_selection);
    ui->folderViewParentIconColorSelector->setColor(colors.folderview_parent_icon);
}

//------------------------------------------------------------------------------
ColorScheme SettingsDialog::collectColorScheme() {
    BaseColorScheme base;
    base.accent = ui->colorSelectorAccent->color();
    base.background = ui->colorSelectorBackground->color();
    base.background_fullscreen = ui->colorSelectorFullscreen->color();
    base.folderview = ui->colorSelectorFolderview->color();
    base.folderview_topbar = ui->colorSelectorFolderviewPanel->color();
    base.text = ui->colorSelectorText->color();
    base.icons = ui->colorSelectorIcons->color();
    base.widget = ui->colorSelectorWidget->color();
    base.widget_border = ui->colorSelectorWidgetBorder->color();
    base.overlay = ui->colorSelectorOverlay->color();
    base.overlay_text = ui->colorSelectorOverlayText->color();
    base.scrollbar = ui->colorSelectorScrollbar->color();
    base.folderview_cell_bg = ui->folderViewCellBackgroundColorSelector->color();
    base.folderview_label_bg = ui->folderViewLabelBackgroundColorSelector->color();
    base.folderview_selected_label_bg = ui->folderViewSelectedLabelBackgroundColorSelector->color();
    base.folderview_selection = ui->folderViewSelectionColorSelector->color();
    base.folderview_parent_icon = ui->folderViewParentIconColorSelector->color();
    base.tid = selectedThemeTid();
    return ColorScheme(base);
}
//------------------------------------------------------------------------------
void SettingsDialog::saveColorScheme() {
    settings->setColorScheme(collectColorScheme());
    settings->saveTheme();
}
//------------------------------------------------------------------------------
void SettingsDialog::markThemeCustom() {
    QSignalBlocker blocker(ui->themeSelectorComboBox);
    ui->themeSelectorComboBox->setCurrentIndex(4);
    mPrevThemeIndex = 4;
}
//------------------------------------------------------------------------------
int SettingsDialog::selectedThemeTid() const {
    switch(ui->themeSelectorComboBox->currentIndex()) {
        case 0: return COLORS_BLACK;
        case 1: return COLORS_DARK;
        case 2: return COLORS_DARKBLUE;
        case 3: return COLORS_LIGHT;
        case 4: return COLORS_CUSTOM;
        case 5: return COLORS_YELLOW;
        default: return COLORS_CUSTOM;
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::readShortcuts() {
    ui->shortcutsTableWidget->clearContents();
    ui->shortcutsTableWidget->setRowCount(0);
    const ActionManager::ShortcutMap &shortcuts = actionManager->allShortcuts();
    for(auto ctx = shortcuts.cbegin(); ctx != shortcuts.cend(); ++ctx) {
        QMapIterator<QString, QString> i(ctx.value());
        while(i.hasNext()) {
            i.next();
            addShortcutToTable(i.value(), i.key(), ctx.key());
        }
    }
}
//------------------------------------------------------------------------------
ViewMode SettingsDialog::contextAtRow(int row) {
    QTableWidgetItem *item = ui->shortcutsTableWidget->item(row, 2);
    return item ? ActionManager::contextFromString(item->data(Qt::UserRole).toString())
                : MODE_DOCUMENT;
}
//------------------------------------------------------------------------------
// Selects the row matching both the shortcut and its context (the same key may
// appear once per context).
void SettingsDialog::selectShortcutRow(const QString &shortcut, ViewMode context) {
    for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
        if(ui->shortcutsTableWidget->item(i, 1)->text() == shortcut && contextAtRow(i) == context) {
            ui->shortcutsTableWidget->selectRow(i);
            return;
        }
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::readScripts() {
    ui->scriptsListWidget->clear();
    const QMap<QString, Script> scripts = scriptManager->allScripts();
    QMapIterator<QString, Script> i(scripts);
    while(i.hasNext()) {
        i.next();
        addScriptToList(i.key());
    }
}
//------------------------------------------------------------------------------
// does not check if the shortcut already there
void SettingsDialog::addScriptToList(const QString &name) {
    if(name.isEmpty())
        return;

    QListWidget *list = ui->scriptsListWidget;
    QListWidgetItem *nameItem = new QListWidgetItem(name);
    nameItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    list->insertItem(ui->scriptsListWidget->count(), nameItem);
    list->sortItems(Qt::AscendingOrder);
}
//------------------------------------------------------------------------------
void SettingsDialog::addScript() {
    ScriptEditorDialog w;
    if(w.exec()) {
        if(w.scriptName().isEmpty())
            return;
        scriptManager->addScript(w.scriptName(), w.script());
        readScripts();
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript() {
    int row = ui->scriptsListWidget->currentRow();
    if(row >= 0) {
        QString name = ui->scriptsListWidget->currentItem()->text();
        editScript(name);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(QListWidgetItem* item) {
    if(item) {
        editScript(item->text());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(QString name) {
    ScriptEditorDialog w(name, scriptManager->getScript(name));
    if(w.exec()) {
        if(w.scriptName().isEmpty())
            return;
        scriptManager->addScript(w.scriptName(), w.script());
        readScripts();
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::removeScript() {
    int row = ui->scriptsListWidget->currentRow();
    if(row >= 0) {
        QString scriptName = ui->scriptsListWidget->currentItem()->text();
        delete ui->scriptsListWidget->takeItem(row);
        saveShortcuts();
        actionManager->removeAllShortcuts("s:"+scriptName);
        readShortcuts();
        scriptManager->removeScript(scriptName);
    }
}
//------------------------------------------------------------------------------
// does not check if the shortcut already there
void SettingsDialog::addShortcutToTable(const QString &action, const QString &shortcut, ViewMode context) {
    if(action.isEmpty() || shortcut.isEmpty())
        return;

    int row = ui->shortcutsTableWidget->rowCount();
    ui->shortcutsTableWidget->setRowCount(row + 1);
    QTableWidgetItem *actionItem = new QTableWidgetItem(action);
    actionItem->setTextAlignment(Qt::AlignCenter);
    ui->shortcutsTableWidget->setItem(row, 0, actionItem);
    QTableWidgetItem *shortcutItem = new QTableWidgetItem(shortcut);
    shortcutItem->setTextAlignment(Qt::AlignCenter);
    ui->shortcutsTableWidget->setItem(row, 1, shortcutItem);
    QTableWidgetItem *contextItem = new QTableWidgetItem(
        (context == MODE_FOLDERVIEW) ? tr("Folder") : tr("Document"));
    contextItem->setTextAlignment(Qt::AlignCenter);
    contextItem->setData(Qt::UserRole, ActionManager::contextToString(context));
    ui->shortcutsTableWidget->setItem(row, 2, contextItem);
    // EFFICIENCY
    ui->shortcutsTableWidget->sortByColumn(0, Qt::AscendingOrder);
}
//------------------------------------------------------------------------------
void SettingsDialog::addShortcut() {
    ShortcutCreatorDialog w;
    if(!w.exec())
        return;
    // Only replace a binding for the same key in the same context; the same key in
    // the other context is a separate, valid binding.
    for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
        if(ui->shortcutsTableWidget->item(i, 1)->text() == w.selectedShortcut()
           && contextAtRow(i) == w.selectedContext()) {
            removeShortcutAt(i);
            break;
        }
    }
    addShortcutToTable(w.selectedAction(), w.selectedShortcut(), w.selectedContext());
    selectShortcutRow(w.selectedShortcut(), w.selectedContext());
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcutAt(int row) {
    if(row > 0 && row >= ui->shortcutsTableWidget->rowCount())
        return;
    ui->shortcutsTableWidget->removeRow(row);
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut(int row) {
    if(row >= 0) {
        ShortcutCreatorDialog w;
        w.setWindowTitle(tr("Edit shortcut"));
        w.setAction(ui->shortcutsTableWidget->item(row, 0)->text());
        w.setShortcut(ui->shortcutsTableWidget->item(row, 1)->text());
        w.setContext(contextAtRow(row));
        if(!w.exec())
            return;
        // remove itself
        removeShortcutAt(row);
        // remove anything we are replacing (same key, same context)
        for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
            if(ui->shortcutsTableWidget->item(i, 1)->text() == w.selectedShortcut()
               && contextAtRow(i) == w.selectedContext()) {
                removeShortcutAt(i);
                break;
            }
        }
        // re-add
        addShortcutToTable(w.selectedAction(), w.selectedShortcut(), w.selectedContext());
        // re-select
        selectShortcutRow(w.selectedShortcut(), w.selectedContext());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut() {
    editShortcut(ui->shortcutsTableWidget->currentRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcut() {
    removeShortcutAt(ui->shortcutsTableWidget->currentRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::saveShortcuts() {
    actionManager->removeAllShortcuts();
    for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
        actionManager->addShortcut(contextAtRow(i),
                                   ui->shortcutsTableWidget->item(i, 1)->text(),
                                   ui->shortcutsTableWidget->item(i, 0)->text());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::resetShortcuts() {
    actionManager->resetDefaults();
    readShortcuts();
}
//------------------------------------------------------------------------------
void SettingsDialog::resetZoomLevels() {
    ui->zoomLevels->setText(settings->defaultZoomLevels());
}
//------------------------------------------------------------------------------
void SettingsDialog::selectMpvPath() {
    QFileDialog dialog;
    QString file;
    file = dialog.getOpenFileName(this, tr("Navigate to mpv binary"), "", "mpv*");
    if(!file.isEmpty()) {
        ui->mpvLineEdit->setText(file);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::onExpandLimitSliderChanged(int value) {
    if(value == 0)
        ui->expandLimitLabel->setText("-");
    else
        ui->expandLimitLabel->setText(QString::number(value) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onJPEGQualitySliderChanged(int value) {
    ui->JPEGQualityLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onZoomStepSliderChanged(int value) {
    ui->zoomStepLabel->setText(QString::number(value / 100.f, 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onMouseScrollingSpeedSliderChanged(int value) {
    ui->mouseScrollingSpeedLabel->setText(QString::number(0.5f + (value*0.25f), 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbnailerThreadsSliderChanged(int value) {
    ui->thumbnailerThreadsLabel->setText(QString::number(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onBgOpacitySliderChanged(int value) {
    ui->bgOpacityPercentLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onAutoResizeLimitSliderChanged(int value) {
    ui->autoResizeLimit->setText(QString::number(value * 5.f, 'f', 0) + "%");
}
//------------------------------------------------------------------------------
int SettingsDialog::exec() {
    return QDialog::exec();
}

void SettingsDialog::switchToPage(int number) {
    ui->sideBar2->selectEntry(number);
}
