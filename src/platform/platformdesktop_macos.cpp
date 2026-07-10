#include "platformdesktop.h"

#include <QDesktopServices>
#include <QProcess>
#include <QUrl>

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
