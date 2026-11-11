// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Tree menu: New folder and Rename use an inline editor, removal asks first; home and the root stay put.

#include "support/tree_test_fixture.h"

#include <QContextMenuEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>

class FolderTreeOperationsTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void newFolderIsCreatedListedAndSelected();
    void renameChangesTheFolderOnDiskAndInTheTree();
    void rejectedNamesLeaveTheFolderUntouched();
    void escapeAndLosingFocusCancelTheEditor();
    void renamingTheCurrentFolderMovesTheAppToTheNewPath();
    void removalAsksForConfirmationForTheRightFolder();
    void homeAndTheRootCannotBeRenamedOrRemoved();

private:
    TreeContextMenu *menu() const { return mFixture.folderView()->findChild<TreeContextMenu *>(); }
    QLineEdit *editor() const { return mFixture.tree()->viewport()->findChild<QLineEdit *>(); }
    QString pathLabelText() const { return mFixture.folderView()->findChild<QLabel *>("pathLabel")->text(); }
    bool listed(const QString &rel) const {
        const QModelIndex parent = mFixture.index(QFileInfo(rel).path() == "." ? QString() : QFileInfo(rel).path());
        for(int row = 0; row < mFixture.model()->rowCount(parent); ++row)
            if(mFixture.model()->index(row, 0, parent).data().toString() == QFileInfo(rel).fileName())
                return true;
        return false;
    }
    // makes a folder on disk and waits until the tree lists it
    void makeFolder(const QString &rel) {
        QVERIFY(QDir(tgtest::TreeFixture::homePath()).mkpath(rel));
        QTRY_VERIFY2(listed(rel) && mFixture.isRowVisible(rel), qPrintable(rel));
    }
    // right-click on the row, then the menu item
    void openMenuOn(const QString &rel) {
        const QPoint pos = mFixture.tree()->visualRect(mFixture.index(rel)).center();
        QContextMenuEvent event(QContextMenuEvent::Mouse, pos, mFixture.tree()->viewport()->mapToGlobal(pos));
        QApplication::sendEvent(mFixture.tree()->viewport(), &event);
        QVERIFY2(menu() && menu()->isVisible(), "The menu should open.");
    }
    void choose(const QString &itemName) {
        auto *entry = menu()->findChild<ContextMenuItem *>(itemName);
        QVERIFY2(entry && entry->isEnabled(), qPrintable(itemName));
        entry->trigger();
    }
    // types the name into the inline editor and presses Enter
    void commitEditor(const QString &text) {
        QVERIFY2(editor() && editor()->isVisible(), "The inline editor should be shown.");
        editor()->setText(text);
        QTest::keyClick(editor(), Qt::Key_Return);
        QVERIFY(!editor()->isVisible());
    }
    // a refused name: the editor comes back holding it, and Esc leaves it
    void commitRefused(const QString &text) {
        QVERIFY2(editor() && editor()->isVisible(), "The inline editor should be shown.");
        editor()->setText(text);
        QTest::keyClick(editor(), Qt::Key_Return);
        QVERIFY2(editor()->isVisible(), qPrintable(text));
        QCOMPARE(editor()->text(), text);
        QTest::keyClick(editor(), Qt::Key_Escape);
        QVERIFY(!editor()->isVisible());
    }
    tgtest::TreeFixture mFixture;
};

void FolderTreeOperationsTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path()), "The tree should list the home folder.");
    settings->setConfirmTrash(true);
    settings->setConfirmDelete(true);
}

void FolderTreeOperationsTest::newFolderIsCreatedListedAndSelected() {
    openMenuOn("b");
    choose("menuNewFolder");
    QVERIFY2(editor() && editor()->isVisible(), "The inline editor should be shown.");
    QCOMPARE(editor()->text(), tr("New folder"));
    QVERIFY2(!QDir(mFixture.path("b/New folder")).exists(), "Nothing is created before Enter.");

    commitEditor("fresh");

    QVERIFY2(QDir(mFixture.path("b/fresh")).exists(), "The folder should be created on disk.");
    QTRY_VERIFY2(listed("b/fresh"), "The tree should list the new folder.");
    QTRY_COMPARE(mFixture.currentTreePath(), mFixture.path("b/fresh"));
    QTRY_VERIFY(mFixture.isRowVisible("b/fresh"));
}

void FolderTreeOperationsTest::renameChangesTheFolderOnDiskAndInTheTree() {
    makeFolder("old1");
    openMenuOn("old1");
    choose("menuRename");
    QVERIFY(editor() && editor()->isVisible());
    QCOMPARE(editor()->text(), QString("old1"));

    commitEditor("new1");

    QVERIFY2(QDir(mFixture.path("new1")).exists() && !QDir(mFixture.path("old1")).exists(),
             "The folder on disk should carry the new name.");
    QTRY_VERIFY2(listed("new1") && !listed("old1"), "The tree should show the new name.");
}

