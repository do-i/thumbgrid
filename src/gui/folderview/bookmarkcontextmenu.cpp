// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).

#include "bookmarkcontextmenu.h"

BookmarkContextMenu::BookmarkContextMenu(QWidget *parent) : PopupMenuBase(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    mBookmarkPage = new QWidget(this);
    auto *items = makeItemLayout(mBookmarkPage);
    items->setContentsMargins(0, 4, 0, 4);
    addEntry(items, "menuOpen", tr("Open"), ":/res/icons/common/menuitem/open16.png",
             [this](const QString &path) { emit openRequested(path); });
    addEntry(items, "menuNewWindow", tr("Open in new window"), QString(),
             [this](const QString &path) { emit newWindowRequested(path); });
    addSeparator(items);
    addEntry(items, "menuRename", tr("Rename…"), ":/res/icons/common/overlay/edit16.png",
             [this](const QString &path) { emit renameRequested(path); });
    addEntry(items, "menuCopyPath", tr("Copy path"), ":/res/icons/common/menuitem/copy16.png",
             [this](const QString &path) { emit copyPathRequested(path); });
    addEntry(items, "menuRemove", tr("Remove"), QString(),
             [this](const QString &path) { emit removeRequested(path); });
    root->addWidget(mBookmarkPage);

    mPanelPage = new QWidget(this);
    items = makeItemLayout(mPanelPage);
    items->setContentsMargins(0, 4, 0, 4);
    addEntry(items, "menuAddFolder", tr("Add folder…"), QString(), [this](const QString &) { emit addFolderRequested(); });
    mBookmarkHereItem = addEntry(items, "menuBookmarkHere", tr("Bookmark this folder"), QString(),
                                 [this](const QString &) { emit bookmarkHereRequested(); });
    root->addWidget(mPanelPage);

    adjustSize();
    hide();
}

// a row without an action id: no shortcut hint, as these have no bindings
ContextMenuItem *BookmarkContextMenu::addEntry(QVBoxLayout *layout, const QString &name, const QString &text,
                                               const QString &iconPath,
                                               std::function<void(const QString &)> action) {
    ContextMenuItem *item = makeItem(text, iconPath);
    item->setObjectName(name);
    connect(item, &ContextMenuItem::pressed, this, [this, action = std::move(action)]() {
        hide();
        action(mTarget);
    });
    layout->addWidget(item);
    return item;
}

void BookmarkContextMenu::prepareShow() {
    mBookmarkPage->setVisible(mForBookmark);
    mPanelPage->setVisible(!mForBookmark);
    adjustSize();
}

void BookmarkContextMenu::openForBookmark(const QString &path, QPoint globalPos) {
    mTarget = path;
    mForBookmark = true;
    showAt(globalPos);
}

void BookmarkContextMenu::openForPanel(bool canBookmarkHere, QPoint globalPos) {
    mTarget.clear();
    mForBookmark = false;
    mBookmarkHereItem->setEnabled(canBookmarkHere);
    showAt(globalPos);
}
