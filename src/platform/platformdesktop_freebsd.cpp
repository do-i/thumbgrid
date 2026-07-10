#include "platformdesktop.h"

#include "components/scriptmanager/scriptmanager.h"
#include "proxystyle.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

void PlatformDesktop::applyApplicationStyle(QApplication *app) {
    app->setStyle(new ProxyStyle);
}

void PlatformDesktop::applyHighDpiPolicy() {
}

bool PlatformDesktop::cacheDirectoryIsConfigurable() {
    return true;
}

QString PlatformDesktop::contextMenuBorderRadius() {
    return "3px";
}

QSettings *PlatformDesktop::createSettingsConfig() {
    QSettings::setDefaultFormat(QSettings::NativeFormat);
    return new QSettings();
}

QSettings *PlatformDesktop::createStateConfig() {
    return new QSettings(QCoreApplication::organizationName(), "savedState");
}

QSettings *PlatformDesktop::createThemeConfig(const QString &configDir) {
    return new QSettings(configDir + "/theme.ini", QSettings::IniFormat);
}

QString PlatformDesktop::defaultCacheDirectory() {
    QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if(cacheLocation.isEmpty())
        cacheLocation = QDir::homePath() + "/.cache";
    return cacheLocation + "/" + QApplication::applicationName();
}

QString PlatformDesktop::defaultMpvBinary() {
    return "/usr/local/bin/mpv";
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

QString PlatformDesktop::settingsConfigDirectory(const QSettings *settingsConfig) {
    return QFileInfo(settingsConfig->fileName()).absolutePath();
}

bool PlatformDesktop::shouldIgnoreWheelEvent(QWheelEvent *) {
    return false;
}

void PlatformDesktop::showInDirectory(const QString &selectedPath, const QString &fallbackDir) {
    if(selectedPath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fallbackDir));
        return;
    }

    QString fm = ScriptManager::runCommand("xdg-mime query default inode/directory");
    if(fm.contains("dolphin"))
        ScriptManager::runCommandDetached("dolphin --select " + selectedPath);
    else if(fm.contains("nautilus"))
        ScriptManager::runCommandDetached("nautilus --select " + selectedPath);
    else
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

bool PlatformDesktop::setWallpaper(const QString &path, QString *errorMessage) {
    const QString session = QString::fromLocal8Bit(qgetenv("DESKTOP_SESSION")).toLower();
    if(session.contains("plasma")) {
        ScriptManager::runCommand("plasma-apply-wallpaperimage \"" + path + "\"");
        return true;
    }
    if(session.contains("gnome")) {
        ScriptManager::runCommand("gsettings set org.gnome.desktop.background picture-uri \"" + path + "\"");
        return true;
    }

    if(errorMessage)
        *errorMessage = "Action is not supported in your desktop session (\"" + session + "\")";
    return false;
}

QString PlatformDesktop::thumbnailCacheDirectory(const QString &cacheDir) {
    return cacheDir + "/thumbnails";
}
