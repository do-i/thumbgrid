#include "platformdesktop.h"

#include "proxystyle.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QSettings>
#include <QUrl>

#include <cwchar>
#include <string>

#include <windows.h>

void PlatformDesktop::applyApplicationStyle(QApplication *app) {
    app->setStyle(new ProxyStyle);
}

void PlatformDesktop::applyHighDpiPolicy() {
#if (QT_VERSION_MAJOR == 6)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif
}

bool PlatformDesktop::cacheDirectoryIsConfigurable() {
    return false;
}

QString PlatformDesktop::contextMenuBorderRadius() {
    return "0px";
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
    return QCoreApplication::applicationDirPath() + "/mpv.exe";
}

QString PlatformDesktop::folderViewInitialRootPath() {
    return QString();
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
    qputenv("QT_PLUGIN_PATH", "");
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
    if(selectedPath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fallbackDir));
        return;
    }

    QStringList args;
    args << "/select," << QDir::toNativeSeparators(selectedPath);
    QProcess::startDetached("explorer", args);
}

QString PlatformDesktop::shortcutsJsonPath(const QString &configDir) {
    return configDir + "/shortcuts.json";
}

int PlatformDesktop::slidePanelUpdateInterval() {
    return 8;
}

bool PlatformDesktop::supportsIcoSaveFormat() {
    return true;
}

bool PlatformDesktop::setWallpaper(const QString &path, QString *errorMessage) {
    Q_UNUSED(errorMessage)

    LONG status;
    HKEY hKey;
    status = RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_WRITE, &hKey);
    if((status == ERROR_SUCCESS) && (hKey != NULL)) {
        const wchar_t *value = L"WallpaperStyle";
        const wchar_t *data = L"10";
        status = RegSetValueExW(hKey,
                                value,
                                0,
                                REG_SZ,
                                reinterpret_cast<const BYTE *>(data),
                                static_cast<DWORD>((wcslen(data) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    const std::wstring wallpaperPath = path.toStdWString();
    SystemParametersInfoW(SPI_SETDESKWALLPAPER,
                          0,
                          const_cast<wchar_t *>(wallpaperPath.c_str()),
                          SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
    return true;
}

QString PlatformDesktop::thumbnailCacheDirectory(const QString &) {
    return QCoreApplication::applicationDirPath() + "/thumbnails";
}
