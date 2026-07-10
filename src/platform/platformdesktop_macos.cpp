#include "platformdesktop.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QProcess>
#include <QSettings>
#include <QStyleFactory>
#include <QUrl>
#include <QWheelEvent>

void PlatformDesktop::applyApplicationStyle(QApplication *app) {
    if(QStyleFactory::keys().contains("Fusion"))
        app->setStyle(QStyleFactory::create("Fusion"));
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

QSettings *PlatformDesktop::createStateConfig() {
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
    return false;
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

bool PlatformDesktop::shouldIgnoreWheelEvent(QWheelEvent *event) {
    return event->phase() == Qt::ScrollBegin || event->phase() == Qt::ScrollEnd;
}

void PlatformDesktop::showInDirectory(const QString &selectedPath, const QString &fallbackDir) {
    if(selectedPath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fallbackDir));
        return;
    }

    QStringList args;
    args << "-e";
    args << "tell application \"Finder\"";
    args << "-e";
    args << "activate";
    args << "-e";
    args << "select POSIX file \"" + selectedPath + "\"";
    args << "-e";
    args << "end tell";
    QProcess::startDetached("osascript", args);
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
