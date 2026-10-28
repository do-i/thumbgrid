#include "appversion.h"
#include "settings.h"
#include "themestore.h"
#include "components/actionmanager/actionmanager.h"
#include "components/scaler/scalerrequest.h"
#include "sourcecontainers/image.h"
#include "sourcecontainers/thumbnail.h"
#include "utils/script.h"
#include "utils/actions.h"
#include "utils/inputmap.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <memory>

namespace {

bool fail(const QString &message) {
    qWarning().noquote() << message;
    return false;
}

bool require(bool condition, const QString &message) {
    return condition || fail(message);
}

void writeVersion(QSettings &conf, const QVersionNumber &version) {
    conf.setValue("lastVerMajor", version.majorVersion());
    conf.setValue("lastVerMinor", version.minorVersion());
    conf.setValue("lastVerMicro", version.microVersion());
}

QString configDir() {
    return QFileInfo(QSettings().fileName()).absolutePath();
}

void seedExistingConfig(QSettings &conf, const QVersionNumber &version) {
    conf.setValue("firstRun", false);
    writeVersion(conf, version);
}

bool freshInstallSkipsVersionedMigrations() {
    Settings::getInstance();

    QSettings conf;
    return require(!conf.contains("theme"),
                   "Fresh install should not create the theme pointer from the versioned grouping migration.");
}

bool upgradeRunsVersionedGroupingMigration() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 7, 5));
    conf.setValue("videoPlayback", true);
    conf.sync();

    Settings::getInstance();

    QSettings after;
    return require(!after.contains("videoPlayback"), "Flat videoPlayback should be removed on upgrade.") &&
           require(after.contains("Document/videoPlayback"), "Document/videoPlayback should be written on upgrade.");
}

bool currentVersionSkipsVersionedGroupingMigration() {
    QSettings conf;
    seedExistingConfig(conf, appVersion);
    conf.setValue("videoPlayback", true);
    conf.sync();

    Settings::getInstance();

    QSettings after;
    return require(after.contains("videoPlayback"), "Current-version flat videoPlayback should be trusted and left alone.") &&
           require(!after.contains("Document/videoPlayback"), "Current-version config should not run the grouping migration.");
}

bool downgradeSkipsVersionedGroupingMigration() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(appVersion.majorVersion() + 1, 0, 0));
    conf.setValue("videoPlayback", true);
    conf.sync();

    Settings::getInstance();

    QSettings after;
    return require(after.contains("videoPlayback"), "Downgrade config should be left untouched.") &&
           require(!after.contains("Document/videoPlayback"), "Downgrade must not run versioned grouping migration.");
}

bool upgradePrunesRetiredKeys() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 7, 5));
    // Both spellings a config could carry: grouped, and flat from before the
    // grouping migration.
    conf.setValue("Document/defaultCropAction", 1);
    conf.setValue("defaultCropAction", 1);
    conf.setValue("Document/panelCenterSelection", true);
    conf.setValue("General/showChangelogs", false);
    conf.setValue("showChangelogs", false);
    conf.sync();

    Settings::getInstance();

    QSettings after;
    return require(!after.contains("Document/defaultCropAction"), "Grouped defaultCropAction should be pruned on upgrade.") &&
           require(!after.contains("defaultCropAction"), "Flat defaultCropAction should be pruned on upgrade.") &&
           require(!after.contains("General/showChangelogs") && !after.contains("showChangelogs"),
                   "The retired showChangelogs switch should be pruned on upgrade.") &&
           require(after.contains("Document/panelCenterSelection"), "Pruning must not touch live keys.");
}

bool themeRecoveryPreservesSelectionPointer() {
    QSettings conf;
    seedExistingConfig(conf, appVersion);
    conf.sync();

    QSettings legacy(configDir() + "/theme.conf", QSettings::IniFormat);
    legacy.beginGroup("Colors");
    legacy.setValue("tid", COLORS_LIGHT);
    legacy.setValue("accent", "#123456");
    legacy.endGroup();
    legacy.sync();

    Settings::getInstance();

    QSettings after;
    QSettings theme(configDir() + "/theme.ini", QSettings::IniFormat);
    return require(QFile::exists(configDir() + "/theme.ini"), "Theme recovery should create theme.ini.") &&
           require(after.value("theme").toString() == QLatin1String("light"),
                   "Theme recovery should preserve the selected preset pointer.") &&
           require(theme.value("Colors/accent").toString() == QLatin1String("#123456"),
                   "Theme recovery should import legacy colors.") &&
           require(!theme.contains("Theme/tid"), "Theme recovery should not leave the selection pointer in theme.ini.");
}

