// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The bookmarks right-click menu, and bookmark labels that can be renamed.

#include "support/tree_test_fixture.h"

#include "gui/folderview/bookmarkcontextmenu.h"
#include "gui/folderview/bookmarksitem.h"
#include "gui/folderview/bookmarkswidget.h"
#include "utils/newwindow.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QLabel>
#include <QSignalSpy>

class BookmarksMenuAndLabelsTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void rightClickingABookmarkOpensItsMenuWithoutNavigating();
    void openNavigatesToTheBookmark();
    void removeTakesTheBookmarkOutOfThePanelAndTheStoredList();
    void copyPathPutsTheNativePathOnTheClipboard();
    void openInNewWindowLaunchesTheBookmarkedFolder();
    void rightClickingTheHeaderOffersAddFolderAndBookmarkThisFolder();

private:
    BookmarksWidget *panel() const { return mFixture.folderView()->findChild<BookmarksWidget *>(); }
    BookmarkContextMenu *menu() const { return mFixture.folderView()->findChild<BookmarkContextMenu *>(); }
    BookmarksItem *row(const QString &path) const {
        for(int i = 0; i < panel()->QWidget::layout()->count(); ++i) {
            auto *item = qobject_cast<BookmarksItem *>(panel()->QWidget::layout()->itemAt(i)->widget());
            if(item && item->path() == path)
                return item;
        }
        return nullptr;
    }
    QString pathLabelText() const { return mFixture.folderView()->findChild<QLabel *>("pathLabel")->text(); }
    void storeAndNotify(const QStringList &paths) {
        settings->setBookmarks(paths);
        settings->sendChangeNotification();
    }
    void rightClick(QWidget *target) {
        QContextMenuEvent event(QContextMenuEvent::Mouse, target->rect().center(),
                                target->mapToGlobal(target->rect().center()));
        QApplication::sendEvent(target, &event);
    }
    // right-clicks the bookmark of rel and triggers the menu item with that object name
    void triggerOn(const QString &rel, const QString &itemName) {
        BookmarksItem *item = row(mFixture.path(rel));
        QVERIFY2(item, "The bookmark should be listed.");
        rightClick(item);
        QVERIFY2(menu() && menu()->isVisible(), "The menu should open.");
        auto *entry = menu()->findChild<ContextMenuItem *>(itemName);
        QVERIFY2(entry && entry->isVisible() && entry->isEnabled(), qPrintable(itemName));
        entry->trigger();
    }
    tgtest::TreeFixture mFixture;
};

void BookmarksMenuAndLabelsTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY(panel() != nullptr);
    QVERIFY2(menu() == nullptr, "The menu should not exist before the first right-click.");
}

void BookmarksMenuAndLabelsTest::init() {
    if(menu())
        menu()->hide();
    storeAndNotify({mFixture.path(), mFixture.path("a"), mFixture.path("b")});
}

void BookmarksMenuAndLabelsTest::rightClickingABookmarkOpensItsMenuWithoutNavigating() {
    const QString before = pathLabelText();
    QSignalSpy selected(mFixture.folderView(), &FolderView::directorySelected);

    rightClick(row(mFixture.path("b")));

    QVERIFY(menu() != nullptr && menu()->isVisible());
    QCOMPARE(menu()->targetPath(), mFixture.path("b"));
    QVERIFY(menu()->findChild<ContextMenuItem *>("menuRemove")->isVisible());
    QVERIFY(!menu()->findChild<ContextMenuItem *>("menuAddFolder")->isVisible());
    QCOMPARE(selected.count(), 0);
    QCOMPARE(pathLabelText(), before);
}

void BookmarksMenuAndLabelsTest::openNavigatesToTheBookmark() {
    QSignalSpy selected(mFixture.folderView(), &FolderView::directorySelected);
    triggerOn("a", "menuOpen");
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.at(0).at(0).toString(), mFixture.path("a"));
}

void BookmarksMenuAndLabelsTest::removeTakesTheBookmarkOutOfThePanelAndTheStoredList() {
    triggerOn("a", "menuRemove");
    QVERIFY(!menu()->isVisible());
    QCOMPARE(settings->bookmarks(), (QStringList{mFixture.path(), mFixture.path("b")}));
    QCOMPARE(row(mFixture.path("a")), nullptr);
}

void BookmarksMenuAndLabelsTest::copyPathPutsTheNativePathOnTheClipboard() {
    QApplication::clipboard()->setText("untouched");
    triggerOn("b", "menuCopyPath");
    QCOMPARE(QApplication::clipboard()->text(), QDir::toNativeSeparators(mFixture.path("b")));
}

void BookmarksMenuAndLabelsTest::openInNewWindowLaunchesTheBookmarkedFolder() {
    QStringList launched;
    NewWindow::setLauncher([&](const QString &path) { launched << path; });
    triggerOn("b", "menuNewWindow");
    NewWindow::setLauncher({});
    QCOMPARE(launched, QStringList{mFixture.path("b")});
}

void BookmarksMenuAndLabelsTest::rightClickingTheHeaderOffersAddFolderAndBookmarkThisFolder() {
    QVERIFY(mFixture.navigate("c"));
    auto *button = mFixture.folderView()->findChild<QWidget *>("newBookmarkButton");
    QTRY_VERIFY(button->isEnabled());
    auto *header = mFixture.folderView()->findChild<QWidget *>("bookmarksLabel");
    const bool shown = panel()->isVisible();

    rightClick(header);

    QCOMPARE(panel()->isVisible(), shown);
    QVERIFY(menu()->isVisible());
    QVERIFY(menu()->findChild<ContextMenuItem *>("menuAddFolder")->isVisible());
    QVERIFY(!menu()->findChild<ContextMenuItem *>("menuRemove")->isVisible());
    auto *here = menu()->findChild<ContextMenuItem *>("menuBookmarkHere");
    QVERIFY(here->isEnabled());
    here->trigger();
    QVERIFY(settings->bookmarks().contains(mFixture.path("c")));

    rightClick(header);
    QVERIFY2(!menu()->findChild<ContextMenuItem *>("menuBookmarkHere")->isEnabled(),
             "A bookmarked folder cannot be added again.");
}

TG_BEHAVIOR_TEST_MAIN(BookmarksMenuAndLabelsTest)

#include "test_bookmarks_menu_and_labels.moc"
