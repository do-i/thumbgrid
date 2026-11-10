// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Folder tree right-click popup. Acts on the folder it was opened for, not the current folder.

#pragma once

#include "gui/popupmenubase.h"

class TreeContextMenu : public PopupMenuBase {
    Q_OBJECT
public:
    explicit TreeContextMenu(QWidget *parent = nullptr);

    // opens at globalPos for the folder at path
    void openFor(const QString &path, QPoint globalPos);
    QString targetPath() const { return mTarget; }

signals:
    // every signal carries the target path, sent after the menu has closed
    void openRequested(const QString &path);

private:
    ContextMenuItem *addEntry(QVBoxLayout *layout, const QString &text, const QString &iconPath,
                              void (TreeContextMenu::*signal)(const QString &));

    QString mTarget;
};