bool shortcutRecoveryStaysLazyAndImportsOnRead() {
    QSettings conf;
    seedExistingConfig(conf, appVersion);
    conf.beginGroup("Controls");
    conf.setValue("shortcuts", QStringList{"nextImage=Right", "zoomIn=eq"});
    conf.endGroup();
    conf.sync();

    Settings::getInstance();
    const QString shortcutsPath = configDir() + "/shortcuts.json";
    if(!require(!QFile::exists(shortcutsPath), "Shortcut recovery should stay lazy until readShortcuts()."))
        return false;

    QMap<ViewMode, QMap<QString, QString>> shortcuts;
    settings->readShortcuts(shortcuts);

    return require(QFile::exists(shortcutsPath), "readShortcuts() should create shortcuts.json from legacy overrides.") &&
           require(shortcuts[MODE_GLOBAL].value("Right") == QLatin1String("nextImage"),
                   "Legacy context-less Right shortcut should migrate to global.") &&
           require(shortcuts[MODE_GLOBAL].value("=") == QLatin1String("zoomIn"),
                   "Legacy eq shortcut should decode to '='.");
}

bool stateRecoveryImportsLegacySavedState() {
    QSettings conf;
    seedExistingConfig(conf, appVersion);
    conf.setValue("State/volume", 55);
    conf.sync();

    QString legacyPath;
    {
        QSettings legacy(QCoreApplication::organizationName(), "savedState");
        legacy.setValue("volume", 42);
        legacy.setValue("placesPanelWidth", 300);
        legacy.sync();
        legacyPath = legacy.fileName();
    }

    Settings::getInstance();

    QSettings after;
    return require(!QFile::exists(legacyPath), "State recovery should delete the legacy savedState file.") &&
           require(after.value("State/placesPanelWidth").toInt() == 300,
                   "Legacy placesPanelWidth should be imported into [State].") &&
           require(after.value("State/volume").toInt() == 55,
                   "Existing [State] keys should win over the legacy savedState file.") &&
           require(settings->placesPanelWidth() == 300,
                   "The accessor should read the imported [State] value.") &&
           require(settings->volume() == 55,
                   "The accessor should read the pre-existing [State] value.");
}

bool duplicateFinderBlobExplodesIntoFlatKeys() {
    QSettings conf;
    seedExistingConfig(conf, appVersion);
    QVariantMap blob;
    blob["similarity"] = 92;
    blob["recursive"] = false;
    blob["targets"] = QStringList{"/photos/a", "/photos/b"};
    conf.setValue("State/duplicateFinder", blob);
    conf.setValue("State/duplicateFinderSimilarity", 78);
    conf.sync();

    Settings::getInstance();

    QSettings after;
    return require(!after.contains("State/duplicateFinder"),
                   "The opaque duplicateFinder blob should be removed.") &&
           require(after.value("State/duplicateFinderSimilarity").toInt() == 78,
                   "Existing flat keys should win over the legacy blob.") &&
           require(after.value("State/duplicateFinderRecursive").toBool() == false,
                   "Blob values should be exploded into flat keys.") &&
           require(settings->duplicateFinderTargets() == QStringList({"/photos/a", "/photos/b"}),
                   "The targets list should survive the explode readably.");
}

bool shortcutPresetPointerIsRecovered() {
    QSettings conf;
    seedExistingConfig(conf, appVersion);
    conf.sync();

    Settings::getInstance();

    QSettings after;
    return require(after.value("Shortcuts/preset").toString() == QLatin1String("qimgv"),
                   "Missing shortcut preset pointer should be backfilled as qimgv.");
}

