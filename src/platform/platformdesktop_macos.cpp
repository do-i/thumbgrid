#include "platformdesktop.h"

#include <QApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QStyleFactory>
#include <QUrl>

void PlatformDesktop::applyApplicationStyle(QApplication *app) {
    if(QStyleFactory::keys().contains("Fusion"))
        app->setStyle(QStyleFactory::create("Fusion"));
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

bool PlatformDesktop::setWallpaper(const QString &, QString *errorMessage) {
    if(errorMessage)
        *errorMessage = "Action is not supported on this platform";
    return false;
}
