#pragma once

#include <QString>

namespace PlatformDesktop {

QString contextMenuBorderRadius();
QString defaultMpvBinary();
void showInDirectory(const QString &selectedPath, const QString &fallbackDir);
bool setWallpaper(const QString &path, QString *errorMessage = nullptr);

}