// Actions introduced after an old install's lastVersion (toggleStatusFooter
// 1.0.3, cutFile 1.0.4, togglePlacesPanel 1.0.6) must still reach an upgrading
// user's shortcuts.json even with the old per-action mergeMissing() calls
// gone: adjustFromVersion()'s generic backfill loop, seeded from the qimgv
// preset, is now the sole mechanism.
bool shortcutNewActionsBackfillWithoutMergeMissing() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(1, 0, 2));
    conf.sync();

    QJsonObject global;
    global.insert("copyFile", QJsonArray{QStringLiteral("C")});
    global.insert("open", QJsonArray{QStringLiteral("$Ctrl+O")});
    QJsonObject root;
    root.insert("global", global);
    QFile shortcutsFile(configDir() + "/shortcuts.json");
    if(!require(shortcutsFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
                "Failed to write the pre-existing shortcuts.json fixture."))
        return false;
    shortcutsFile.write(QJsonDocument(root).toJson());
    shortcutsFile.close();

    Settings::getInstance();
    actionManager = ActionManager::getInstance();

    // Simulate the Core::onUpdate() upgrade path for a user last seen on 1.0.2.
    actionManager->adjustFromVersion(QVersionNumber(1, 0, 2));

    return require(actionManager->actionForShortcut(MODE_GLOBAL, "Ctrl+B") == QLatin1String("toggleStatusFooter"),
                   "toggleStatusFooter should be backfilled from the qimgv preset without mergeMissing.") &&
           require(actionManager->actionForShortcut(MODE_GLOBAL, "Ctrl+X") == QLatin1String("cutFile"),
                   "cutFile should be backfilled from the qimgv preset without mergeMissing.") &&
           require(actionManager->actionForShortcut(MODE_GLOBAL, "Ctrl+E") == QLatin1String("togglePlacesPanel"),
                   "togglePlacesPanel should be backfilled from the qimgv preset without mergeMissing.");
}

// Writes a shortcuts.json fixture shaped like the leftie preset: I and O are
// spent on zoom, and File info is not bound at all.
bool seedLeftieShortcuts() {
    QJsonObject document;
    document.insert("zoomIn", QJsonArray{QStringLiteral("$Ctrl++"), QStringLiteral("$Ctrl+="), QStringLiteral("I")});
    document.insert("zoomOut", QJsonArray{QStringLiteral("$Ctrl+-"), QStringLiteral("O")});
    QJsonObject grid;
    grid.insert("folderView", QJsonArray{QStringLiteral("U")});
    QJsonObject root;
    root.insert("document", document);
    root.insert("grid", grid);
    QFile shortcutsFile(configDir() + "/shortcuts.json");
    if(!require(shortcutsFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
                "Failed to write the leftie shortcuts.json fixture."))
        return false;
    shortcutsFile.write(QJsonDocument(root).toJson());
    shortcutsFile.close();
    return true;
}

// File info reaches an upgrading user from whichever key their *preset* assigns,
// never a key named in the migration. On leftie that has to be Alt+I, because I
// is already that preset's zoomIn - the previous migration hardcoded I and so
// silently did nothing for these users.
bool fileInfoBackfillsFromThePresetKey() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 7, 14));
    conf.setValue("Shortcuts/preset", "leftie");
    conf.sync();

    if(!seedLeftieShortcuts())
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 7, 14));

    return require(actionManager->actionForShortcut(MODE_DOCUMENT, "Alt+I") == QLatin1String("toggleImageInfo"),
                   "leftie's document context should pick up File info on Alt+I.") &&
           require(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Alt+I") == QLatin1String("toggleImageInfo"),
                   "leftie's grid context should pick up File info on Alt+I.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "I") == QLatin1String("zoomIn"),
                   "the backfill must not take I away from zoomIn.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "O") == QLatin1String("zoomOut"),
                   "the backfill must leave the rest of the preset alone.");
}

// A user who already chose a key for File info keeps it: the backfill must not
// hand the action a second binding behind their back.
bool fileInfoBackfillSkipsAnExistingBinding() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 7, 14));
    conf.setValue("Shortcuts/preset", "leftie");
    conf.sync();

    QJsonObject document;
    document.insert("toggleImageInfo", QJsonArray{QStringLiteral("F4")});
    QJsonObject root;
    root.insert("document", document);
    QFile shortcutsFile(configDir() + "/shortcuts.json");
    if(!require(shortcutsFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
                "Failed to write the customised shortcuts.json fixture."))
        return false;
    shortcutsFile.write(QJsonDocument(root).toJson());
    shortcutsFile.close();

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 7, 14));

    return require(actionManager->actionForShortcut(MODE_DOCUMENT, "F4") == QLatin1String("toggleImageInfo"),
                   "a user's own File info key must survive the upgrade.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "Alt+I").isEmpty(),
                   "File info already had a document binding, so Alt+I must not be added there too.");
}

// Writes a shortcuts.json fixture with the given document and grid sections.
bool seedDocumentAndGridShortcuts(const QJsonObject &document, const QJsonObject &grid) {
    QJsonObject root;
    root.insert("document", document);
    root.insert("grid", grid);
    QFile shortcutsFile(configDir() + "/shortcuts.json");
    if(!require(shortcutsFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
                "Failed to write the shortcuts.json fixture."))
        return false;
    shortcutsFile.write(QJsonDocument(root).toJson());
    shortcutsFile.close();
    return true;
}

