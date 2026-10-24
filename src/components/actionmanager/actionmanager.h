#pragma once

#include <QObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMap>
#include <QHash>
#include <QDebug>
#include <QStringList>
#include <cstdint>
#include "utils/actions.h"
#include "shortcutbuilder.h"
#include "components/scriptmanager/scriptmanager.h"
#include "settings.h"
#include "shortcutpresetstore.h"

enum ActionType : std::uint8_t {
    ACTION_INVALID,
    ACTION_NORMAL,
    ACTION_SCRIPT
};

class ActionManager : public QObject {
    Q_OBJECT
public:
    static ActionManager* getInstance();
    ~ActionManager() override;
    // Shortcuts are scoped to a context. MODE_GLOBAL bindings work everywhere;
    // the active view context wins when it uses the same key.
    using ContextMap = QMap<QString, QString>;                 // <shortcut, action>
    using ShortcutMap = QMap<ViewMode, ContextMap>;            // context -> bindings

    // The active context is tracked here and used by processEvent(). It is kept in
    // sync with the visible screen by CentralWidget.
    void setContext(ViewMode context);
    ViewMode currentContext() const;
    // Copy / Move and clip edit: keys that only work while that is open, and
    // that global bindings don't reach
    static bool isOverlayContext(ViewMode context);
    // While set, processEvent() tries this context's bindings first and falls
    // back to the active context's (clip editing sets MODE_CLIPEDIT)
    void setOverlayContext(ViewMode context);
    void clearOverlayContext();
    static QString contextToString(ViewMode context);
    static ViewMode contextFromString(const QString &name);

    bool processEvent(QInputEvent*);
    void addShortcut(ViewMode context, const QString &keys, const QString &action);
    void resetDefaults();
    void resetDefaults(ViewMode context);
    void resetDefaults(const QString& action);
    QString actionForShortcut(ViewMode context, const QString &keys);
    // Whether a viewer may scroll the picture view's wheel itself (keeping smooth
    // and trackpad scrolling) rather than leave it to the binding: true while an
    // unmodified wheel is bound to scrollOrPrevImage/scrollOrNextImage, or the
    // wheel is unbound.
    bool viewerHandlesWheel(QWheelEvent *event);
    const QString shortcutForAction(ViewMode context, const QString& action);
    // Returns every shortcut bound to action across all contexts (used for key filters).
    const QStringList shortcutsForAction(const QString& action);
    QStringList actionList();
    const ShortcutMap& allShortcuts();
    const ShortcutMap& allDefaultShortcuts();
    void removeShortcut(ViewMode context, const QString &keys);
    void removeAllShortcuts();
    void removeAllShortcuts(const QString& actionName);
    QString keyForNativeScancode(quint32 scanCode);
    void adjustFromVersion(const QVersionNumber& lastVer);
    void saveShortcuts();
    // Preset selection (delegates to ShortcutPresetStore + Settings).
    static QList<PresetInfo> availablePresets();
    static QString selectedPreset();
    void applyPreset(const QString &id);
    // A scroll action (scrollUp ... scrollRightFine) runs its fallback action
    // when there is nothing to scroll that way: previous/next file for the
    // arrows and video seek for the fine ones by default, editable per action
    // in Settings > Shortcuts. Overrides map action -> fallback ("none": none).
    static bool hasScrollFallback(const QString &action);
    static QString defaultScrollFallback(const QString &action);
    QString scrollFallback(const QString &action) const;
    const QMap<QString, QString> &scrollFallbackOverrides() const;
    void setScrollFallbackOverrides(const QMap<QString, QString> &overrides);
public slots:
    bool invokeAction(const QString &actionName);
private:
    explicit ActionManager(QObject *parent = nullptr);
    ShortcutMap defaults, shortcuts;
    QMap<ViewMode, QHash<QString, QString>> shortcutLookup;
    ViewMode context = MODE_DOCUMENT;
    ViewMode overlayContext = MODE_CLIPEDIT;
    bool hasOverlayContext = false;
    // set by a wheel turned with the right button held (RMB+WheelUp/Down)
    bool rightButtonWheel = false;
    QMap<QString, QString> scrollFallbacks;

    static void initDefaults();
    static void initActions();
    static void initShortcuts();
    QString modifierKeys(QEvent *event);
    bool invokeActionForShortcut(ViewMode context, const QString &shortcut);
    void rebuildShortcutLookup();
    QString lookupActionForShortcut(ViewMode context, const QString &shortcut) const;
    void validateShortcuts();
    void readShortcuts();
    ActionType validateAction(const QString &actionName);

signals:
    void open();
    void save();
    void saveAs();
    void openSettings();
    void crop();
    void setWallpaper();
    void nextImage();
    void prevImage();
    void fitWindow();
    void fitWidth();
    void fitNormal();
    void fitWindowStretch();
    void flipH();
    void flipV();
    void toggleFitMode();
    void toggleFullscreen();
    void scrollUp();
    void scrollDown();
    void scrollLeft();
    void scrollRight();
    void scrollUpFine();
    void scrollDownFine();
    void scrollLeftFine();
    void scrollRightFine();
    void scrollOrPrevImage();
    void scrollOrNextImage();
    // drag actions: only bindings of drags (LMB_Drag ...) run them; invoked
    // any other way they do nothing
    void dragPan();
    void dragZoom();
    void dragFileOut();
    // copy/move to the Copy / Move overlay's numbered folders
    void sendToFolder1();
    void sendToFolder2();
    void sendToFolder3();
    void sendToFolder4();
    void sendToFolder5();
    void sendToFolder6();
    void sendToFolder7();
    void sendToFolder8();
    void sendToFolder9();
    void openSelected();
    void selectAll();
    void startSearch();
    void selectPrevious();
    void selectNext();
    void selectAbove();
    void selectBelow();
    void selectPageUp();
    void selectPageDown();
    void selectFirst();
    void selectLast();
    void zoomIn();
    void zoomOut();
    void zoomInCursor();
    void zoomOutCursor();
    void resize();
    void rotateLeft();
    void rotateRight();
    void exit();
    void removeFile();
    void copyFile();
    void moveFile();
    void closeFullScreenOrExit();
    void jumpToFirst();
    void jumpToLast();
    void folderView();
    void documentView();
    void runScript(const QString&);
    void pauseVideo();
    void seekVideoForward();
    void seekVideoBackward();
    void frameStep();
    void frameStepBack();
    void toggleFolderView();
    void moveToTrash();
    void reloadImage();
    void copyFileClipboard();
    void cutFile();
    void copyPathClipboard();
    void renameFile();
    void createDirectory();
    void contextMenu();
    void toggleTransparencyGrid();
    void sortByName();
    void sortByTime();
    void sortBySize();
    void toggleImageInfo();
    void stripMetadata();
    void toggleShuffle();
    void toggleScalingFilter();
    void showInDirectory();
    void toggleMute();
    // the clip of a video: the bracket button's edit mode and its keys
    void toggleClipEdit();
    void commitClipEdit();
    void cancelClipEdit();
    void clearClip();
    void setClipStart();
    void setClipEnd();
    void toggleVideoLoop();
    void volumeUp();
    void volumeDown();
    void toggleSlideshow();
    void discardEdits();
    void goUp();
    void nextDirectory();
    void prevDirectory();
    void lockZoom();
    void lockView();
    void print();
    void toggleFullscreenInfoBar();
    void pasteFile();
    void toggleFolderViewTopBar();
    void toggleStatusFooter();
    void togglePlacesPanel();
    void findDuplicates();
};

extern ActionManager *actionManager;
