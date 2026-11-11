// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Files dropped on a tree folder or a bookmark are moved, or copied with Ctrl held, after the usual prompt.

#include "support/tree_test_fixture.h"

#include <QDropEvent>
#include <QSignalSpy>
#include <QTimer>

#include "gui/folderview/bookmarksitem.h"

class FolderTreeDropsTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void aPlainDropMovesAndCtrlCopies_data();
    void aPlainDropMovesAndCtrlCopies();
    void aDropOnABookmarkFollowsCtrlToo();
    void aFolderDroppedIntoItselfIsRefusedWithoutAPrompt();
    void emptySpaceAndForeignDataAreNotAccepted();

private:
    // drags mime over target (a viewport or a bookmark) and drops it; true when the drop was accepted
    bool dragAndDrop(QWidget *target, QPoint pos, const QMimeData &mime, Qt::KeyboardModifiers mods,
                     Qt::DropAction expected) {
        const Qt::DropActions offered = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
        QDragEnterEvent enter(QPointF(pos), offered, &mime, Qt::LeftButton, mods);
        QApplication::sendEvent(target, &enter);
        QDragMoveEvent move(QPointF(pos), offered, &mime, Qt::LeftButton, mods);
        QApplication::sendEvent(target, &move);
        QDropEvent drop(QPointF(pos), offered, &mime, Qt::LeftButton, mods);
        QApplication::sendEvent(target, &drop);
        if(enter.isAccepted() && move.isAccepted() && drop.isAccepted()) {
            const bool cursorsMatch = move.dropAction() == expected && drop.dropAction() == expected;
            return cursorsMatch;
        }
        return false;
    }
    static QMimeData *urls(const QStringList &paths) {
        auto *mime = new QMimeData;
        QList<QUrl> list;
        for(const QString &path : paths)
            list << QUrl::fromLocalFile(path);
        mime->setUrls(list);
        return mime;
    }
    QPoint rowCenter(const QString &rel) const { return mFixture.tree()->visualRect(mFixture.index(rel)).center(); }
    // declines the confirmation, so nothing happens on disk; true once a prompt was seen
    void declinePrompts() {
        mPrompted = false;
        mPoll.start();
    }
    tgtest::TreeFixture mFixture;
    QTimer mPoll;
    bool mPrompted = false;
};

void FolderTreeDropsTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path()), "The tree should list the home folder.");
    QVERIFY(mFixture.isRowVisible("b"));
    mPoll.setInterval(20);
    connect(&mPoll, &QTimer::timeout, this, [this] {
        auto *box = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if(!box)
            return;
        mPrompted = true;
        mPoll.stop();
        box->reject();
    });
}

void FolderTreeDropsTest::init() {
    mPoll.stop();
    mPrompted = false;
}

void FolderTreeDropsTest::aPlainDropMovesAndCtrlCopies_data() {
    QTest::addColumn<Qt::KeyboardModifiers>("mods");
    QTest::addColumn<bool>("copy");
    QTest::newRow("plain") << Qt::KeyboardModifiers(Qt::NoModifier) << false;
    QTest::newRow("ctrl") << Qt::KeyboardModifiers(Qt::ControlModifier) << true;
    QTest::newRow("ctrl+shift") << Qt::KeyboardModifiers(Qt::ControlModifier | Qt::ShiftModifier) << true;
}

void FolderTreeDropsTest::aPlainDropMovesAndCtrlCopies() {
    QFETCH(Qt::KeyboardModifiers, mods);
    QFETCH(bool, copy);
    QSignalSpy copies(mFixture.folderView(), &FolderView::copyUrlsRequested);
    QSignalSpy moves(mFixture.folderView(), &FolderView::moveUrlsRequested);
    std::unique_ptr<QMimeData> mime(urls({mFixture.path("a/one.png")}));

    declinePrompts();
    QVERIFY2(dragAndDrop(mFixture.tree()->viewport(), rowCenter("b"), *mime, mods,
                         copy ? Qt::CopyAction : Qt::MoveAction), "The drop should be accepted with the matching action.");

    QSignalSpy &asked = copy ? copies : moves;
    QCOMPARE(asked.count(), 1);
    QCOMPARE((copy ? moves : copies).count(), 0);
    QCOMPARE(asked.at(0).at(0).toStringList(), QStringList({mFixture.path("a/one.png")}));
    QCOMPARE(asked.at(0).at(1).toString(), mFixture.path("b"));
    QVERIFY2(mPrompted, "The usual confirmation should be asked.");
    QVERIFY2(QFileInfo::exists(mFixture.path("a/one.png")) && !QFileInfo::exists(mFixture.path("b/one.png")),
             "Declining leaves the files where they were.");
}