// Resize gained a grid flow, so an upgrading user gets their picture-view resize
// key in the grid as well - the same key the qimgv preset now binds there.
bool resizeReachesTheGridOnUpgrade() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 9, 2));
    conf.sync();

    QJsonObject document;
    document.insert("resize", QJsonArray{QStringLiteral("R")});
    QJsonObject grid;
    grid.insert("goUp", QJsonArray{QStringLiteral("Backspace")});
    if(!seedDocumentAndGridShortcuts(document, grid))
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 9, 2));

    return require(actionManager->actionForShortcut(MODE_FOLDERVIEW, "R") == QLatin1String("resize"),
                   "R should open Resize in the grid after the upgrade.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "R") == QLatin1String("resize"),
                   "R must keep opening Resize in the picture view.") &&
           require(actionManager->allDefaultShortcuts().value(MODE_GLOBAL).value("R") == QLatin1String("resize"),
                   "The qimgv preset binds R in both views, so its defaults should carry it globally.");
}

// The grid gets the user's own picture-view key, not the preset's.
bool resizeGridBackfillFollowsTheUsersKey() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 9, 2));
    conf.sync();

    QJsonObject document;
    document.insert("resize", QJsonArray{QStringLiteral("F9")});
    if(!seedDocumentAndGridShortcuts(document, QJsonObject()))
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 9, 2));

    return require(actionManager->actionForShortcut(MODE_FOLDERVIEW, "F9") == QLatin1String("resize"),
                   "A rebound resize key should be mirrored into the grid.") &&
           require(actionManager->actionForShortcut(MODE_FOLDERVIEW, "R").isEmpty(),
                   "The preset's R must not be added when the user moved resize elsewhere.");
}

// A key that already means something in the grid is never taken over.
bool resizeGridBackfillSkipsATakenKey() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 9, 2));
    conf.sync();

    QJsonObject document;
    document.insert("resize", QJsonArray{QStringLiteral("R")});
    QJsonObject grid;
    grid.insert("reloadImage", QJsonArray{QStringLiteral("R")});
    if(!seedDocumentAndGridShortcuts(document, grid))
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 9, 2));

    return require(actionManager->actionForShortcut(MODE_FOLDERVIEW, "R") == QLatin1String("reloadImage"),
                   "The grid's own R binding must survive the upgrade.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "R") == QLatin1String("resize"),
                   "The picture view keeps R for Resize.");
}

