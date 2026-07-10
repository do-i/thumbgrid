#include "platformdesktop.h"

#include "proxystyle.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QProcess>
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

QString PlatformDesktop::contextMenuBorderRadius() {
    return "0px";
}

QString PlatformDesktop::defaultMpvBinary() {
    return QCoreApplication::applicationDirPath() + "/mpv.exe";
}

void PlatformDesktop::prepareApplicationEnvironment() {
    qputenv("QT_PLUGIN_PATH", "");
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
