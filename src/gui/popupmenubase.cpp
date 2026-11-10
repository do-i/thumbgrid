// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).

#include "popupmenubase.h"

#include <algorithm>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QStyleOption>
#include "gui/customwidgets/dismisskey.h"

PopupMenuBase::PopupMenuBase(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::Popup);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoMousePropagation, true);
}

ContextMenuItem *PopupMenuBase::makeItem(const QString &text, const QString &iconPath, const QString &action) {
    auto *item = new ContextMenuItem(this);
    item->setText(text);
    item->setIconPath(iconPath);
    if(!action.isEmpty())
        item->setAction(action);
    return item;
}

QVBoxLayout *PopupMenuBase::makeItemLayout(QWidget *page) {
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    return layout;
}

void PopupMenuBase::addSeparator(QVBoxLayout *layout) {
    auto *separator = new QWidget(layout->parentWidget());
    separator->setAccessibleName("HLineSeparator");
    separator->setFixedHeight(1);
    separator->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    auto *sepLayout = new QVBoxLayout();
    sepLayout->setContentsMargins(11, 4, 11, 4);
    sepLayout->addWidget(separator);
    layout->addLayout(sepLayout);
}

void PopupMenuBase::showAt(QPoint pos) {
    // a handful of lookups per open; a longer hint may widen the menu
    bool hintsChanged = false;
    for(ContextMenuItem *item : findChildren<ContextMenuItem*>())
        hintsChanged |= item->refreshShortcut();
    prepareShow();
    if(hintsChanged)
        adjustSize();
    QRect geom = geometry();
    geom.moveTopLeft(pos);
    clampToScreen(geom);
    setGeometry(geom);
    show();
}

// past the bottom it opens upward from the cursor; past the right edge it is pushed left
void PopupMenuBase::clampToScreen(QRect &geom) const {
    auto screen = QGuiApplication::screenAt(cursor().pos());
    if(!screen)
        screen = QGuiApplication::primaryScreen();
    if(screen) {
        if(geom.bottom() > screen->geometry().bottom())
            geom.moveBottom(cursor().pos().y());
        if(geom.right() > screen->geometry().right())
            geom.moveRight(screen->geometry().right());
    }
}

// a click that no row takes closes the menu
void PopupMenuBase::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);
    hide();
}

void PopupMenuBase::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

// rows the keyboard can reach, top to bottom; rows of hidden pages are not visible
QList<ContextMenuItem*> PopupMenuBase::navigableItems() {
    QList<ContextMenuItem*> items;
    for(ContextMenuItem *item : findChildren<ContextMenuItem*>())
        if(item->isVisible() && item->isEnabled())
            items << item;
    std::sort(items.begin(), items.end(), [this](ContextMenuItem *a, ContextMenuItem *b) {
        return a->mapTo(this, QPoint(0, 0)).y() < b->mapTo(this, QPoint(0, 0)).y();
    });
    return items;
}

void PopupMenuBase::setHighlighted(ContextMenuItem *item) {
    for(ContextMenuItem *each : findChildren<ContextMenuItem*>())
        each->setHighlighted(each == item);
    mHighlighted = item;
}

// first press highlights the first (Down) or last (Up) row; then wraps around
void PopupMenuBase::moveHighlight(int step) {
    const QList<ContextMenuItem*> items = navigableItems();
    if(items.isEmpty())
        return;
    const int current = items.indexOf(mHighlighted.data());
    int next;
    if(current < 0)
        next = (step > 0) ? 0 : items.count() - 1;
    else
        next = (current + step + items.count()) % items.count();
    setHighlighted(items.at(next));
}

void PopupMenuBase::activateHighlighted() {
    ContextMenuItem *item = mHighlighted.data();
    if(!item || !navigableItems().contains(item))
        return;
    // rows that open another page (Open with, Convert to, Back) keep the menu
    // open, the way a click on them does; everything else closes it first
    if(item->passesClicksThrough()) {
        hide();
        setHighlighted(nullptr);
    }
    item->trigger();
}

void PopupMenuBase::keyPressEvent(QKeyEvent *event) {
    if(handleDismissKey(this, event))
        return;
    switch(event->key()) {
    case Qt::Key_Up:
        moveHighlight(-1);
        break;
    case Qt::Key_Down:
        moveHighlight(1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        activateHighlighted();
        break;
    default:
        actionManager->processEvent(event);
    }
}