void FolderTreeOperationsTest::rejectedNamesLeaveTheFolderUntouched() {
    makeFolder("keep");
    const QString outside = QDir(mFixture.path()).absoluteFilePath("../x");
    QSignalSpy requests(mFixture.folderView(), &FolderView::folderOperationRequested);

    for(const QString &bad : {QString("../x"), QString("a/b"), QString("b")}) { // traversal, separator, existing sibling
        openMenuOn("keep");
        choose("menuRename");
        commitRefused(bad);
        QVERIFY2(QDir(mFixture.path("keep")).exists(), qPrintable(bad));
        QVERIFY2(!QFileInfo::exists(outside), qPrintable(bad));
        QVERIFY2(!QDir(mFixture.path("a/b")).exists(), qPrintable(bad));
    }
    QCOMPARE(requests.count(), 3);

    // an empty name asks for nothing
    openMenuOn("keep");
    choose("menuRename");
    commitEditor("   ");
    QCOMPARE(requests.count(), 3);

    // new folder: a separator and an existing name create nothing
    openMenuOn("keep");
    choose("menuNewFolder");
    commitRefused("../y");
    QVERIFY(!QFileInfo::exists(QDir(mFixture.path()).absoluteFilePath("../y")));
    openMenuOn("keep");
    choose("menuNewFolder");
    commitRefused("a1/..");
    openMenuOn("a");
    choose("menuNewFolder");
    commitRefused("a1");
    QVERIFY(QDir(mFixture.path("keep")).entryList(QDir::AllDirs | QDir::NoDotAndDotDot).isEmpty());
}

void FolderTreeOperationsTest::escapeAndLosingFocusCancelTheEditor() {
    QSignalSpy requests(mFixture.folderView(), &FolderView::folderOperationRequested);
    openMenuOn("keep");
    choose("menuRename");
    QVERIFY(editor()->isVisible());
    QTest::keyClick(editor(), Qt::Key_Escape);
    QVERIFY(!editor()->isVisible());

    openMenuOn("keep");
    choose("menuRename");
    editor()->setText("gone");
    QFocusEvent focusOut(QEvent::FocusOut, Qt::OtherFocusReason);
    QApplication::sendEvent(editor(), &focusOut);
    QVERIFY(!editor()->isVisible());
    QCOMPARE(requests.count(), 0);
    QVERIFY(QDir(mFixture.path("keep")).exists());
}

void FolderTreeOperationsTest::renamingTheCurrentFolderMovesTheAppToTheNewPath() {
    makeFolder("cur");
    QVERIFY(QDir(mFixture.path("cur")).mkdir("sub"));
    QVERIFY(mFixture.navigate("cur/sub"));
    QTRY_COMPARE(pathLabelText(), mFixture.path("cur/sub"));
    QTRY_VERIFY(mFixture.isRowVisible("cur"));

    openMenuOn("cur");
    choose("menuRename");
    commitEditor("cur2");

    QVERIFY(QDir(mFixture.path("cur2/sub")).exists());
    QTRY_COMPARE(pathLabelText(), mFixture.path("cur2/sub"));
}

void FolderTreeOperationsTest::removalAsksForConfirmationForTheRightFolder() {
    // QFile::moveToTrash refuses folders under the temp dir, so only the prompt is checked, then refused
    makeFolder("doomed");
    QSignalSpy requests(mFixture.folderView(), &FolderView::folderOperationRequested);
    for(const QString &itemName : {QString("menuTrash"), QString("menuDelete")}) {
        bool sawPrompt = false;
        QTimer poll;
        poll.setInterval(20);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            auto *box = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if(!box)
                return;
            sawPrompt = box->findChild<QPushButton *>("dangerButton") != nullptr;
            poll.stop();
            box->reject();
        });
        poll.start();
        openMenuOn("doomed");
        choose(itemName);
        QVERIFY2(sawPrompt, qPrintable(itemName));
        QVERIFY(QDir(mFixture.path("doomed")).exists());
    }
    QCOMPARE(requests.count(), 2);
    QCOMPARE(requests.at(0).at(0).value<TreeFolderOp>(), TreeFolderOp::Trash);
    QCOMPARE(requests.at(1).at(0).value<TreeFolderOp>(), TreeFolderOp::Delete);
    QCOMPARE(requests.at(0).at(1).toString(), mFixture.path("doomed"));
    QCOMPARE(requests.at(1).at(1).toString(), mFixture.path("doomed"));
}

void FolderTreeOperationsTest::homeAndTheRootCannotBeRenamedOrRemoved() {
    for(const QString &path : {mFixture.path(), QString("/")}) {
        const QModelIndex index = mFixture.model()->index(path);
        QVERIFY2(index.isValid(), qPrintable(path));
        emit mFixture.tree()->contextMenuRequestedAt(index, mFixture.tree()->mapToGlobal(QPoint(10, 10)));
        QVERIFY(menu()->isVisible());
        QCOMPARE(menu()->targetPath(), path);
        for(const char *name : {"menuRename", "menuTrash", "menuDelete"})
            QVERIFY2(!menu()->findChild<ContextMenuItem *>(name)->isEnabled(), name);
        QVERIFY(menu()->findChild<ContextMenuItem *>("menuNewFolder")->isEnabled());
        menu()->hide();
    }
    QSignalSpy requests(mFixture.folderView(), &FolderView::folderOperationRequested);
    emit mFixture.folderView()->folderOperationRequested(TreeFolderOp::Rename, mFixture.path(), "elsewhere");
    QVERIFY2(QDir(mFixture.path()).exists(), "Core refuses to rename home even when asked directly.");
}

TG_BEHAVIOR_TEST_MAIN(FolderTreeOperationsTest)

#include "test_folder_tree_creates_renames_and_removes_folders.moc"
