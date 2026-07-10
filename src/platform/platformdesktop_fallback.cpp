#include "platformdesktop.h"

#include "proxystyle.h"

#include <QApplication>
#include <QDesktopServices>
#include <QUrl>

void PlatformDesktop::applyApplicationStyle(QApplication *app) {
    app->setStyle(new ProxyStyle);
}

void PlatformDesktop::applyHighDpiPolicy() {
}

QString PlatformDesktop::contextMenuBorderRadius() {
    return "3px";
}

QString PlatformDesktop::defaultMpvBinary() {
    return QString();
}

void PlatformDesktop::prepareApplicationEnvironment() {
}

void PlatformDesktop::showInDirectory(const QString &selectedPath, const QString &fallbackDir) {
    Q_UNUSED(selectedPath)
    QDesktopServices::openUrl(QUrl::fromLocalFile(fallbackDir));
}

bool PlatformDesktop::setWallpaper(const QString &, QString *errorMessage) {
    if(errorMessage)
        *errorMessage = "Action is not supported on this platform";
    return false;
}
