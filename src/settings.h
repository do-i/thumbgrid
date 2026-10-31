#pragma once

#include <QObject>
#include <QSettings>
#include <QApplication>
#include <QStandardPaths>
#include <QDebug>
#include <QImageReader>
#include <QStringList>
#include <QColor>
#include <QPalette>
#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
#include <QMap>
#include <QFont>
#include <QFontMetrics>
#include <QVersionNumber>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <cstdint>
#include "utils/script.h"
#include "themestore.h"

enum SortingMode : std::uint8_t {
    SORT_NAME,
    SORT_NAME_DESC,
    SORT_SIZE,
    SORT_SIZE_DESC,
    SORT_TIME,
    SORT_TIME_DESC
};

enum ImageFitMode : std::uint8_t {
    FIT_WINDOW,
    FIT_WIDTH,
    FIT_ORIGINAL,
    FIT_WINDOW_STRETCH,
    FIT_FREE
};

enum PanelPosition : std::uint8_t {
    PANEL_TOP,
    PANEL_BOTTOM,
    PANEL_LEFT,
    PANEL_RIGHT
};

// Stored values: 2 was an OpenCV bilinear filter (now read as SMOOTH), 3 and 4 its bicubic ones.
enum ScalingFilter : std::uint8_t {
    QI_FILTER_NEAREST = 0,
    QI_FILTER_BILINEAR = 1,             // "Smooth": Qt smooth scaling
    QI_FILTER_HIGH_QUALITY = 3,         // bicubic; below half size smooth + sharpen
    QI_FILTER_HIGH_QUALITY_SHARPEN = 4, // also sharpened when shrinking less (resize dialog)
    QI_FILTER_HIGH_QUALITY_SOFT = 5     // internal, never stored: no sharpening below half size
};

enum ZoomIndicatorMode : std::uint8_t {
    INDICATOR_DISABLED,
    INDICATOR_ENABLED,
    INDICATOR_AUTO
};

enum ImageFocusPoint : std::uint8_t {
    FOCUS_TOP,
    FOCUS_CENTER,
    FOCUS_CURSOR
};

enum ImageScrolling : std::uint8_t {
    SCROLL_NONE,
    SCROLL_BY_TRACKPAD,
    SCROLL_BY_TRACKPAD_AND_WHEEL
};

enum ViewMode : std::uint8_t {
    MODE_DOCUMENT,
    MODE_FOLDERVIEW,
    MODE_GLOBAL,
    // shortcut context only: keys of the open Copy/Move overlay
    MODE_COPYMOVE
};

enum FolderEndAction : std::uint8_t {
    FOLDER_END_NO_ACTION,
    FOLDER_END_LOOP,
    FOLDER_END_GOTO_ADJACENT
};

enum FolderViewMode : std::uint8_t {
    FV_SIMPLE,
    FV_EXTENDED,
    FV_EXT_FOLDERS
};

enum ThumbPanelStyle : std::uint8_t {
    TH_PANEL_SIMPLE,
    TH_PANEL_EXTENDED
};

// when the viewer hides an idle mouse pointer
enum CursorAutohideMode : std::uint8_t {
    CURSOR_AUTOHIDE_NEVER,
    CURSOR_AUTOHIDE_FULLSCREEN,
    CURSOR_AUTOHIDE_ALWAYS
};

