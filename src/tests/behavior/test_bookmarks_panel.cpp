// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The bookmarks panel: labels, following the stored list, the current-folder button, tooltips.

#include "support/tree_test_fixture.h"

#include "gui/folderview/bookmarksitem.h"
#include "gui/folderview/bookmarkswidget.h"

class BookmarksPanelTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void theRootBookmarkHasANameAndTrailingSlashesAreIgnored();
    void bookmarksRemovedFromTheStoredListLeaveThePanel();
    void theRowOrderFollowsTheStoredListWithoutRebuildingRows();
    void everyRowShowsItsFullPathAsTooltip();
    void theAddButtonBookmarksTheCurrentFolderOnce();

private:
    BookmarksWidget *panel() const { return mFixture.folderView()->findChild<BookmarksWidget *>(); }
    QList<BookmarksItem *> rows() const {
        QList<BookmarksItem *> list;
        for(int i = 0; i < panel()->QWidget::layout()->count(); ++i)
            list << qobject_cast<BookmarksItem *>(panel()->QWidget::layout()->itemAt(i)->widget());
        return list;
    }
    QStringList rowPaths() const {
        QStringList list;
        for(auto *row : rows())
            list << row->path();
        return list;
    }
    void storeAndNotify(const QStringList &paths) {
        settings->setBookmarks(paths);
        settings->sendChangeNotification();
    }
    tgtest::TreeFixture mFixture;
};

void BookmarksPanelTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY(panel() != nullptr);
}

void BookmarksPanelTest::init() {
    storeAndNotify({mFixture.path()});
}

void BookmarksPanelTest::theRootBookmarkHasANameAndTrailingSlashesAreIgnored() {
    storeAndNotify({"/", "/photos/trip/", mFixture.path()});
    const auto list = rows();
    QCOMPARE(list.count(), 3);
    QCOMPARE(list.at(0)->label(), QStringLiteral("/"));
    QCOMPARE(list.at(1)->label(), QStringLiteral("trip"));
    QCOMPARE(list.at(2)->label(), QFileInfo(mFixture.path()).fileName());
}

void BookmarksPanelTest::bookmarksRemovedFromTheStoredListLeaveThePanel() {
    storeAndNotify({mFixture.path(), mFixture.path("a"), mFixture.path("b")});
    QCOMPARE(rowPaths().count(), 3);

    storeAndNotify({mFixture.path(), mFixture.path("b")});

    QCOMPARE(rowPaths(), (QStringList{mFixture.path(), mFixture.path("b")}));
    QCOMPARE(settings->bookmarks(), (QStringList{mFixture.path(), mFixture.path("b")}));
}

void BookmarksPanelTest::theRowOrderFollowsTheStoredListWithoutRebuildingRows() {
    storeAndNotify({mFixture.path("a"), mFixture.path("b"), mFixture.path("c")});
    const auto before = rows();
    QPointer<BookmarksItem> keptA = before.at(0), keptC = before.at(2);

    storeAndNotify({mFixture.path("c"), mFixture.path("a"), mFixture.path("b")});

    QCOMPARE(rowPaths(), (QStringList{mFixture.path("c"), mFixture.path("a"), mFixture.path("b")}));
    QCOMPARE(rows().at(0), keptC.data());
    QCOMPARE(rows().at(1), keptA.data());
    QCOMPARE(rows().at(1), before.at(0));
    QCOMPARE(settings->bookmarks(), (QStringList{mFixture.path("c"), mFixture.path("a"), mFixture.path("b")}));
}

void BookmarksPanelTest::everyRowShowsItsFullPathAsTooltip() {
    storeAndNotify({mFixture.path("a"), "/"});
    for(auto *row : rows())
        QCOMPARE(row->toolTip(), QDir::toNativeSeparators(row->path()));
}

void BookmarksPanelTest::theAddButtonBookmarksTheCurrentFolderOnce() {
    auto *button = mFixture.folderView()->findChild<QWidget *>("newBookmarkButton");
    QVERIFY(button != nullptr);
    QVERIFY(mFixture.navigate("a"));
    QTRY_VERIFY(button->isEnabled());
    storeAndNotify({mFixture.path("b")});
    QTRY_VERIFY(button->isEnabled());

    QTest::mouseClick(button, Qt::LeftButton);

    QCOMPARE(settings->bookmarks(), (QStringList{mFixture.path("b"), mFixture.path("a")}));
    QCOMPARE(rowPaths(), settings->bookmarks());
    QVERIFY2(!button->isEnabled(), "A bookmarked folder cannot be added again.");
    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(settings->bookmarks().count(), 2);
    QCOMPARE(button->toolTip(), QObject::tr("This folder is already bookmarked"));
}

TG_BEHAVIOR_TEST_MAIN(BookmarksPanelTest)

#include "test_bookmarks_panel.moc"
