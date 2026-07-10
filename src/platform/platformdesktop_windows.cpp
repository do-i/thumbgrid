#include "platformdesktop.h"

#include <QDesktopServices>
#include <QDir>
#include <QProcess>
#include <QUrl>

void PlatformDesktop::showInDirectory(const QString &selectedPath, const QString &fallbackDir) {
    if(selectedPath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fallbackDir));
        return;
    }

    QStringList args;
    args << "/select," << QDir::toNativeSeparators(selectedPath);
    QProcess::startDetached("explorer", args);
}
