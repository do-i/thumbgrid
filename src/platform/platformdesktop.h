#pragma once

#include <QString>

namespace PlatformDesktop {

void showInDirectory(const QString &selectedPath, const QString &fallbackDir);
bool setWallpaper(const QString &path, QString *errorMessage = nullptr);

}
