// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).

#include "treecontextmenu.h"

TreeContextMenu::TreeContextMenu(QWidget *parent) : PopupMenuBase(parent) {
    auto *items = makeItemLayout(this);
    items->setContentsMargins(0, 4, 0, 4);
    addEntry(items, tr("Open"), ":/res/icons/common/menuitem/open16.png", &TreeContextMenu::openRequested);
    adjustSize();
    hide();
}

// a row without an action id: no shortcut hint, as the tree has no bindings for these
ContextMenuItem *TreeContextMenu::addEntry(QVBoxLayout *layout, const QString &text, const QString &iconPath,
                                           void (TreeContextMenu::*signal)(const QString &)) {
    ContextMenuItem *item = makeItem(text, iconPath);
    connect(item, &ContextMenuItem::pressed, this, [this, signal]() {
        hide();
        const QString path = mTarget;
        emit (this->*signal)(path);
    });
    layout->addWidget(item);
    return item;
}

void TreeContextMenu::openFor(const QString &path, QPoint globalPos) {
    mTarget = path;
    showAt(globalPos);
}
