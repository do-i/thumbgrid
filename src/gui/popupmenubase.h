// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Shared base of the right-click menus: placement, drawing, keys, items.
// A new menu: derive, then in its constructor
//     auto *items = makeItemLayout(this);
//     items->setContentsMargins(0, 4, 0, 4);
//     items->addWidget(makeItem(tr("Copy path"), ":/res/icons/common/menuitem/copy16.png", "copyPathClipboard"));
//     addSeparator(items);
// and call showAt(globalPos) on right-click. The QSS styles every PopupMenuBase.

#pragma once

#include <QPointer>
#include <QWidget>
#include <QVBoxLayout>
#include "gui/customwidgets/contextmenuitem.h"

class PopupMenuBase : public QWidget {
    Q_OBJECT
public:
    explicit PopupMenuBase(QWidget *parent = nullptr);

public slots:
    // opens at pos (global), kept on the cursor's screen
    void showAt(QPoint pos);

protected:
    // a row with icon and text; an action also fills in its shortcut hint
    ContextMenuItem *makeItem(const QString &text, const QString &iconPath,
                              const QString &action = QString());
    static QVBoxLayout *makeItemLayout(QWidget *page);
    void addSeparator(QVBoxLayout *layout);
    void clampToScreen(QRect &geom) const;
    // runs before every showAt(): reset page, refresh hints
    virtual void prepareShow() {}
    // keyboard highlight over the visible, enabled rows; clear it on page switches
    void setHighlighted(ContextMenuItem *item);

    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QPointer<ContextMenuItem> mHighlighted;
    QList<ContextMenuItem*> navigableItems();
    void moveHighlight(int step);
    void activateHighlighted();
};
