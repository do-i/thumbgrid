// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The bookmarks panel: labels, stored list, current-folder button, tooltips, drag to reorder.

#include "support/tree_test_fixture.h"

#include <QAbstractScrollArea>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QSignalSpy>
#include <QTimer>

#include "gui/folderview/bookmarkdrag.h"
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
    void aClickWithoutMovementStillNavigatesAndADragDoesNot();
    void droppingABookmarkBetweenRowsReordersThePanelAndTheStoredList();
    void aFileDropOnABookmarkStillRequestsAMoveOrACopy();
    void aBookmarkDropIsIgnoredByTheTreeAndTheGrid();

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
        settle();
    }
    // waits for the layout pass that gives the rows their place
    void settle() {
        panel()->QWidget::layout()->activate();
        QTRY_VERIFY2(panel()->height() >= rows().count() * 10, "The panel should be laid out.");
    }
    BookmarksItem *row(const QString &rel) const {
        for(auto *item : rows())
            if(item->path() == mFixture.path(rel))
                return item;
        return nullptr;
    }
    static QMimeData *bookmarkMime(const QString &path) {
        auto *mime = new QMimeData;
        mime->setData(QLatin1String(kBookmarkMime), path.toUtf8());
        return mime;
    }
    // enter, move and drop at pos on target; true when all three were accepted
    static bool dragTo(QWidget *target, QPoint pos, const QMimeData &mime, Qt::KeyboardModifiers mods = {}) {
        const Qt::DropActions offered = Qt::CopyAction | Qt::MoveAction;
        QDragEnterEvent enter(QPointF(pos), offered, &mime, Qt::LeftButton, mods);
        QApplication::sendEvent(target, &enter);
        QDragMoveEvent move(QPointF(pos), offered, &mime, Qt::LeftButton, mods);
        QApplication::sendEvent(target, &move);
        QDropEvent drop(QPointF(pos), offered, &mime, Qt::LeftButton, mods);
        QApplication::sendEvent(target, &drop);
        return enter.isAccepted() && move.isAccepted() && drop.isAccepted();
    }
    QPoint inRow(const QString &rel, bool upperHalf) const {
        const int h = row(rel)->height();
        return QPoint(5, upperHalf ? 1 : h - 2);
    }
    tgtest::TreeFixture mFixture;
};

void BookmarksPanelTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY(panel() != nullptr);
    settings->setPlacesPanel(true);
    settings->sendChangeNotification();
    QTRY_VERIFY2(panel()->isVisible(), "The places panel should be shown.");
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

void BookmarksPanelTest::aClickWithoutMovementStillNavigatesAndADragDoesNot() {
    storeAndNotify({mFixture.path("a"), mFixture.path("b")});
    QSignalSpy selected(mFixture.folderView(), &FolderView::directorySelected);
    int dragged = 0;
    QString draggedPath;
    panel()->setDragHook([&](const QMimeData *mime) {
        ++dragged;
        draggedPath = QString::fromUtf8(mime->data(QLatin1String(kBookmarkMime)));
        return true;
    });
    BookmarksItem *item = row("a");
    const QPoint start(8, 8);
    auto mouse = [&](QEvent::Type type, QPoint pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, QPointF(pos), QPointF(item->mapToGlobal(pos)), button, buttons, {});
        QApplication::sendEvent(item, &event);
    };

    mouse(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(QEvent::MouseMove, start + QPoint(1, 1), Qt::NoButton, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, start + QPoint(1, 1), Qt::LeftButton, Qt::NoButton);
    QCOMPARE(dragged, 0);
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.at(0).at(0).toString(), mFixture.path("a"));

    const QPoint far = start + QPoint(0, QApplication::startDragDistance() + 4);
    mouse(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(QEvent::MouseMove, far, Qt::NoButton, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, far, Qt::LeftButton, Qt::NoButton);
    panel()->setDragHook({});
    QCOMPARE(dragged, 1);
    QCOMPARE(draggedPath, mFixture.path("a"));
    QCOMPARE(selected.count(), 1);
}

