// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The keyboard-highlighted row of a popup menu is painted, not only flagged, and
// the script and shortcut dialogs tab through their fields in reading order.

#include "support/thumbgrid_test_support.h"

#include <QPushButton>

#include "gui/contextmenu.h"
#include "gui/customwidgets/contextmenuitem.h"
#include "gui/dialogs/scripteditordialog.h"
#include "gui/dialogs/shortcutcreatordialog.h"

class KeyboardFocusIsVisibleTest : public QObject {
    Q_OBJECT

private slots:
    void theHighlightedMenuRowIsPaintedDifferently();
    void theScriptDialogTabsThroughItsFieldsInReadingOrder();
    void theShortcutDialogTabsFromTheSequenceToTheGestureButtonThenOk();

private:
    // next widget in the focus chain a Tab press can land on
    static QWidget *nextFocusable(QWidget *from) {
        QWidget *w = from->nextInFocusChain();
        while(w != from && !(w->focusPolicy() & Qt::TabFocus && w->isEnabled() && w->isVisible()))
            w = w->nextInFocusChain();
        return w;
    }
};

void KeyboardFocusIsVisibleTest::theHighlightedMenuRowIsPaintedDifferently() {
    ContextMenu menu;
    menu.showAt(QPoint(10, 10));
    QTRY_VERIFY(menu.isVisible());
    QTest::keyClick(&menu, Qt::Key_Down);
    ContextMenuItem *row = nullptr;
    for(ContextMenuItem *item : menu.findChildren<ContextMenuItem *>())
        if(item->isHighlighted())
            row = item;
    QVERIFY2(row, "Down should highlight a row.");
    const QImage lit = row->grab().toImage();
    row->setHighlighted(false);
    const QImage plain = row->grab().toImage();
    QVERIFY2(lit != plain, "The highlighted row should look different from the plain one.");
}

void KeyboardFocusIsVisibleTest::theScriptDialogTabsThroughItsFieldsInReadingOrder() {
    ScriptEditorDialog dialog;
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *name = dialog.findChild<QWidget *>("nameLineEdit");
    auto *path = dialog.findChild<QWidget *>("pathLineEdit");
    auto *browse = dialog.findChild<QWidget *>("fileSelectButton");
    auto *wait = dialog.findChild<QWidget *>("blockingCheckBox");
    QVERIFY(name && path && browse && wait);
    QCOMPARE(nextFocusable(name), path);
    QCOMPARE(nextFocusable(path), browse);
    QCOMPARE(nextFocusable(browse), wait);
}

void KeyboardFocusIsVisibleTest::theShortcutDialogTabsFromTheSequenceToTheGestureButtonThenOk() {
    ShortcutCreatorDialog dialog;
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *sequence = dialog.findChild<QWidget *>("sequenceEdit");
    auto *ok = dialog.findChild<QWidget *>("okButton");
    QVERIFY(sequence && ok);
    QWidget *gestures = nextFocusable(sequence);
    QCOMPARE(gestures->accessibleName(), QStringLiteral("mouseGestureButton"));
    QCOMPARE(nextFocusable(gestures), ok);
}

TG_BEHAVIOR_TEST_MAIN(KeyboardFocusIsVisibleTest)

#include "test_keyboard_focus_is_visible_and_tab_order_follows_layout.moc"
