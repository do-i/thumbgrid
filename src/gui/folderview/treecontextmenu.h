// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Folder tree right-click popup. Acts on the folder it was opened for, not the current folder.

#pragma once

#include "gui/popupmenubase.h"

// what the menu needs to know about the row it opens for
struct TreeMenuTarget {
    QString path;
    bool bookmarked = false;
    // home and the root: no rename, no removal
    bool protectedFolder = false;
};

class TreeContextMenu : public PopupMenuBase {
    Q_OBJECT
public:
    explicit TreeContextMenu(QWidget *parent = nullptr);

    // opens at globalPos for the folder in target
    void openFor(const TreeMenuTarget &target, QPoint globalPos);
    QString targetPath() const { return mTarget; }

signals:
    // every signal carries the target path, sent after the menu has closed
    void openRequested(const QString &path);
    void newWindowRequested(const QString &path);
    void copyPathRequested(const QString &path);
    void addBookmarkRequested(const QString &path);
    void newFolderRequested(const QString &path);
    void renameRequested(const QString &path);
    void trashRequested(const QString &path);
    void deleteRequested(const QString &path);
    void collapseAllRequested(const QString &path);

private:
    ContextMenuItem *addEntry(QVBoxLayout *layout, const QString &name, const QString &text,
                              const QString &iconPath, void (TreeContextMenu::*signal)(const QString &));

    QString mTarget;
    ContextMenuItem *mBookmarkItem = nullptr;
    ContextMenuItem *mRenameItem = nullptr;
    ContextMenuItem *mTrashItem = nullptr;
    ContextMenuItem *mDeleteItem = nullptr;
};
