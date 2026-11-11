// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// focusFolderTree (Ctrl+Shift+E) moves the keyboard to the tree on the current folder and back; F2, Delete and Esc there.

#include "support/tree_test_fixture.h"

#include <QLineEdit>
#include <QSignalSpy>
#include <QTimer>

#include "components/actionmanager/actionmanager.h"
#include "gui/folderview/foldergridview.h"
#include "utils/actions.h"

class FolderTreeFocusTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void theActionMovesFocusToTheTreeAndBack();
    void theDefaultKeyAndEscapeWork();
    void aHiddenTreeIsShownFirst();
    void f2RenamesTheFocusedRow();
    void deleteAsksToTrashTheFocusedRowButNotARootRow();
    void tabIntoTheFolderViewIsNotSentBackToTheGrid();

private:
    FolderGridView *grid() const { return mFixture.folderView()->findChild<FolderGridView *>(); }
    QLineEdit *editor() const { return mFixture.tree()->viewport()->findChild<QLineEdit *>(); }
    bool treeFocused() const { return QApplication::focusWidget() == mFixture.tree(); }
    bool gridFocused() const { return QApplication::focusWidget() == grid(); }
    // a closed prompt can leave no active window; take it back first
    void focusTree() {
        tgtest::mainWindow()->activateWindow();
        QTRY_VERIFY(QApplication::activeWindow() == tgtest::mainWindow());
        if(!treeFocused())
            actionManager->invokeAction("focusFolderTree");
        QTRY_VERIFY(treeFocused());
    }
    tgtest::TreeFixture mFixture;
};

void FolderTreeFocusTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start("a/a1"), "The app should open home/a/a1.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path("a")), "The tree should list folder a.");
    QVERIFY(appActions->getList().contains("focusFolderTree"));
    tgtest::mainWindow()->activateWindow();
    QTRY_VERIFY(gridFocused());
}

void FolderTreeFocusTest::theActionMovesFocusToTheTreeAndBack() {
    TreeViewCustom *tree = mFixture.tree();
    QSignalSpy opened(mFixture.folderView(), &FolderView::directorySelected);
    actionManager->invokeAction("focusFolderTree");
    QTRY_VERIFY2(treeFocused(), "The tree should take the keyboard.");
    QCOMPARE(mFixture.currentTreePath(), mFixture.path("a/a1"));

    // arrows move the tree's own row without opening anything
    QTest::keyClick(tree, Qt::Key_Down);
    QCOMPARE(mFixture.currentTreePath(), mFixture.path("a/a2"));
    QCOMPARE(opened.count(), 0);

    actionManager->invokeAction("focusFolderTree");
    QTRY_VERIFY2(gridFocused(), "Invoking it again should return the keyboard to the grid.");

    actionManager->invokeAction("focusFolderTree");
    QTRY_VERIFY(treeFocused());
    QCOMPARE(mFixture.currentTreePath(), mFixture.path("a/a1"));
}

void FolderTreeFocusTest::theDefaultKeyAndEscapeWork() {
    actionManager->invokeAction("focusFolderTree");
    QTRY_VERIFY(gridFocused());
    QTest::keyClick(grid(), Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY2(treeFocused(), "Ctrl+Shift+E should be bound by the default preset.");
    QTest::keyClick(mFixture.tree(), Qt::Key_Escape);
    QTRY_VERIFY2(gridFocused(), "Esc in the tree should return to the grid.");
    QVERIFY2(tgtest::mainWindow()->isVisible(), "Esc in the tree must not reach the app's exit binding.");
}

void FolderTreeFocusTest::aHiddenTreeIsShownFirst() {
    QVERIFY(QMetaObject::invokeMethod(mFixture.folderView(), "toggleFilesystemView"));
    QVERIFY(!mFixture.tree()->isVisible());
    actionManager->invokeAction("focusFolderTree");
    QTRY_VERIFY(treeFocused());
    QVERIFY(mFixture.tree()->isVisible());
    QVERIFY2(settings->placesPanelTreeExpanded(), "Shown the same way as its own toggle, so it stays shown.");
}

void FolderTreeFocusTest::f2RenamesTheFocusedRow() {
    focusTree();
    mFixture.tree()->setCurrentIndex(mFixture.index("b"));
    QTest::keyClick(mFixture.tree(), Qt::Key_F2);
    QVERIFY2(editor() && editor()->isVisible(), "F2 should open the inline editor.");
    QCOMPARE(editor()->text(), QString("b"));
    QTest::keyClick(editor(), Qt::Key_Escape);
    QVERIFY(!editor()->isVisible());
    QVERIFY(QDir(mFixture.path("b")).exists());
}

void FolderTreeFocusTest::deleteAsksToTrashTheFocusedRowButNotARootRow() {
    QSignalSpy requests(mFixture.folderView(), &FolderView::folderOperationRequested);
    bool prompted = false;
    QTimer poll;
    poll.setInterval(20);
    connect(&poll, &QTimer::timeout, this, [&] {
        if(auto *box = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
            prompted = true;
            box->reject();
        }
    });
    poll.start();
    settings->setConfirmTrash(true);

    focusTree();
    mFixture.tree()->setCurrentIndex(mFixture.index("b"));
    QTest::keyClick(mFixture.tree(), Qt::Key_Delete);
    QCOMPARE(requests.count(), 1);
    QCOMPARE(requests.at(0).at(0).value<TreeFolderOp>(), TreeFolderOp::Trash);
    QCOMPARE(requests.at(0).at(1).toString(), mFixture.path("b"));
    QTRY_VERIFY2(prompted, "Move to trash asks first, as from the menu.");
    QVERIFY(QDir(mFixture.path("b")).exists());

    // rooted at "/", home and the folders above it are rows: F2 and Delete do nothing there
    mFixture.folderView()->setDirectoryPath("/");
    for(const QString &path : {mFixture.path(), QFileInfo(mFixture.path()).path()}) {
        prompted = false;
        focusTree();
        mFixture.tree()->setCurrentIndex(mFixture.model()->index(path));
        QTest::keyClick(mFixture.tree(), Qt::Key_Delete);
        QTest::keyClick(mFixture.tree(), Qt::Key_F2);
        QCoreApplication::processEvents();
        QVERIFY2(!prompted, qPrintable(path));
        QVERIFY2(!editor() || !editor()->isVisible(), qPrintable(path));
    }
    QCOMPARE(requests.count(), 1);
    mFixture.folderView()->setDirectoryPath(mFixture.path("a/a1"));
}

void FolderTreeFocusTest::tabIntoTheFolderViewIsNotSentBackToTheGrid() {
    focusTree();
    QFocusEvent other(QEvent::FocusIn, Qt::OtherFocusReason);
    QApplication::sendEvent(mFixture.folderView(), &other);
    QVERIFY2(gridFocused(), "An ordinary focus-in lands on the grid.");
    QFocusEvent tab(QEvent::FocusIn, Qt::TabFocusReason);
    QApplication::sendEvent(mFixture.folderView(), &tab);
    QVERIFY2(!gridFocused(), "Tab moves on past the folder view instead of back to the grid.");
}

TG_BEHAVIOR_TEST_MAIN(FolderTreeFocusTest)

#include "test_folder_tree_focus_action_and_keys.moc"
