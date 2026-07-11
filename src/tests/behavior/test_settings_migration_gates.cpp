#include "appversion.h"
#include "settings.h"
#include "themestore.h"
#include "components/scaler/scalerrequest.h"
#include "sourcecontainers/image.h"
#include "sourcecontainers/thumbnail.h"
#include "utils/script.h"
#include "utils/actions.h"
#include "utils/inputmap.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
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

bool shortcutPresetPointerIsRecovered() {
    QSettings conf;
    seedExistingConfig(conf, appVersion);
    conf.sync();

    Settings::getInstance();

    QSettings after;
    return require(after.value("Shortcuts/preset").toString() == QLatin1String("qimgv"),
                   "Missing shortcut preset pointer should be backfilled as qimgv.");
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
    if(scenario == QLatin1String("theme-recovery"))
        return themeRecoveryPreservesSelectionPointer();
    if(scenario == QLatin1String("shortcut-recovery"))
        return shortcutRecoveryStaysLazyAndImportsOnRead();
    if(scenario == QLatin1String("shortcut-preset-recovery"))
        return shortcutPresetPointerIsRecovered();
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
