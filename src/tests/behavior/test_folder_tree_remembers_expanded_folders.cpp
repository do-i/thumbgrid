// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Expanded tree folders are saved and come back in a new folder view once listed; missing ones and Collapse all are respected.

#include "support/tree_test_fixture.h"

#include <QSignalSpy>

#include "constants.h"

class FolderTreeRemembersExpandedTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void expandedFoldersComeBackOnceListed();
    void aSavedFolderThatIsGoneIsIgnored();
    void theSavedListIsCapped();
    void collapseAllIsNotUndoneByALateListing();

private:
    // a second folder view, as the next session builds it from the stored settings
    static std::unique_ptr<FolderView> openSession() {
        auto view = std::make_unique<FolderView>();
        view->resize(1200, 800);
        view->show();
        return view;
    }
    static TreeViewCustom *treeOf(FolderView *view) { return view->findChild<TreeViewCustom *>("dirTreeView"); }
    // the row of home/rel among rows already listed; never makes the model look at the disk
    static QModelIndex listedRow(FolderView *view, const QString &rel) {
        auto *model = treeOf(view)->model();
        QModelIndex index = treeOf(view)->rootIndex();
        for(const QString &name : rel.split('/')) {
            QModelIndex next;
            for(int row = 0; row < model->rowCount(index) && !next.isValid(); ++row) {
                if(model->index(row, 0, index).data().toString() == name)
                    next = model->index(row, 0, index);
            }
            if(!next.isValid())
                return {};
            index = next;
        }
        return index;
    }
    static bool expandedRow(FolderView *view, const QString &rel) {
        const QModelIndex index = listedRow(view, rel);
        return index.isValid() && treeOf(view)->isExpanded(index);
    }
    tgtest::TreeFixture mFixture;
};

void FolderTreeRemembersExpandedTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY(QDir(mFixture.path()).mkpath("a/a1/deep"));
    QVERIFY2(mFixture.addFolders("many", 150), "The many folder's children should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path()), "The tree should list the home folder.");
}

void FolderTreeRemembersExpandedTest::expandedFoldersComeBackOnceListed() {
    TreeViewCustom *tree = mFixture.tree();
    tree->expand(mFixture.index("a"));
    QVERIFY(mFixture.waitForTreeLoaded(mFixture.path("a")));
    tree->expand(mFixture.index("a/a1"));
    tree->expand(mFixture.index("c"));
    mFixture.folderView()->saveTreeState();
    const QStringList saved = settings->treeExpandedPaths();
    QCOMPARE(saved, QStringList({mFixture.path("a"), mFixture.path("c"), mFixture.path("a/a1")}));

    auto view = openSession();
    view->setDirectoryPath(mFixture.path("b"));
    QTRY_VERIFY2(expandedRow(view.get(), "a/a1"), "a/a1 should be expanded again once a is listed.");
    QTRY_VERIFY(listedRow(view.get(), "a/a1/deep").isValid());
    QVERIFY(expandedRow(view.get(), "a"));
    QVERIFY(expandedRow(view.get(), "c"));
    QVERIFY2(!expandedRow(view.get(), "b"), "A folder that was not expanded stays collapsed.");
    QVERIFY2(!expandedRow(view.get(), "a/a1/deep"), "Nothing below the saved folders is expanded.");
    // restoring leaves the current folder current and in view
    TreeViewCustom *restored = treeOf(view.get());
    QCOMPARE(restored->currentIndex(), listedRow(view.get(), "b"));
    QVERIFY(restored->visualRect(restored->currentIndex()).intersects(restored->viewport()->rect()));
}

void FolderTreeRemembersExpandedTest::aSavedFolderThatIsGoneIsIgnored() {
    settings->setTreeExpandedPaths({mFixture.path("gone"), mFixture.path("gone/deeper"), mFixture.path("b")});
    auto view = openSession();
    QTRY_VERIFY(listedRow(view.get(), "b").isValid());
    QTRY_VERIFY(expandedRow(view.get(), "b"));
    QVERIFY(!QDir(mFixture.path("gone")).exists());
    view->saveTreeState();
    // home was listed without "gone", so it is dropped; "gone/deeper" waits for a parent that never lists
    const QStringList saved = settings->treeExpandedPaths();
    QVERIFY2(saved.contains(mFixture.path("b")), qPrintable(saved.join(", ")));
    QVERIFY2(!saved.contains(mFixture.path("gone")), qPrintable(saved.join(", ")));
}

void FolderTreeRemembersExpandedTest::theSavedListIsCapped() {
    settings->setTreeExpandedPaths({});
    auto view = openSession();
    TreeViewCustom *tree = treeOf(view.get());
    QTRY_VERIFY(listedRow(view.get(), "many").isValid());
    tree->expand(listedRow(view.get(), "many"));
    QTRY_COMPARE(tree->model()->rowCount(listedRow(view.get(), "many")), 150);
    for(int row = 0; row < 150; ++row)
        tree->expand(tree->model()->index(row, 0, listedRow(view.get(), "many")));
    tree->expand(listedRow(view.get(), "c"));
    view->saveTreeState();
    const QStringList saved = settings->treeExpandedPaths();
    QCOMPARE(saved.size(), tg::limit::kTreeExpandedMax);
    // shallowest first: the folders right under home are kept, the deepest are cut
    QCOMPARE(saved.at(0), mFixture.path("c"));
    QCOMPARE(saved.at(1), mFixture.path("many"));
    QVERIFY(saved.contains(mFixture.path("many/f000")));
    QVERIFY(!saved.contains(mFixture.path("many/f149")));
}

void FolderTreeRemembersExpandedTest::collapseAllIsNotUndoneByALateListing() {
    settings->setTreeExpandedPaths({mFixture.path("a"), mFixture.path("a/a1"), mFixture.path("c")});
    auto view = openSession();
    auto *model = qobject_cast<QFileSystemModel *>(treeOf(view.get())->model());
    QSignalSpy loaded(model, &QFileSystemModel::directoryLoaded);
    // before any listing reaches the view: the listings are queued to this thread
    QVERIFY(QMetaObject::invokeMethod(view.get(), "collapseTree"));
    QTRY_VERIFY(listedRow(view.get(), "c").isValid());
    QTRY_VERIFY(loaded.count() > 0);
    emit model->directoryLoaded(mFixture.path());
    QCoreApplication::processEvents();
    QVERIFY2(!expandedRow(view.get(), "a") && !expandedRow(view.get(), "c"), "Collapse all should stay in effect.");
    view->saveTreeState();
    QVERIFY2(settings->treeExpandedPaths().isEmpty(), "Nothing is expanded, so nothing is saved.");
}

TG_BEHAVIOR_TEST_MAIN(FolderTreeRemembersExpandedTest)

#include "test_folder_tree_remembers_expanded_folders.moc"
