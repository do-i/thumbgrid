#pragma once

#include <QString>

class QApplication;
class QSettings;
class QWheelEvent;

namespace PlatformDesktop {

void applyApplicationStyle(QApplication *app);
void applyHighDpiPolicy();
bool cacheDirectoryIsConfigurable();
QString contextMenuBorderRadius();
QSettings *createSettingsConfig();
QSettings *createStateConfig();
QSettings *createThemeConfig(const QString &configDir);
QString defaultCacheDirectory();
QString defaultMpvBinary();
QString folderViewInitialRootPath();
QString folderViewRootPathFor(const QString &path);
bool isWaylandPlatform();
bool needsWaylandCursorWorkaround();
void prepareApplicationEnvironment();
QString settingsConfigDirectory(const QSettings *settingsConfig);
bool shouldIgnoreWheelEvent(QWheelEvent *event);
void showInDirectory(const QString &selectedPath, const QString &fallbackDir);
QString shortcutsJsonPath(const QString &configDir);
int slidePanelUpdateInterval();
bool supportsIcoSaveFormat();
bool setWallpaper(const QString &path, QString *errorMessage = nullptr);
QString thumbnailCacheDirectory(const QString &cacheDir);

}
