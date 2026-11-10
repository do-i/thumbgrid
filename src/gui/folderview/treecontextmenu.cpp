// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).

#include "treecontextmenu.h"

TreeContextMenu::TreeContextMenu(QWidget *parent) : PopupMenuBase(parent) {
    auto *items = makeItemLayout(this);
    items->setContentsMargins(0, 4, 0, 4);
    addEntry(items, "menuOpen", tr("Open"), ":/res/icons/common/menuitem/open16.png",
             &TreeContextMenu::openRequested);
    addEntry(items, "menuNewWindow", tr("Open in new window"), QString(), &TreeContextMenu::newWindowRequested);
    addSeparator(items);
    addEntry(items, "menuNewFolder", tr("New folder…"), QString(), &TreeContextMenu::newFolderRequested);
    mRenameItem = addEntry(items, "menuRename", tr("Rename…"), ":/res/icons/common/overlay/edit16.png",
                           &TreeContextMenu::renameRequested);
    mTrashItem = addEntry(items, "menuTrash", tr("Move to trash"), ":/res/icons/common/menuitem/trash16.png",
                          &TreeContextMenu::trashRequested);
    // destructive and not undoable, set apart as in the grid menu; the confirmation prompt follows
    addSeparator(items);
    mDeleteItem = addEntry(items, "menuDelete", tr("Delete permanently"), ":/res/icons/common/overlay/close16.png",
                           &TreeContextMenu::deleteRequested);
    addSeparator(items);
    addEntry(items, "menuCopyPath", tr("Copy path"), ":/res/icons/common/menuitem/copy16.png",
             &TreeContextMenu::copyPathRequested);
    mBookmarkItem = addEntry(items, "menuAddBookmark", tr("Add to bookmarks"), QString(),
                             &TreeContextMenu::addBookmarkRequested);
    addSeparator(items);
    addEntry(items, "menuCollapseAll", tr("Collapse all"), QString(), &TreeContextMenu::collapseAllRequested);
    adjustSize();
    hide();
}

// a row without an action id: no shortcut hint, as the tree has no bindings for these
ContextMenuItem *TreeContextMenu::addEntry(QVBoxLayout *layout, const QString &name, const QString &text,
                                           const QString &iconPath, void (TreeContextMenu::*signal)(const QString &)) {
    ContextMenuItem *item = makeItem(text, iconPath);
    item->setObjectName(name);
    connect(item, &ContextMenuItem::pressed, this, [this, signal]() {
        hide();
        const QString path = mTarget;
        emit (this->*signal)(path);
    });
    layout->addWidget(item);
    return item;
}

void TreeContextMenu::openFor(const TreeMenuTarget &target, QPoint globalPos) {
    mTarget = target.path;
    mBookmarkItem->setEnabled(!target.bookmarked);
    mRenameItem->setEnabled(!target.protectedFolder);
    mTrashItem->setEnabled(!target.protectedFolder);
    mDeleteItem->setEnabled(!target.protectedFolder);
    showAt(globalPos);
}