class Settings : public QObject
{
    Q_OBJECT
public:
    static Settings* getInstance();
    ~Settings() override;
    QStringList supportedMimeTypes();
    QList<QByteArray> supportedFormats();
    QString supportedFormatsFilter();
    QString supportedFormatsRegex();
    int panelPreviewsSize();
    void setPanelPreviewsSize(int size);
    bool usePreloader();
    void setUsePreloader(bool mode);
    bool fullscreenMode();
    void setFullscreenMode(bool mode);
    ImageFitMode imageFitMode();
    void setImageFitMode(ImageFitMode mode);
    QRect windowGeometry();
    void setWindowGeometry(QRect geometry);
    bool playVideoSounds();
    void setPlayVideoSounds(bool mode);
    bool videoHardwareDecoding();
    void setVideoHardwareDecoding(bool mode);
    bool showVideoControls();
    void setShowVideoControls(bool mode);
    void setVolume(int vol);
    int volume();
    QString thumbnailCacheDir();
    QString mpvBinary();
    void setMpvBinary(const QString& path);
    PanelPosition panelPosition();
    void setPanelPosition(PanelPosition);
    bool loopSlideshow();
    void setLoopSlideshow(bool mode);
    // Shortcuts are scoped per context (ViewMode). shortcuts.json stores each
    // context as action -> sorted keys, with optional per-action primacy.
    void readShortcuts(QMap<ViewMode, QMap<QString, QString>> &shortcuts);
    void saveShortcuts(const QMap<ViewMode, QMap<QString, QString>> &shortcuts);
    // Hoists document+grid duplicates into global and drops repeats of global, as readShortcuts() stores them.
    // Public so preset defaults get the identical pass.
    static bool collapseShortcutContexts(QMap<ViewMode, QMap<QString, QString>> &shortcuts);
    void readShortcutPrimary(QMap<ViewMode, QMap<QString, QString>> &primary);
    void saveShortcutPrimary(const QMap<ViewMode, QMap<QString, QString>> &primary);
    void readDisabledShortcuts(QMap<ViewMode, QStringList> &disabled);
    // overrides of the scroll actions' fallbacks: action -> action, "none" for none
    void readShortcutFallbacks(QMap<QString, QString> &fallbacks);
    void saveShortcutFallbacks(const QMap<QString, QString> &fallbacks);
    // the same for their fallbacks in a video
    void readShortcutVideoFallbacks(QMap<QString, QString> &fallbacks);
    void saveShortcutVideoFallbacks(const QMap<QString, QString> &fallbacks);
    void saveDisabledShortcuts(const QMap<ViewMode, QStringList> &disabled);
    // [Shortcuts] pointers: selected preset and whether the mapping diverged (mapping is in shortcuts.json)
    QString selectedPreset();
    void setSelectedPreset(const QString &id);
    bool shortcutsModified();
    void setShortcutsModified(bool modified);
    bool shortcutsJsonExists();
    bool panelEnabled();
    void setPanelEnabled(bool mode);
    int lastDisplay();
    void setLastDisplay(int display);
    bool squareThumbnails();
    void setSquareThumbnails(bool mode);
    bool transparencyGrid();
    void setTransparencyGrid(bool mode);
    // the two alternating checker colors behind transparent images (viewer and grid)
    QColor transparencyGridColor1();
    QColor transparencyGridColor2();
    static QColor defaultTransparencyGridColor1();
    static QColor defaultTransparencyGridColor2();
    // persist=false only swaps the in-memory colors, for a live preview
    void setTransparencyGridColors(const QColor &color1, const QColor &color2, bool persist = true);
    bool enableSmoothScroll();
    void setEnableSmoothScroll(bool mode);
    bool useThumbnailCache();
    void setUseThumbnailCache(bool mode);
    int thumbnailerMemCacheLimit();
    void setThumbnailerMemCacheLimit(int limitMB);
    QStringList savedPaths();
    void setSavedPaths(const QStringList& paths);
    bool savedPathsStored();
    void clearSavedPaths();
    void clearBookmarks();
    void clearDuplicateFinderTargets();
    QString duplicateHashCachePath();
    QString settingsFilePath();
    QStringList clearOnExitStores();
    void setClearOnExitStores(const QStringList &ids);
    QString tmpDir();
    int thumbnailerThreadCount();
    void setThumbnailerThreadCount(int count);
    int duplicateSearchThreadCount();
    void setDuplicateSearchThreadCount(int count);
    QByteArray duplicateFinderGeometry();
    void setDuplicateFinderGeometry(const QByteArray &geometry);
    QByteArray fileInfoDialogGeometry();
    void setFileInfoDialogGeometry(const QByteArray &geometry);
    QByteArray duplicateFinderHeader();
    void setDuplicateFinderHeader(const QByteArray &header);
    int duplicateFinderSimilarity();
    void setDuplicateFinderSimilarity(int value);
    bool duplicateFinderRecursive();
    void setDuplicateFinderRecursive(bool mode);
    bool duplicateFinderRotated();
    void setDuplicateFinderRotated(bool mode);
    bool duplicateFinderMirrored();
    void setDuplicateFinderMirrored(bool mode);
    QStringList duplicateFinderTargets();
    void setDuplicateFinderTargets(const QStringList &targets);
    bool smoothUpscaling();
    void setSmoothUpscaling(bool mode);
    void setExpandImage(bool mode);
    bool expandImage();
    ScalingFilter scalingFilter();
    void setScalingFilter(ScalingFilter mode);
    // High quality filter: sharpen images shrunk below 50%
    bool scalingSharpen();
    void setScalingSharpen(bool mode);
    // filter used by the Resize dialog; separate from the viewer's zoom filter
    ScalingFilter resizeFilter();
    void setResizeFilter(ScalingFilter mode);
    bool smoothAnimatedImages();
    void setSmoothAnimatedImages(bool mode);
    bool panelFullscreenOnly();
    void setPanelFullscreenOnly(bool mode);
    QVersionNumber lastVersion();
    void setLastVersion(QVersionNumber &ver);
    qreal backgroundOpacity();
    void setBackgroundOpacity(qreal value);
    bool blurBackground();
    void setBlurBackground(bool mode);
    void setSortingMode(SortingMode mode);
    SortingMode sortingMode();    
    void readScripts(QMap<QString, Script> &scripts);
    void saveScripts(const QMap<QString, Script> &scripts);
    int folderViewIconSize();
    void setFolderViewIconSize(int value);
    int folderViewFontPointSize();
    void setFolderViewFontPointSize(int value);
    QColor folderViewLabelBackgroundColor();
    QColor folderViewSelectionColor();
    QColor folderViewParentIconColor();
    QColor folderViewSelectedLabelBackgroundColor();
    QColor folderViewCellBackgroundColor();