void BookmarksPanelTest::droppingABookmarkBetweenRowsReordersThePanelAndTheStoredList() {
    storeAndNotify({mFixture.path("a"), mFixture.path("b"), mFixture.path("c")});
    std::unique_ptr<QMimeData> mime(bookmarkMime(mFixture.path("c")));

    // mid-drag: the insertion line shows above the first row
    QDragEnterEvent enter(QPointF(inRow("a", true)), Qt::MoveAction, mime.get(), Qt::LeftButton, {});
    QApplication::sendEvent(row("a"), &enter);
    QVERIFY(enter.isAccepted());
    QWidget *indicator = nullptr;
    for(auto *child : panel()->findChildren<QWidget *>())
        if(child->accessibleName() == "BookmarkDropIndicator")
            indicator = child;
    QVERIFY(indicator && !indicator->isHidden());
    QCOMPARE(indicator->geometry().top(), qMax(0, row("a")->geometry().top() - 1));
    QDragLeaveEvent leave;
    QApplication::sendEvent(row("a"), &leave);
    QVERIFY(indicator->isHidden());

    QVERIFY(dragTo(row("a"), inRow("a", true), *mime));
    const QStringList first{mFixture.path("c"), mFixture.path("a"), mFixture.path("b")};
    QCOMPARE(rowPaths(), first);
    QCOMPARE(settings->bookmarks(), first);
    QVERIFY(indicator->isHidden());

    // below the last row, on the panel itself: a goes to the end
    settle();
    std::unique_ptr<QMimeData> second(bookmarkMime(mFixture.path("c")));
    QVERIFY(dragTo(panel(), QPoint(5, panel()->height() - 1), *second));
    const QStringList last{mFixture.path("a"), mFixture.path("b"), mFixture.path("c")};
    QCOMPARE(rowPaths(), last);
    QCOMPARE(settings->bookmarks(), last);

    // dropping a row on itself changes nothing
    settle();
    QVERIFY(dragTo(row("b"), inRow("b", false), *std::unique_ptr<QMimeData>(bookmarkMime(mFixture.path("b")))));
    QCOMPARE(rowPaths(), last);
}

void BookmarksPanelTest::aFileDropOnABookmarkStillRequestsAMoveOrACopy() {
    storeAndNotify({mFixture.path("a"), mFixture.path("b")});
    QSignalSpy copies(mFixture.folderView(), &FolderView::copyUrlsRequested);
    QSignalSpy moves(mFixture.folderView(), &FolderView::moveUrlsRequested);
    QMimeData files;
    files.setUrls({QUrl::fromLocalFile(mFixture.path("a/one.png"))});
    // the app asks before it moves or copies; declining leaves the files alone
    QTimer decline;
    decline.setInterval(20);
    connect(&decline, &QTimer::timeout, this, [] {
        if(auto *box = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
            box->reject();
    });
    decline.start();

    QVERIFY(dragTo(row("b"), QPoint(5, 5), files));
    QVERIFY(dragTo(row("b"), QPoint(5, 5), files, Qt::ControlModifier));
    QVERIFY(QFileInfo::exists(mFixture.path("a/one.png")));

    QCOMPARE(moves.count(), 1);
    QCOMPARE(copies.count(), 1);
    QCOMPARE(moves.at(0).at(1).toString(), mFixture.path("b"));
    QCOMPARE(rowPaths(), (QStringList{mFixture.path("a"), mFixture.path("b")}));
}

void BookmarksPanelTest::aBookmarkDropIsIgnoredByTheTreeAndTheGrid() {
    std::unique_ptr<QMimeData> mime(bookmarkMime(mFixture.path("a")));
    QSignalSpy copies(mFixture.folderView(), &FolderView::copyUrlsRequested);
    QSignalSpy moves(mFixture.folderView(), &FolderView::moveUrlsRequested);
    QSignalSpy dropped(mFixture.folderView(), &FolderView::droppedInto);
    QWidget *treeView = mFixture.tree()->viewport();
    QWidget *gridView = mFixture.folderView()->findChild<QAbstractScrollArea *>("thumbnailGrid")->viewport();
    for(QWidget *target : {treeView, gridView}) {
        const QPoint pos = target == treeView ? mFixture.tree()->visualRect(mFixture.index("b")).center()
                                              : QPoint(20, 20);
        QVERIFY2(!dragTo(target, pos, *mime), "A bookmark drag is not a file drop.");
    }
    QCOMPARE(copies.count() + moves.count() + dropped.count(), 0);
}

TG_BEHAVIOR_TEST_MAIN(BookmarksPanelTest)

#include "test_bookmarks_panel.moc"
