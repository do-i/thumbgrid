// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Folder operations the tree asks Core to run: (op, folder path, argument such as a new name).

#pragma once

#include <QDir>
#include <QString>

// CopyPath, Trash, Delete: path is the folder. Rename: arg is the new name.
// NewFolder: path is the parent folder, arg the new folder's name.
enum class TreeFolderOp { CopyPath, NewFolder, Rename, Trash, Delete };

// Home, the file system root and the folders above home are never renamed or removed from the tree.
inline bool isProtectedTreeFolder(const QString &path) {
    const QString clean = QDir::cleanPath(path);
    const QString home = QDir::cleanPath(QDir::homePath());
    return clean == QDir::cleanPath(QDir::rootPath()) || clean == home || home.startsWith(clean + QLatin1Char('/'));
}
