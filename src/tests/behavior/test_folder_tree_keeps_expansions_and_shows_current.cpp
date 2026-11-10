// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Navigating keeps the folders the user expanded, shows the current folder's row
// as soon as its parent is listed, and is a no-op for the folder already current.

#include "support/tree_test_fixture.h"

#include <QScrollBar>
#include <QSignalSpy>

class FolderTreeKeepsExpansionsTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void navigatingKeepsTheFoldersTheUserExpanded();
    void aDeepFolderIsShownOnceItsParentIsListed();
    void openingTheCurrentFolderAgainChangesNothing();

private:
    tgtest::TreeFixture mFixture;
};

void FolderTreeKeepsExpansionsTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.addFolders("tall", 80), "The tall folder's children should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path()), "The tree should list the home folder.");
}

void FolderTreeKeepsExpansionsTest::navigatingKeepsTheFoldersTheUserExpanded() {
    TreeViewCustom *tree = mFixture.tree();
    tree->expand(mFixture.index("a"));
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path("a")), "The tree should list folder a.");

    QVERIFY2(mFixture.navigate("b"), "Opening b should succeed.");
    QCOMPARE(mFixture.currentTreePath(), mFixture.path("b"));
    QVERIFY2(tree->isExpanded(mFixture.index("a")), "Folder a should stay expanded after opening b.");

    // the user folds away the branch holding the current folder, then navigates on
    QVERIFY2(mFixture.navigate("a/a1"), "Opening a/a1 should succeed.");
    tree->collapse(mFixture.index("a"));
    tree->expand(mFixture.index("c"));
    QVERIFY2(mFixture.navigate("b"), "Opening b again should succeed.");
    QVERIFY2(tree->isExpanded(mFixture.index("c")), "Folder c should stay expanded after opening b.");
    QVERIFY2(!tree->isExpanded(mFixture.index("a")), "Folder a should stay collapsed after opening b.");
}

void FolderTreeKeepsExpansionsTest::aDeepFolderIsShownOnceItsParentIsListed() {
    TreeViewCustom *tree = mFixture.tree();
    QVERIFY2(!tree->isExpanded(mFixture.index("tall")), "Folder tall should start collapsed.");

    QVERIFY2(mFixture.navigate("tall/f075"), "Opening tall/f075 should succeed.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path("tall")), "The tree should list folder tall.");

    QVERIFY2(tree->verticalScrollBar()->maximum() > 0, "The tree should be taller than its viewport.");
    QCOMPARE(mFixture.currentTreePath(), mFixture.path("tall/f075"));
    QVERIFY2(tree->selectionModel()->isSelected(mFixture.index("tall/f075")), "The row should be selected.");
    QVERIFY2(mFixture.isRowVisible("tall/f075"), "The row should be scrolled into view without a delay.");
}

void FolderTreeKeepsExpansionsTest::openingTheCurrentFolderAgainChangesNothing() {
    TreeViewCustom *tree = mFixture.tree();
    QVERIFY2(mFixture.navigate("a/a2"), "Opening a/a2 should succeed.");
    QCOMPARE(mFixture.currentTreePath(), mFixture.path("a/a2"));
    // the user hides the current row and opens another branch
    tree->collapse(mFixture.index("a"));
    tree->expand(mFixture.index("tall"));
    tree->verticalScrollBar()->setValue(tree->verticalScrollBar()->maximum());
    const int scroll = tree->verticalScrollBar()->value();
    QSignalSpy selection(tree->selectionModel(), &QItemSelectionModel::selectionChanged);
    QSignalSpy current(tree->selectionModel(), &QItemSelectionModel::currentChanged);

    mFixture.folderView()->setDirectoryPath(mFixture.path("a/a2"));

    QVERIFY2(!tree->isExpanded(mFixture.index("a")), "Folder a should stay collapsed.");
    QVERIFY2(tree->isExpanded(mFixture.index("tall")), "Folder tall should stay expanded.");
    QCOMPARE(tree->verticalScrollBar()->value(), scroll);
    QCOMPARE(selection.count(), 0);
    QCOMPARE(current.count(), 0);
}

TG_BEHAVIOR_TEST_MAIN(FolderTreeKeepsExpansionsTest)

#include "test_folder_tree_keeps_expansions_and_shows_current.moc"
