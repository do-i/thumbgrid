#include "platformdesktop.h"

#include <QDesktopServices>
#include <QUrl>

void PlatformDesktop::showInDirectory(const QString &selectedPath, const QString &fallbackDir) {
    Q_UNUSED(selectedPath)
    QDesktopServices::openUrl(QUrl::fromLocalFile(fallbackDir));
}

bool PlatformDesktop::setWallpaper(const QString &, QString *errorMessage) {
    if(errorMessage)
        *errorMessage = "Action is not supported on this platform";
    return false;
}
