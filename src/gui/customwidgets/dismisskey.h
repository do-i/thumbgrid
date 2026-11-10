// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The one Esc path for popups, menus and overlays.

#pragma once

#include <QKeyEvent>

// Esc hides the widget and is not passed on (that would also run the global
// Esc binding). Calls W's own hide(), which overlays redeclare non-virtually.
template<typename W>
bool handleDismissKey(W *widget, QKeyEvent *event) {
    if(event->key() != Qt::Key_Escape)
        return false;
    event->accept();
    widget->hide();
    return true;
}
