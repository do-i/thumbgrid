// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Bookmarks panel right-click popup: one page for a bookmark row, one for the empty panel.

#pragma once

#include <functional>
#include "gui/popupmenubase.h"

class BookmarkContextMenu : public PopupMenuBase {
    Q_OBJECT
public:
    explicit BookmarkContextMenu(QWidget *parent = nullptr);

    // opens at globalPos for the bookmark at path
    void openForBookmark(const QString &path, QPoint globalPos);
    // opens at globalPos for the panel itself; canBookmarkHere is false when the current folder is bookmarked
    void openForPanel(bool canBookmarkHere, QPoint globalPos);
    QString targetPath() const { return mTarget; }

signals:
    // sent after the menu has closed
    void openRequested(const QString &path);
    void newWindowRequested(const QString &path);
    void copyPathRequested(const QString &path);
    void removeRequested(const QString &path);
    void addFolderRequested();
    void bookmarkHereRequested();

protected:
    void prepareShow() override;

private:
    ContextMenuItem *addEntry(QVBoxLayout *layout, const QString &name, const QString &text,
                              const QString &iconPath, std::function<void(const QString &)> action);

    QWidget *mBookmarkPage = nullptr;
    QWidget *mPanelPage = nullptr;
    ContextMenuItem *mBookmarkHereItem = nullptr;
    QString mTarget;
    bool mForBookmark = true;
};
