// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Files dropped on a folder (tree row or bookmark): a plain drop moves, Ctrl held copies.

#pragma once

#include <QDropEvent>
#include <QMimeData>
#include <QStringList>
#include <QUrl>

inline bool folderDropCopies(const QDropEvent *event) {
    return event->modifiers().testFlag(Qt::ControlModifier);
}

// on drag enter and move, so the pointer shows what a drop will do
inline void acceptFolderDrop(QDropEvent *event) {
    event->setDropAction(folderDropCopies(event) ? Qt::CopyAction : Qt::MoveAction);
    event->accept();
}

// The app moves the files itself after a prompt; a reported MoveAction would let the source
// delete its originals first, so the source is always told Copy
// (TargetMoveAction was not verified to be non-destructive on xcb/Wayland).
inline void acceptFolderDropped(QDropEvent *event) {
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

inline QStringList droppedLocalPaths(const QMimeData *mimeData) {
    QStringList paths;
    for(const QUrl &url : mimeData->urls()) {
        if(url.isLocalFile())
            paths << url.toLocalFile();
    }
    return paths;
}