    bool firstRun();
    void setFirstRun(bool mode);

    void sync();
    CursorAutohideMode cursorAutohide();
    void setCursorAutohide(CursorAutohideMode mode);

    bool infoBarFullscreen();
    void setInfoBarFullscreen(bool mode);
    bool infoBarWindowed();
    void setInfoBarWindowed(bool mode);

    bool windowTitleExtendedInfo();
    void setWindowTitleExtendedInfo(bool mode);

    bool maximizedWindow();
    void setMaximizedWindow(bool mode);

    bool keepFitMode();
    void setKeepFitMode(bool mode);

    int expandLimit();
    void setExpandLimit(int value);

    float zoomStep();
    void setZoomStep(float value);
    int JPEGSaveQuality();
    void setJPEGSaveQuality(int value);
    void setZoomIndicatorMode(ZoomIndicatorMode mode);
    ZoomIndicatorMode zoomIndicatorMode();
    void setFocusPointIn1to1Mode(ImageFocusPoint mode);
    ImageFocusPoint focusPointIn1to1Mode();
    bool placesPanel();
    void setPlacesPanel(bool mode);
    bool folderViewTopBar();
    void setFolderViewTopBar(bool mode);

    QStringList bookmarks();
    void setBookmarks(const QStringList& paths);
    bool placesPanelBookmarksExpanded();
    void setPlacesPanelBookmarksExpanded(bool mode);
    bool placesPanelTreeExpanded();
    void setPlacesPanelTreeExpanded(bool mode);

    void setSlideshowInterval(int ms);
    int slideshowInterval();

    ImageScrolling imageScrolling();
    void setImageScrolling(ImageScrolling mode);

    int placesPanelWidth();
    void setPlacesPanelWidth(int width);

    ViewMode defaultViewMode();
    void setDefaultViewMode(ViewMode mode);

    FolderEndAction folderEndAction();
    void setFolderEndAction(FolderEndAction mode);

    const ColorScheme& colorScheme();
    ColorScheme customColorScheme();
    void setColorScheme(ColorScheme scheme);
    void saveTheme();

    bool videoPlayback();
    void setVideoPlayback(bool mode);

    // [General]/theme pointer: "system", "custom" or a preset name
    int selectedThemeTid();
    void setSelectedThemeTid(int tid);