// Arrows became scroll actions: an upgrading user still on the old qimgv
// defaults gets them, keeping prev/next and seeking as their fallbacks.
bool arrowKeysBecomeScrollActionsOnUpgrade() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 2));
    conf.sync();

    QJsonObject document;
    document.insert("prevImage", QJsonArray{QStringLiteral("Left"), QStringLiteral("PgUp"), QStringLiteral("WheelUp")});
    document.insert("nextImage", QJsonArray{QStringLiteral("Right"), QStringLiteral("WheelDown"), QStringLiteral("XButton2")});
    document.insert("seekVideoBackward", QJsonArray{QStringLiteral("Ctrl+Left")});
    document.insert("zoomIn", QJsonArray{QStringLiteral("Ctrl+Up"), QStringLiteral("+")});
    if(!seedDocumentAndGridShortcuts(document, QJsonObject()))
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 2));

    const auto bound = [](const char *key, const char *action) {
        return actionManager->actionForShortcut(MODE_DOCUMENT, key) == QLatin1String(action);
    };
    return require(bound("Left", "scrollLeft") && bound("Right", "scrollRight"),
                   "Left/Right should move from prev/next to the scroll actions.") &&
           require(bound("Up", "scrollUp") && bound("Down", "scrollDown"),
                   "Unbound Up/Down should get the scroll actions.") &&
           require(bound("Ctrl+Left", "scrollLeftFine") && bound("Ctrl+Right", "scrollRightFine") &&
                   bound("Ctrl+Up", "scrollUpFine") && bound("Ctrl+Down", "scrollDownFine"),
                   "Ctrl+arrows should move to the fine scroll actions.") &&
           require(bound("WheelUp", "scrollOrPrevImage") && bound("WheelDown", "scrollOrNextImage"),
                   "The wheel should move from prev/next to the scroll-or-change-file actions.") &&
           require(bound("RMB+WheelUp", "zoomInCursor") && bound("RMB+WheelDown", "zoomOutCursor"),
                   "The right button + wheel zoom should become bindings.") &&
           require(bound("LMB_LeftEdge", "prevImage") && bound("LMB_RightEdge", "nextImage"),
                   "The clickable edges should become bindings.") &&
           require(bound("LMB_Drag", "dragPan") && bound("RMB_Drag", "dragZoom"),
                   "The picture's left/right drags should become bindings.") &&
           require(actionManager->actionForShortcut(MODE_FOLDERVIEW, "LMB_TileDoubleClick") == QLatin1String("openSelected") &&
                   actionManager->actionForShortcut(MODE_FOLDERVIEW, "LMB_Drag") == QLatin1String("dragFileOut"),
                   "A grid tile's double-click and drag should become bindings.") &&
           require(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Ctrl+WheelUp") == QLatin1String("zoomIn") &&
                   actionManager->actionForShortcut(MODE_FOLDERVIEW, "Ctrl+WheelDown") == QLatin1String("zoomOut"),
                   "The grid's Ctrl+wheel thumbnail resize should become bindings.") &&
           require(actionManager->actionForShortcut(MODE_COPYMOVE, "1") == QLatin1String("sendToFolder1") &&
                   actionManager->actionForShortcut(MODE_COPYMOVE, "9") == QLatin1String("sendToFolder9"),
                   "The Copy / Move overlay's digits should become bindings.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "[") == QLatin1String("setClipStart") &&
                   actionManager->actionForShortcut(MODE_DOCUMENT, "\\") == QLatin1String("toggleVideoLoop") &&
                   actionManager->actionForShortcut(MODE_CLIPEDIT, "Esc") == QLatin1String("cancelClipEdit"),
                   "The video clip keys should be bound on upgrade.") &&
           require(bound("PgUp", "prevImage") && bound("XButton2", "nextImage") && bound("+", "zoomIn"),
                   "The user's other keys for those actions must survive.");
}

// The upgrade follows the selected preset: leftie binds the arrows to the same
// scroll actions, so a leftie config on its old defaults gets them too.
bool arrowKeysFollowTheSelectedPreset() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 2));
    conf.sync();

    QJsonObject document;
    document.insert("prevImage", QJsonArray{QStringLiteral("Left"), QStringLiteral("PgUp")});
    document.insert("nextImage", QJsonArray{QStringLiteral("PgDown"), QStringLiteral("Right")});
    document.insert("seekVideoBackward", QJsonArray{QStringLiteral("Ctrl+Left")});
    document.insert("seekVideoForward", QJsonArray{QStringLiteral("Ctrl+Right")});
    if(!seedDocumentAndGridShortcuts(document, QJsonObject()))
        return false;

    Settings::getInstance()->setSelectedPreset(QStringLiteral("leftie"));
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 2));

    const auto bound = [](ViewMode ctx, const char *key, const char *action) {
        return actionManager->actionForShortcut(ctx, key) == QLatin1String(action);
    };
    return require(bound(MODE_DOCUMENT, "Left", "scrollLeft") && bound(MODE_DOCUMENT, "Right", "scrollRight") &&
                   bound(MODE_DOCUMENT, "Up", "scrollUp") && bound(MODE_DOCUMENT, "Ctrl+Left", "scrollLeftFine"),
                   "A leftie config should get the scroll actions on the arrows.") &&
           require(bound(MODE_DOCUMENT, "PgUp", "prevImage") && bound(MODE_DOCUMENT, "PgDown", "nextImage"),
                   "PgUp/PgDown keep changing file.") &&
           require(bound(MODE_FOLDERVIEW, "Enter", "openSelected") && bound(MODE_FOLDERVIEW, "Backspace", "goUp"),
                   "A leftie config should get the grid keys too.");
}

// Space used to pause a video in code; it now runs its binding. A config with
// Space unbound gets the preset's (leftie: pauseVideo, qimgv: toggleFitMode),
// and a Space the user bound is left alone.
bool spaceGetsThePresetBinding() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 2));
    conf.sync();

    QJsonObject document;
    document.insert("crop", QJsonArray{QStringLiteral("N")});
    if(!seedDocumentAndGridShortcuts(document, QJsonObject()))
        return false;

    Settings::getInstance()->setSelectedPreset(QStringLiteral("leftie"));
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 2));
    return require(actionManager->actionForShortcut(MODE_DOCUMENT, "Space") == QLatin1String("pauseVideo"),
                   "A leftie config with Space unbound should get Space = pauseVideo.");
}

