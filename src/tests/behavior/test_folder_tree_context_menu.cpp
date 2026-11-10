// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The tree's right-click menu targets the row under the pointer without navigating, and its items act on it.

#include "support/tree_test_fixture.h"

#include "gui/folderview/bookmarksitem.h"
#include "utils/newwindow.h"

#include <QClipboard>
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
    void copyPathPutsTheTargetPathOnTheClipboard();
    void addToBookmarksStoresTheTargetAndUpdatesThePanel();
    void openInNewWindowLaunchesTheTargetFolder();
    void collapseAllKeepsOnlyThePathToTheCurrentFolder();
    void middleClickingARowOpensItInANewWindow();

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
    // right-clicks the row of rel and triggers the menu item with that object name
    void triggerOn(const QString &rel, const QString &itemName) {
        rightClickAt(mFixture.tree()->visualRect(mFixture.index(rel)).center());
        QVERIFY2(menu() && menu()->isVisible(), "The menu should open.");
        auto *entry = menu()->findChild<ContextMenuItem *>(itemName);
        QVERIFY2(entry != nullptr, qPrintable(itemName));
        QVERIFY2(entry->isEnabled(), qPrintable(itemName));
        entry->trigger();
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

void FolderTreeContextMenuTest::copyPathPutsTheTargetPathOnTheClipboard() {
    QApplication::clipboard()->setText("untouched");
    triggerOn("a", "menuCopyPath");
    QCOMPARE(QApplication::clipboard()->text(), QDir::toNativeSeparators(mFixture.path("a")));
    QVERIFY2(!menu()->isVisible(), "The menu should close.");
}

void FolderTreeContextMenuTest::addToBookmarksStoresTheTargetAndUpdatesThePanel() {
    auto *panel = mFixture.folderView()->findChild<BookmarksWidget *>();
    QVERIFY(panel != nullptr);
    auto panelHas = [&](const QString &path) {
        for(auto *each : panel->findChildren<BookmarksItem *>())
            if(each->path() == path)
                return true;
        return false;
    };
    const QString target = mFixture.path("b");
    QVERIFY(!panelHas(target) && !settings->bookmarks().contains(target));

    triggerOn("b", "menuAddBookmark");

    QVERIFY2(settings->bookmarks().contains(target), "The bookmark should be stored.");
    QVERIFY2(panelHas(target), "The bookmarks panel should list the folder.");

    rightClickAt(mFixture.tree()->visualRect(mFixture.index("b")).center());
    QVERIFY2(!menu()->findChild<ContextMenuItem *>("menuAddBookmark")->isEnabled(),
             "A bookmarked folder cannot be added again.");
    menu()->hide();
    rightClickAt(mFixture.tree()->visualRect(mFixture.index("c")).center());
    QVERIFY(menu()->findChild<ContextMenuItem *>("menuAddBookmark")->isEnabled());
    menu()->hide();
}

void FolderTreeContextMenuTest::openInNewWindowLaunchesTheTargetFolder() {
    QStringList launched;
    NewWindow::setLauncher([&](const QString &path) { launched << path; });
    triggerOn("a", "menuNewWindow");
    NewWindow::setLauncher({});
    QCOMPARE(launched, QStringList{mFixture.path("a")});
}

void FolderTreeContextMenuTest::collapseAllKeepsOnlyThePathToTheCurrentFolder() {
    TreeViewCustom *tree = mFixture.tree();
    QVERIFY(mFixture.navigate("a/a1"));
    QVERIFY(mFixture.waitForTreeLoaded(mFixture.path("a")));
    QTRY_VERIFY(tree->isExpanded(mFixture.index("a")));
    QVERIFY(mFixture.waitForTreeLoaded(mFixture.path("a")));
    tree->expand(mFixture.index("c"));
    QVERIFY(tree->isExpanded(mFixture.index("c")));
    QTRY_COMPARE(mFixture.currentTreePath(), mFixture.path("a/a1"));

    triggerOn("b", "menuCollapseAll");

    QVERIFY2(!tree->isExpanded(mFixture.index("c")), "An unrelated folder should fold.");
    QVERIFY2(tree->isExpanded(mFixture.index("a")), "The parent of the current folder stays open.");
    QCOMPARE(mFixture.currentTreePath(), mFixture.path("a/a1"));
    QTRY_VERIFY2(mFixture.isRowVisible("a/a1"), "The current folder stays visible.");
}

void FolderTreeContextMenuTest::middleClickingARowOpensItInANewWindow() {
    TreeViewCustom *tree = mFixture.tree();
    QStringList launched;
    NewWindow::setLauncher([&](const QString &path) { launched << path; });
    QSignalSpy selected(mFixture.folderView(), &FolderView::directorySelected);
    const QString before = pathLabelText();

    QTest::mouseClick(tree->viewport(), Qt::MiddleButton, {}, tree->visualRect(mFixture.index("b")).center());
    NewWindow::setLauncher({});

    QCOMPARE(launched, QStringList{mFixture.path("b")});
    QCOMPARE(selected.count(), 0);
    QCOMPARE(pathLabelText(), before);
}

TG_BEHAVIOR_TEST_MAIN(FolderTreeContextMenuTest)

#include "test_folder_tree_context_menu.moc"
