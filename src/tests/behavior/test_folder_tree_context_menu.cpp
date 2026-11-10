// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The tree's right-click menu targets the row under the pointer without navigating, and its items act on it.

#include "support/tree_test_fixture.h"

#include <QContextMenuEvent>
#include <QLabel>
#include <QSignalSpy>

class FolderTreeContextMenuTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void rightClickingARowOpensTheMenuForItWithoutNavigating();
    void rightClickingEmptySpaceOpensNothing();
    void theMenuKeyOpensTheMenuForTheCurrentRow();
    void openNavigatesToTheTarget();

private:
    TreeContextMenu *menu() const { return mFixture.folderView()->findChild<TreeContextMenu *>(); }
    QString pathLabelText() const { return mFixture.folderView()->findChild<QLabel *>("pathLabel")->text(); }
    // the event Qt sends to the viewport after a right-button press (QTest clicks do not generate it)
    void rightClickAt(const QPoint &viewportPos) {
        QTreeView *tree = mFixture.tree();
        QTest::mouseClick(tree->viewport(), Qt::RightButton, {}, viewportPos);
        QContextMenuEvent event(QContextMenuEvent::Mouse, viewportPos, tree->viewport()->mapToGlobal(viewportPos));
        QApplication::sendEvent(tree->viewport(), &event);
    }
    tgtest::TreeFixture mFixture;
};

void FolderTreeContextMenuTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path()), "The tree should list the home folder.");
    QVERIFY2(menu() == nullptr, "The menu should not exist before the first right-click.");
}

void FolderTreeContextMenuTest::rightClickingARowOpensTheMenuForItWithoutNavigating() {
    TreeViewCustom *tree = mFixture.tree();
    QTRY_VERIFY2(mFixture.isRowVisible("b"), "Folder b should be shown in the tree.");
    const QString before = pathLabelText();
    const QString currentRow = mFixture.currentTreePath();
    QSignalSpy selected(mFixture.folderView(), &FolderView::directorySelected);

    rightClickAt(tree->visualRect(mFixture.index("b")).center());

    QVERIFY2(menu() != nullptr, "The menu should be created by the right-click.");
    QVERIFY(menu()->isVisible());
    QCOMPARE(menu()->targetPath(), mFixture.path("b"));
    QCOMPARE(selected.count(), 0);
    QCOMPARE(pathLabelText(), before);
    QCOMPARE(mFixture.currentTreePath(), currentRow);
    menu()->hide();
}

void FolderTreeContextMenuTest::rightClickingEmptySpaceOpensNothing() {
    TreeViewCustom *tree = mFixture.tree();
    const QRect last = tree->visualRect(mFixture.index("c"));
    QVERIFY2(tree->viewport()->height() > last.bottom() + 20, "The tree should have empty space below its rows.");
    rightClickAt(QPoint(last.center().x(), tree->viewport()->height() - 5));
    QVERIFY(menu() && !menu()->isVisible());
}

void FolderTreeContextMenuTest::theMenuKeyOpensTheMenuForTheCurrentRow() {
    TreeViewCustom *tree = mFixture.tree();
    tree->setCurrentIndex(mFixture.index("a"));
    tree->setFocus();
    QSignalSpy selected(mFixture.folderView(), &FolderView::directorySelected);

    QTest::keyClick(tree, Qt::Key_Menu);
    QVERIFY(menu()->isVisible());
    QCOMPARE(menu()->targetPath(), mFixture.path("a"));
    menu()->hide();

    tree->setCurrentIndex(mFixture.index("b"));
    QTest::keyClick(tree, Qt::Key_F10, Qt::ShiftModifier);
    QVERIFY(menu()->isVisible());
    QCOMPARE(menu()->targetPath(), mFixture.path("b"));
    QCOMPARE(selected.count(), 0);
    menu()->hide();
}

void FolderTreeContextMenuTest::openNavigatesToTheTarget() {
    TreeViewCustom *tree = mFixture.tree();
    QSignalSpy selected(mFixture.folderView(), &FolderView::directorySelected);
    rightClickAt(tree->visualRect(mFixture.index("c")).center());
    QVERIFY(menu()->isVisible());

    QTest::keyClick(menu(), Qt::Key_Down);
    QTest::keyClick(menu(), Qt::Key_Return);

    QVERIFY(!menu()->isVisible());
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.at(0).at(0).toString(), mFixture.path("c"));
}

TG_BEHAVIOR_TEST_MAIN(FolderTreeContextMenuTest)

#include "test_folder_tree_context_menu.moc"