bool spaceMigrationKeepsCustomBinding() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 2));
    conf.sync();

    QJsonObject document;
    document.insert("nextImage", QJsonArray{QStringLiteral("Space")});
    if(!seedDocumentAndGridShortcuts(document, QJsonObject()))
        return false;

    Settings::getInstance()->setSelectedPreset(QStringLiteral("leftie"));
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 2));
    return require(actionManager->actionForShortcut(MODE_DOCUMENT, "Space") == QLatin1String("nextImage"),
                   "A customised Space must survive the upgrade.");
}

// gwenview, irfanview and xnviewmp used to bind Space to nextImage; a config
// still on that default moves Space to pauseVideo, keeping its other keys.
bool spaceMovesOffTheOldNextImageDefault() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 2));
    conf.sync();

    QJsonObject document;
    document.insert("nextImage", QJsonArray{QStringLiteral("Space"), QStringLiteral("PgDown")});
    if(!seedDocumentAndGridShortcuts(document, QJsonObject()))
        return false;

    Settings::getInstance()->setSelectedPreset(QStringLiteral("gwenview"));
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 2));
    return require(actionManager->actionForShortcut(MODE_DOCUMENT, "Space") == QLatin1String("pauseVideo"),
                   "A gwenview config on its old Space default should get Space = pauseVideo.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "PgDown") == QLatin1String("nextImage"),
                   "PgDown keeps changing file.");
}

// A key the user deliberately rebound is never taken over.
bool arrowKeyMigrationKeepsCustomBindings() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 2));
    conf.sync();

    QJsonObject document;
    document.insert("crop", QJsonArray{QStringLiteral("Left")});
    document.insert("rotateRight", QJsonArray{QStringLiteral("Ctrl+Right")});
    document.insert("zoomOut", QJsonArray{QStringLiteral("WheelDown")});
    document.insert("rotateLeft", QJsonArray{QStringLiteral("RMB+WheelDown")});
    if(!seedDocumentAndGridShortcuts(document, QJsonObject()))
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 2));

    return require(actionManager->actionForShortcut(MODE_DOCUMENT, "Left") == QLatin1String("crop"),
                   "A customised Left must survive the upgrade.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "Ctrl+Right") == QLatin1String("rotateRight"),
                   "A customised Ctrl+Right must survive the upgrade.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "WheelDown") == QLatin1String("zoomOut"),
                   "A customised WheelDown must survive the upgrade.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "RMB+WheelDown") == QLatin1String("rotateLeft"),
                   "A customised RMB+WheelDown must survive the upgrade.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "Right") == QLatin1String("scrollRight"),
                   "A free Right should still get its scroll action.") &&
           require(actionManager->actionForShortcut(MODE_DOCUMENT, "WheelUp") == QLatin1String("scrollOrPrevImage"),
                   "A free WheelUp should still get its scroll action.");
}

// The grid's hard-coded Enter / Backspace / Ctrl+A / "/" became actions: an
// upgrading config gets them, replacing Enter's never-reached documentView.
bool gridKeysBecomeActionsOnUpgrade() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 2));
    conf.sync();

    QJsonObject grid;
    grid.insert("documentView", QJsonArray{QStringLiteral("Enter"), QStringLiteral("U")});
    if(!seedDocumentAndGridShortcuts(QJsonObject(), grid))
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 2));

    const auto bound = [](const char *key, const char *action) {
        return actionManager->actionForShortcut(MODE_FOLDERVIEW, key) == QLatin1String(action);
    };
    return require(bound("Enter", "openSelected"), "Enter should open the selected item.") &&
           require(bound("Backspace", "goUp"), "Backspace should go up a folder.") &&
           require(bound("Ctrl+A", "selectAll"), "Ctrl+A should select all.") &&
           require(bound("/", "startSearch"), "/ should start the search.") &&
           require(bound("U", "documentView"), "The user's other documentView key must survive.");
}

// A grid key the user bound to something else is left alone.
bool gridKeyMigrationKeepsCustomBindings() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 2));
    conf.sync();

    QJsonObject grid;
    grid.insert("createDirectory", QJsonArray{QStringLiteral("Backspace")});
    if(!seedDocumentAndGridShortcuts(QJsonObject(), grid))
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 2));

    return require(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Backspace") == QLatin1String("createDirectory"),
                   "A customised grid Backspace must survive the upgrade.") &&
           require(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Enter") == QLatin1String("openSelected"),
                   "A free Enter should still get openSelected.");
}

