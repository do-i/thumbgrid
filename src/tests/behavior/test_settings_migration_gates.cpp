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
    conf.sync();

    Settings::getInstance();

    QSettings after;
    return require(!after.contains("Document/defaultCropAction"), "Grouped defaultCropAction should be pruned on upgrade.") &&
           require(!after.contains("defaultCropAction"), "Flat defaultCropAction should be pruned on upgrade.") &&
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

bool runScenario(const QString &scenario) {
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
