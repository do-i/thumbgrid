#pragma once

#include <QObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMap>
#include <QHash>
#include <QDebug>
#include <QStringList>
#include "utils/actions.h"
#include "shortcutbuilder.h"
#include "components/scriptmanager/scriptmanager.h"
#include "settings.h"
#include "shortcutpresetstore.h"

enum ActionType {
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
    static QString contextToString(ViewMode context);
    static ViewMode contextFromString(const QString &name);

    bool processEvent(QInputEvent*);
    void addShortcut(ViewMode context, const QString &keys, const QString &action);
    void resetDefaults();
    void resetDefaults(ViewMode context);
    void resetDefaults(const QString& action);
    QString actionForShortcut(ViewMode context, const QString &keys);
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
public slots:
    bool invokeAction(const QString &actionName);
private:
    explicit ActionManager(QObject *parent = nullptr);
    ShortcutMap defaults, shortcuts;
    QMap<ViewMode, QHash<QString, QString>> shortcutLookup;
    ViewMode context = MODE_DOCUMENT;

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
