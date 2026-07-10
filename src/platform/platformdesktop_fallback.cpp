#include "platformdesktop.h"

#include <QDesktopServices>
#include <QUrl>

QString PlatformDesktop::contextMenuBorderRadius() {
    return "3px";
}

QString PlatformDesktop::defaultMpvBinary() {
    return QString();
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
