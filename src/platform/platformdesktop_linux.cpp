#include "platformdesktop.h"

#include "components/scriptmanager/scriptmanager.h"

#include <QDesktopServices>
#include <QUrl>

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
