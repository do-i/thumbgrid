// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).

#include "popupmenubase.h"

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
    prepareShow();
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

void PopupMenuBase::keyPressEvent(QKeyEvent *event) {
    if(!handleDismissKey(this, event))
        actionManager->processEvent(event);
}
