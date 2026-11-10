// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The grid context menu takes Up/Down/Enter like the picture menu: the arrows
// skip disabled rows and wrap, Enter runs the row or opens its page.

#include "support/thumbgrid_test_support.h"

#include <QSignalSpy>

#include "gui/customwidgets/contextmenuitem.h"
#include "gui/folderview/gridcontextmenu.h"

class GridContextMenuKeyboardNavigationTest : public QObject {
    Q_OBJECT

private slots:
    void downThenEnterRunsTheFirstEnabledRow();
    void enterOnConvertOpensItsPage();
};

namespace {

// object name when the row has one (labels may be reworded), else its text
QString highlighted(GridContextMenu *menu) {
    for(ContextMenuItem *item : menu->findChildren<ContextMenuItem *>())
        if(item->isHighlighted())
            return item->objectName().isEmpty() ? item->text() : item->objectName();
    return QString();
}

} // namespace

void GridContextMenuKeyboardNavigationTest::downThenEnterRunsTheFirstEnabledRow() {
    QSignalSpy renameAction(actionManager, &ActionManager::renameFile);
    GridContextMenu menu;
    SelectionInfo oneFolder;
    oneFolder.dirCount = 1; // Convert and Resize are disabled, Rename is the first enabled row
    menu.setSelectionInfo(oneFolder);
    menu.showAt(QPoint(10, 10));
    QTRY_VERIFY(menu.isVisible());
    QCOMPARE(highlighted(&menu), QString());

    QTest::keyClick(&menu, Qt::Key_Up);
    QCOMPARE(highlighted(&menu), QStringLiteral("Settings"));
    QTest::keyClick(&menu, Qt::Key_Down);
    QCOMPARE(highlighted(&menu), QStringLiteral("menuRename"));

    QTest::keyClick(&menu, Qt::Key_Return);
    QCOMPARE(renameAction.count(), 1);
    QVERIFY2(!menu.isVisible(), "Running a row should close the menu.");
}

void GridContextMenuKeyboardNavigationTest::enterOnConvertOpensItsPage() {
    GridContextMenu menu;
    SelectionInfo oneImage;
    oneImage.fileCount = 1;
    oneImage.allConvertible = true;
    menu.setSelectionInfo(oneImage);
    menu.showAt(QPoint(10, 10));
    QTRY_VERIFY(menu.isVisible());

    QTest::keyClick(&menu, Qt::Key_Down);
    QCOMPARE(highlighted(&menu), QStringLiteral("menuConvert"));
    QTest::keyClick(&menu, Qt::Key_Enter);
    QVERIFY2(menu.isVisible(), "Convert to should switch page, not close the menu.");
    QCOMPARE(highlighted(&menu), QString());
    QTest::keyClick(&menu, Qt::Key_Down);
    QCOMPARE(highlighted(&menu), QStringLiteral("Back"));
}

TG_BEHAVIOR_TEST_MAIN(GridContextMenuKeyboardNavigationTest)

#include "test_grid_context_menu_keyboard_navigation.moc"
