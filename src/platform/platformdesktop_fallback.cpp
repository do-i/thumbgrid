#include "platformdesktop.h"

#include "proxystyle.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QSettings>
#include <QUrl>

void PlatformDesktop::applyApplicationStyle(QApplication *app) {
    app->setStyle(new ProxyStyle);
}

void PlatformDesktop::applyHighDpiPolicy() {
}

bool PlatformDesktop::cacheDirectoryIsConfigurable() {
    return false;
}

QString PlatformDesktop::contextMenuBorderRadius() {
    return "3px";
}

QSettings *PlatformDesktop::createSettingsConfig() {
    const QString configDir = settingsConfigDirectory(nullptr);
    return new QSettings(configDir + "/" + QCoreApplication::applicationName() + ".ini", QSettings::IniFormat);
}

QSettings *PlatformDesktop::createLegacyStateConfig() {
    return new QSettings(settingsConfigDirectory(nullptr) + "/savedState.ini", QSettings::IniFormat);
}

QSettings *PlatformDesktop::createThemeConfig(const QString &configDir) {
    return new QSettings(configDir + "/theme.ini", QSettings::IniFormat);
}

QString PlatformDesktop::defaultCacheDirectory() {
    return QCoreApplication::applicationDirPath() + "/cache";
}

QString PlatformDesktop::defaultMpvBinary() {
    return QString();
}

QString PlatformDesktop::folderViewInitialRootPath() {
    return QDir::homePath();
}

QString PlatformDesktop::folderViewRootPathFor(const QString &) {
    return QString();
}

bool PlatformDesktop::isWaylandPlatform() {
    return qApp->platformName() == "wayland";
}

bool PlatformDesktop::needsWaylandCursorWorkaround() {
    return false;
}

void PlatformDesktop::prepareApplicationEnvironment() {
}

QString PlatformDesktop::settingsConfigDirectory(const QSettings *) {
    const QString configDir = QCoreApplication::applicationDirPath() + "/conf";
    QDir().mkpath(configDir);
    return configDir;
}

bool PlatformDesktop::shouldIgnoreWheelEvent(QWheelEvent *) {
    return false;
}

void PlatformDesktop::showInDirectory(const QString &selectedPath, const QString &fallbackDir) {
    Q_UNUSED(selectedPath)
    QDesktopServices::openUrl(QUrl::fromLocalFile(fallbackDir));
}

QString PlatformDesktop::shortcutsJsonPath(const QString &configDir) {
    return configDir + "/shortcuts.json";
}

int PlatformDesktop::slidePanelUpdateInterval() {
    return 16;
}

bool PlatformDesktop::supportsIcoSaveFormat() {
    return false;
}

bool PlatformDesktop::setWallpaper(const QString &, QString *errorMessage) {
    if(errorMessage)
        *errorMessage = "Action is not supported on this platform";
    return false;
}

QString PlatformDesktop::thumbnailCacheDirectory(const QString &) {
    return QCoreApplication::applicationDirPath() + "/thumbnails";
}
