// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The folder tree and the app agree on the current folder in both directions.

#include "support/tree_test_fixture.h"

#include <QLabel>
#include <QSignalSpy>

class FolderTreeFollowsNavigationTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void clickingAFolderInTheTreeOpensIt();
    void openingAFolderSelectsItInTheTree();

private:
    tgtest::TreeFixture mFixture;
};

void FolderTreeFollowsNavigationTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path()), "The tree should list the home folder.");
}

void FolderTreeFollowsNavigationTest::clickingAFolderInTheTreeOpensIt() {
    TreeViewCustom *tree = mFixture.tree();
    QTRY_VERIFY2(mFixture.isRowVisible("b"), "Folder b should be shown in the tree.");
    QSignalSpy selected(mFixture.folderView(), &FolderView::directorySelected);

    QTest::mouseClick(tree->viewport(), Qt::LeftButton, {}, tree->visualRect(mFixture.index("b")).center());

    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.at(0).at(0).toString(), mFixture.path("b"));
    auto pathLabel = mFixture.folderView()->findChild<QLabel *>("pathLabel");
    QVERIFY2(pathLabel != nullptr, "The path label should exist.");
    QTRY_COMPARE(pathLabel->text(), mFixture.path("b"));
}

void FolderTreeFollowsNavigationTest::openingAFolderSelectsItInTheTree() {
    QVERIFY2(mFixture.navigate("a/a1"), "Opening a/a1 should succeed.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path("a")), "The tree should list folder a.");

    QTRY_COMPARE(mFixture.currentTreePath(), mFixture.path("a/a1"));
    QVERIFY2(mFixture.tree()->selectionModel()->isSelected(mFixture.index("a/a1")),
             "The opened folder's row should be selected.");
    QTRY_VERIFY2(mFixture.isRowVisible("a/a1"), "The opened folder's row should be shown.");
}

TG_BEHAVIOR_TEST_MAIN(FolderTreeFollowsNavigationTest)

#include "test_folder_tree_follows_navigation.moc"