    void loadStylesheet();

    bool showSaveOverlay();
    void setShowSaveOverlay(bool mode);
    bool confirmDelete();
    void setConfirmDelete(bool mode);
    bool confirmTrash();
    void setConfirmTrash(bool mode);

    FolderViewMode folderViewMode();
    void setFolderViewMode(FolderViewMode mode);
    bool folderViewShowInfo();
    void setFolderViewShowInfo(bool mode);
    bool folderViewPreviewFit();
    void setFolderViewPreviewFit(bool mode);

    const QMultiMap<QByteArray, QByteArray> videoFormats() const;

    bool printLandscape();
    void setPrintLandscape(bool mode);
    bool printPdfDefault();
    void setPrintPdfDefault(bool mode);
    bool printColor();
    void setPrintColor(bool mode);
    bool printFitToPage();
    void setPrintFitToPage(bool mode);
    QString lastPrinter();
    void setLastPrinter(const QString& name);
    int shortcutsSortColumn();
    void setShortcutsSortColumn(int column);
    Qt::SortOrder shortcutsSortOrder();
    void setShortcutsSortOrder(Qt::SortOrder order);
    bool unloadThumbs();
    void setUnloadThumbs(bool mode);
    ThumbPanelStyle thumbPanelStyle();
    void setThumbPanelStyle(ThumbPanelStyle mode);

    bool jxlAnimation();
    void setJxlAnimation(bool mode);
    bool showFullMetadata();
    void setShowFullMetadata(bool mode);
    bool autoResizeWindow();
    void setAutoResizeWindow(bool mode);
    int autoResizeLimit();
    void setAutoResizeLimit(int percent);

    bool panelPinned();
    void setPanelPinned(bool mode);
    int memoryAllocationLimit();
    void setMemoryAllocationLimit(int limitMB);
    bool panelCenterSelection();
    void setPanelCenterSelection(bool mode);
    QString language();
    void setLanguage(const QString& lang);

    QString defaultZoomLevels();
    QString zoomLevels();
    void setZoomLevels(const QString& levels);
    bool useFixedZoomLevels();
    void setUseFixedZoomLevels(bool mode);
    bool unlockMinZoom();
    void setUnlockMinZoom(bool mode);
    bool sortFolders();
    void setSortFolders(bool mode);
    bool allowBrowseRoot();
    void setAllowBrowseRoot(bool mode);
    bool trackpadDetection();
    void setTrackpadDetection(bool mode);

    bool clickableEdges();
    void setClickableEdges(bool mode);
    bool clickableEdgesVisible();
    void setClickableEdgesVisible(bool mode);

    float mouseScrollingSpeed();
    void setMouseScrollingSpeed(float value);

    bool showHiddenFiles();
    void setShowHiddenFiles(bool mode);
    bool showOtherFileTypes();
    void setShowOtherFileTypes(bool mode);

private:
    explicit Settings(QObject *parent = nullptr);
    QSettings *settingsConf, *themeConf;
    QString mShortcutsJsonPath;
    QDir *mTmpDir, *mThumbCacheDir;
    ColorScheme mColorScheme;
    // read once: the grid paints these on every transparent thumbnail
    QColor mTransparencyGridColors[2];
    void loadTransparencyGridColors();
    QMultiMap<QByteArray, QByteArray> mVideoFormatsMap; // [mimetype, format]
    void loadTheme();
    void createColorVariants();

    void setupCache();
    void fillVideoFormats();

    // Keys are grouped by settings-dialog category; runtime state lives under [State].
    QVariant readSetting(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void writeSetting(const QString &key, const QVariant &value);
    void runVersionedSettingsMigrations(const QVersionNumber &lastVer);
    void runConfigRecoveryMigrations(const QString &confDir);
    // One-time move of flat top-level keys into their category groups, and of
    // the old useSystemColorScheme boolean / theme.ini tid into [General]/theme.
    void migrateConfigGroups();
    // Removes keys left behind by features that no longer exist.
    void pruneRetiredSettings();

signals:
    void settingsChanged();

public slots:
    void sendChangeNotification();

};

extern Settings *settings;