// The grid's arrows, PgUp/PgDown and Home/End moved the selection in code; they
// are now the selection actions. An upgrading config gets all eight in the
// grid, replacing any grid binding on those keys (it never ran).
bool gridNavigationKeysBecomeActions() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 2));
    conf.sync();

    QJsonObject grid;
    grid.insert("createDirectory", QJsonArray{QStringLiteral("Right"), QStringLiteral("F7")});
    QJsonObject document;
    document.insert("scrollLeft", QJsonArray{QStringLiteral("Left")});
    if(!seedDocumentAndGridShortcuts(document, grid))
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 2));

    const auto bound = [](ViewMode ctx, const char *key, const char *action) {
        return actionManager->actionForShortcut(ctx, key) == QLatin1String(action);
    };
    return require(bound(MODE_FOLDERVIEW, "Left", "selectPrevious") && bound(MODE_FOLDERVIEW, "Right", "selectNext") &&
                   bound(MODE_FOLDERVIEW, "Up", "selectAbove") && bound(MODE_FOLDERVIEW, "Down", "selectBelow") &&
                   bound(MODE_FOLDERVIEW, "PgUp", "selectPageUp") && bound(MODE_FOLDERVIEW, "PgDown", "selectPageDown") &&
                   bound(MODE_FOLDERVIEW, "Home", "selectFirst") && bound(MODE_FOLDERVIEW, "End", "selectLast"),
                   "The grid should get all eight selection actions.") &&
           require(bound(MODE_FOLDERVIEW, "F7", "createDirectory"),
                   "The other key of a replaced grid binding must survive.") &&
           require(bound(MODE_DOCUMENT, "Left", "scrollLeft"),
                   "The picture view's arrows are untouched.");
}

// Writes a shortcuts.json fixture binding full screen to F and a double-click.
bool seedDoubleClickFullscreen() {
    QJsonObject global;
    global.insert("toggleFullscreen", QJsonArray{QStringLiteral("F"), QStringLiteral("LMB_DoubleClick")});
    QJsonObject root;
    root.insert("global", global);
    QFile shortcutsFile(configDir() + "/shortcuts.json");
    if(!require(shortcutsFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
                "Failed to write the shortcuts.json fixture."))
        return false;
    shortcutsFile.write(QJsonDocument(root).toJson());
    shortcutsFile.close();
    return true;
}

// The qimgv preset no longer toggles full screen on a double-click, and a
// config still on that default loses it on upgrade.
bool doubleClickFullscreenIsDroppedOnUpgrade() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 3));
    conf.sync();
    if(!seedDoubleClickFullscreen())
        return false;

    Settings::getInstance();
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 3));
    return require(actionManager->actionForShortcut(MODE_GLOBAL, "LMB_DoubleClick").isEmpty() &&
                   actionManager->actionForShortcut(MODE_DOCUMENT, "LMB_DoubleClick").isEmpty(),
                   "A double-click should no longer toggle full screen.") &&
           require(actionManager->actionForShortcut(MODE_GLOBAL, "F") == QLatin1String("toggleFullscreen"),
                   "F keeps toggling full screen.");
}

// gwenview still binds the double-click to full screen, so its config keeps it.
bool doubleClickFullscreenStaysWhenThePresetKeepsIt() {
    QSettings conf;
    seedExistingConfig(conf, QVersionNumber(2026, 10, 3));
    conf.sync();
    if(!seedDoubleClickFullscreen())
        return false;

    Settings::getInstance()->setSelectedPreset(QStringLiteral("gwenview"));
    actionManager = ActionManager::getInstance();
    actionManager->adjustFromVersion(QVersionNumber(2026, 10, 3));
    return require(actionManager->actionForShortcut(MODE_GLOBAL, "LMB_DoubleClick") == QLatin1String("toggleFullscreen"),
                   "A gwenview config keeps the double-click full screen.");
}

