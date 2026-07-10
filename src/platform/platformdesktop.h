#pragma once

#include <QString>

class QApplication;

namespace PlatformDesktop {

void applyApplicationStyle(QApplication *app);
void applyHighDpiPolicy();
QString contextMenuBorderRadius();
QString defaultMpvBinary();
void prepareApplicationEnvironment();
void showInDirectory(const QString &selectedPath, const QString &fallbackDir);
bool setWallpaper(const QString &path, QString *errorMessage = nullptr);

}
