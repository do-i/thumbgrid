#pragma once

#include <QString>

class QFileInfo;

namespace FileOperationsPlatform {

bool canRemoveSource(const QFileInfo &file);
bool canReplaceExistingDestination(const QFileInfo &file);
bool isWritableParentDirectory(const QFileInfo &directory);
bool moveToTrash(const QString &filePath);

}
