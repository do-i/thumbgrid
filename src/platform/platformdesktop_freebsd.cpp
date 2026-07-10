#include "platformdesktop.h"

#include "components/scriptmanager/scriptmanager.h"
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
    return "/usr/local/bin/mpv";
}

void PlatformDesktop::prepareApplicationEnvironment() {
}

void PlatformDesktop::showInDirectory(const QString &selectedPath, const QString &fallbackDir) {
    if(selectedPath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fallbackDir));
        return;
    }

    QString fm = ScriptManager::runCommand("xdg-mime query default inode/directory");
    if(fm.contains("dolphin"))
        ScriptManager::runCommandDetached("dolphin --select " + selectedPath);
    else if(fm.contains("nautilus"))
        ScriptManager::runCommandDetached("nautilus --select " + selectedPath);
    else
        QDesktopServices::openUrl(QUrl::fromLocalFile(fallbackDir));
}

bool PlatformDesktop::setWallpaper(const QString &path, QString *errorMessage) {
    const QString session = QString::fromLocal8Bit(qgetenv("DESKTOP_SESSION")).toLower();
    if(session.contains("plasma")) {
        ScriptManager::runCommand("plasma-apply-wallpaperimage \"" + path + "\"");
        return true;
    }
    if(session.contains("gnome")) {
        ScriptManager::runCommand("gsettings set org.gnome.desktop.background picture-uri \"" + path + "\"");
        return true;
    }

    if(errorMessage)
        *errorMessage = "Action is not supported in your desktop session (\"" + session + "\")";
    return false;
}