void FolderTreeDropsTest::aDropOnABookmarkFollowsCtrlToo() {
    BookmarksItem *home = nullptr;
    for(auto *item : mFixture.folderView()->findChildren<BookmarksItem *>()) {
        if(item->path() == mFixture.path())
            home = item;
    }
    QVERIFY2(home, "The home bookmark should exist.");
    QSignalSpy copies(mFixture.folderView(), &FolderView::copyUrlsRequested);
    QSignalSpy moves(mFixture.folderView(), &FolderView::moveUrlsRequested);
    std::unique_ptr<QMimeData> mime(urls({mFixture.path("a/one.png")}));

    declinePrompts();
    QVERIFY(dragAndDrop(home, QPoint(5, 5), *mime, Qt::ControlModifier, Qt::CopyAction));
    QTRY_VERIFY(!mPoll.isActive());
    declinePrompts();
    QVERIFY(dragAndDrop(home, QPoint(5, 5), *mime, Qt::NoModifier, Qt::MoveAction));

    QCOMPARE(copies.count(), 1);
    QCOMPARE(moves.count(), 1);
    QCOMPARE(copies.at(0).at(1).toString(), mFixture.path());
    QCOMPARE(moves.at(0).at(1).toString(), mFixture.path());
}

void FolderTreeDropsTest::aFolderDroppedIntoItselfIsRefusedWithoutAPrompt() {
    mFixture.tree()->expand(mFixture.index("a"));
    QVERIFY(mFixture.waitForTreeLoaded(mFixture.path("a")));
    QTRY_VERIFY(mFixture.isRowVisible("a/a1"));
    std::unique_ptr<QMimeData> mime(urls({mFixture.path("a")}));
    QSignalSpy copies(mFixture.folderView(), &FolderView::copyUrlsRequested);
    QSignalSpy moves(mFixture.folderView(), &FolderView::moveUrlsRequested);
    for(Qt::KeyboardModifiers mods : {Qt::KeyboardModifiers(Qt::NoModifier), Qt::KeyboardModifiers(Qt::ControlModifier)}) {
        for(const QString &rel : {QString("a"), QString("a/a1")}) {
            declinePrompts();
            QVERIFY(dragAndDrop(mFixture.tree()->viewport(), rowCenter(rel), *mime, mods,
                                mods ? Qt::CopyAction : Qt::MoveAction));
            QCoreApplication::processEvents();
            QVERIFY2(!mPrompted, qPrintable("No prompt for a drop into " + rel));
            mPoll.stop();
        }
    }
    QCOMPARE(copies.count(), 2);
    QCOMPARE(moves.count(), 2);
    QVERIFY(!QDir(mFixture.path("a/a")).exists());
    QVERIFY(!QDir(mFixture.path("a/a1/a")).exists());
}

void FolderTreeDropsTest::emptySpaceAndForeignDataAreNotAccepted() {
    QSignalSpy copies(mFixture.folderView(), &FolderView::copyUrlsRequested);
    QSignalSpy moves(mFixture.folderView(), &FolderView::moveUrlsRequested);
    QWidget *viewport = mFixture.tree()->viewport();
    std::unique_ptr<QMimeData> mime(urls({mFixture.path("a/one.png")}));
    // below the last row
    const QPoint empty(5, viewport->height() - 2);
    QVERIFY(!mFixture.tree()->indexAt(empty).isValid());
    QDragMoveEvent move(QPointF(empty), Qt::CopyAction | Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &move);
    QVERIFY2(!move.isAccepted(), "Empty space is no drop target.");
    QDropEvent drop(QPointF(empty), Qt::CopyAction | Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &drop);

    QMimeData text;
    text.setText("not a file");
    QDropEvent textDrop(QPointF(rowCenter("b")), Qt::CopyAction | Qt::MoveAction, &text, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &textDrop);
    QVERIFY(!textDrop.isAccepted());
    QCOMPARE(copies.count() + moves.count(), 0);
}

TG_BEHAVIOR_TEST_MAIN(FolderTreeDropsTest)

#include "test_folder_tree_drops_move_or_copy_with_ctrl.moc"