bool runScenario(const QString &scenario) {
    if(scenario == QLatin1String("double-click-fullscreen-dropped"))
        return doubleClickFullscreenIsDroppedOnUpgrade();
    if(scenario == QLatin1String("double-click-fullscreen-kept-by-preset"))
        return doubleClickFullscreenStaysWhenThePresetKeepsIt();
    if(scenario == QLatin1String("fresh"))
        return freshInstallSkipsVersionedMigrations();
    if(scenario == QLatin1String("upgrade"))
        return upgradeRunsVersionedGroupingMigration();
    if(scenario == QLatin1String("current"))
        return currentVersionSkipsVersionedGroupingMigration();
    if(scenario == QLatin1String("downgrade"))
        return downgradeSkipsVersionedGroupingMigration();
    if(scenario == QLatin1String("retired-key-prune"))
        return upgradePrunesRetiredKeys();
    if(scenario == QLatin1String("theme-recovery"))
        return themeRecoveryPreservesSelectionPointer();
    if(scenario == QLatin1String("shortcut-recovery"))
        return shortcutRecoveryStaysLazyAndImportsOnRead();
    if(scenario == QLatin1String("state-recovery"))
        return stateRecoveryImportsLegacySavedState();
    if(scenario == QLatin1String("duplicate-finder-state-recovery"))
        return duplicateFinderBlobExplodesIntoFlatKeys();
    if(scenario == QLatin1String("shortcut-preset-recovery"))
        return shortcutPresetPointerIsRecovered();
    if(scenario == QLatin1String("shortcut-new-action-backfill"))
        return shortcutNewActionsBackfillWithoutMergeMissing();
    if(scenario == QLatin1String("file-info-preset-backfill"))
        return fileInfoBackfillsFromThePresetKey();
    if(scenario == QLatin1String("file-info-backfill-skips-existing"))
        return fileInfoBackfillSkipsAnExistingBinding();
    if(scenario == QLatin1String("resize-grid-backfill"))
        return resizeReachesTheGridOnUpgrade();
    if(scenario == QLatin1String("resize-grid-backfill-follows-user-key"))
        return resizeGridBackfillFollowsTheUsersKey();
    if(scenario == QLatin1String("resize-grid-backfill-skips-taken-key"))
        return resizeGridBackfillSkipsATakenKey();
    if(scenario == QLatin1String("arrow-keys-become-scroll-actions"))
        return arrowKeysBecomeScrollActionsOnUpgrade();
    if(scenario == QLatin1String("arrow-keys-follow-the-selected-preset"))
        return arrowKeysFollowTheSelectedPreset();
    if(scenario == QLatin1String("space-gets-the-preset-binding"))
        return spaceGetsThePresetBinding();
    if(scenario == QLatin1String("space-moves-off-the-old-next-image-default"))
        return spaceMovesOffTheOldNextImageDefault();
    if(scenario == QLatin1String("space-migration-keeps-custom-binding"))
        return spaceMigrationKeepsCustomBinding();
    if(scenario == QLatin1String("arrow-key-migration-keeps-custom-bindings"))
        return arrowKeyMigrationKeepsCustomBindings();
    if(scenario == QLatin1String("grid-navigation-keys-become-actions"))
        return gridNavigationKeysBecomeActions();
    if(scenario == QLatin1String("grid-keys-become-actions"))
        return gridKeysBecomeActionsOnUpgrade();
    if(scenario == QLatin1String("grid-key-migration-keeps-custom-bindings"))
        return gridKeyMigrationKeepsCustomBindings();
    return fail("Unknown scenario: " + scenario);
}

} // namespace

int main(int argc, char **argv) {
    QTemporaryDir testHome;
    QTemporaryDir configHome;
    QTemporaryDir cacheHome;
    if(testHome.isValid()) {
        qputenv("HOME", testHome.path().toUtf8());
        qputenv("USERPROFILE", testHome.path().toUtf8());
    }
    if(configHome.isValid())
        qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());
    if(cacheHome.isValid())
        qputenv("XDG_CACHE_HOME", cacheHome.path().toUtf8());
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("thumbgrid-tests");
    QCoreApplication::setOrganizationDomain("github.com/do-i/thumbgrid");
    QCoreApplication::setApplicationName("thumbgrid-tests");
    QCoreApplication::setApplicationVersion(appVersion.toString());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::NativeFormat);
    qRegisterMetaType<ScalerRequest>("ScalerRequest");
    qRegisterMetaType<Script>("Script");
    qRegisterMetaType<std::shared_ptr<Image>>("std::shared_ptr<Image>");
    qRegisterMetaType<std::shared_ptr<Thumbnail>>("std::shared_ptr<Thumbnail>");
    inputMap = InputMap::getInstance();
    appActions = Actions::getInstance();

    if(argc != 2)
        return fail("Expected one migration scenario argument.") ? 0 : 1;

    return runScenario(QString::fromLocal8Bit(argv[1])) ? 0 : 1;
}
