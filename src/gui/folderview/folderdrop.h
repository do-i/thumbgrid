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

// on enter, move and drop, so the source shows the matching cursor
inline void acceptFolderDrop(QDropEvent *event) {
    event->setDropAction(folderDropCopies(event) ? Qt::CopyAction : Qt::MoveAction);
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
