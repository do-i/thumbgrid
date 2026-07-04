#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QGraphicsItem>
#include <QTemporaryDir>

#include "core.h"
#include "gui/customwidgets/thumbnailwidget.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// Navigating child -> parent must serve subfolder composite thumbnails from
// the cache, not regenerate them (no cache files rewritten on the revisit).
class ReturningToParentReusesCachedThumbnailsTest : public QObject {
    Q_OBJECT
private slots:
    void goingUpServesDirThumbsFromCache();
};

static int loadedTileCount(FolderGridView *grid) {
    int loaded = 0;
    for(QGraphicsItem *item : grid->items()) {
        if(auto tile = qgraphicsitem_cast<ThumbnailWidget *>(item))
            if(tile->isLoaded)
                loaded++;
    }
    return loaded;
}

static QHash<QString, QDateTime> cacheSnapshot() {
    QHash<QString, QDateTime> snap;
    QDir d(settings->thumbnailCacheDir());
    for(const QFileInfo &fi : d.entryInfoList(QDir::Files))
        snap.insert(fi.fileName(), fi.lastModified());
    return snap;
}

void ReturningToParentReusesCachedThumbnailsTest::goingUpServesDirThumbsFromCache() {
    settings->setUseThumbnailCache(true);

    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());
    const int subDirs = 5;
    for(int d = 0; d < subDirs; d++) {
        QString sub = QString("gallery/sub%1").arg(d);
        QVERIFY(root.mkpath(sub));
        for(int i = 0; i < 8; i++)
            QVERIFY(tgtest::writeImage(root.filePath(sub + QString("/img%1.png").arg(i)), Qt::darkCyan));
    }

    Core core;
    QVERIFY(core.loadPath(root.filePath("gallery")));
    core.showGui();
    QTRY_VERIFY(tgtest::mainWindow() != nullptr);
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY(grid != nullptr);

    // parent ".." tile + 5 subfolder tiles
    QTRY_COMPARE(grid->itemCount(), subDirs + 1);
    QTRY_COMPARE(loadedTileCount(grid), subDirs + 1);

    QTest::qWait(200);
    QHash<QString, QDateTime> afterFirstVisit = cacheSnapshot();
    qInfo() << "cache entries after first parent visit:" << afterFirstVisit.count();
    QVERIFY(afterFirstVisit.count() >= subDirs);

    // enter sub0 (index 1; index 0 is "..")
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(grid->itemCount(), 9); // ".." + 8 images
    QTRY_COMPARE(loadedTileCount(grid), 9);

    QTest::qWait(1100); // make rewrites distinguishable by mtime

    // go up to the parent again
    QVERIFY(QMetaObject::invokeMethod(&core, "loadParentDir", Qt::DirectConnection));
    QTRY_COMPARE(grid->itemCount(), subDirs + 1);
    QTRY_COMPARE(loadedTileCount(grid), subDirs + 1);
    QTest::qWait(300);

    QHash<QString, QDateTime> afterReturn = cacheSnapshot();
    int rewritten = 0, added = 0;
    for(auto it = afterReturn.constBegin(); it != afterReturn.constEnd(); ++it) {
        if(!afterFirstVisit.contains(it.key()))
            added++;
        else if(afterFirstVisit.value(it.key()) != it.value())
            rewritten++;
    }
    qInfo() << "after return: total" << afterReturn.count()
            << "new files" << added << "rewritten files" << rewritten;
    QCOMPARE(rewritten, 0); // regeneration on re-visit = the reported bug
}

TG_BEHAVIOR_TEST_MAIN(ReturningToParentReusesCachedThumbnailsTest)

#include "test_returning_to_parent_reuses_cached_thumbnails.moc"
