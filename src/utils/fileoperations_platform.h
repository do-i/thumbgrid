#pragma once

#include <QString>

class QFileInfo;

namespace FileOperationsPlatform {

enum class RenameResult { Success, DestinationExists, CrossDevice, Unsupported, Error };
enum class FilesystemRelation { Same, Different, Unknown };

// Atomically rename the entry itself without replacing any destination, including a dangling link.
RenameResult renameNoReplace(const QString &srcPath, const QString &dstPath);
// Compare the source entry (without following links) with the destination directory.
FilesystemRelation filesystemRelation(const QString &path, const QString &dirPath);

bool canRemoveSource(const QFileInfo &file);
bool canReplaceExistingDestination(const QFileInfo &file);
bool isWritableParentDirectory(const QFileInfo &directory);
bool moveToTrash(const QString &filePath);

}
