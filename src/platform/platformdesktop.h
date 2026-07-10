#pragma once

#include <QString>

class QApplication;
class QSettings;

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
void prepareApplicationEnvironment();
QString settingsConfigDirectory(const QSettings *settingsConfig);
void showInDirectory(const QString &selectedPath, const QString &fallbackDir);
QString shortcutsJsonPath(const QString &configDir);
bool setWallpaper(const QString &path, QString *errorMessage = nullptr);
QString thumbnailCacheDirectory(const QString &cacheDir);

}
