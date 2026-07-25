#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QGraphicsItem>
#include <QTemporaryDir>

#include "core.h"
#include "gui/customwidgets/thumbnailwidget.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// ThumbnailView::populate() turns painting off (setUpdatesEnabled(false)) while
// it rebuilds the item list, and returns with it still off: the re-enable is
// deferred to a 0-ms single-shot timer (ThumbnailView::onLayoutSettled), so the
// scene is not seen shifting as the scrollbar appears. If that deferred
// re-enable ever stops running - timer never armed, slot disconnected, the
// setUpdatesEnabled(true) dropped - the grid stays permanently blank while the
// model looks perfectly healthy. Model-state assertions cannot see that, so
// this test watches the painting state itself, across two consecutive
// populates (the second one exercises the re-armed-timer path).
class GridRepaintsAfterPopulateTest : public QObject {
    Q_OBJECT

private slots:
    void gridPaintingIsReEnabledAfterEveryPopulate();
};

// Counts the grid tiles that have finished loading (left the loading-icon state).
static int loadedTileCount(FolderGridView *grid) {
    int loaded = 0;
    for(QGraphicsItem *item : grid->items()) {
        if(auto tile = qgraphicsitem_cast<ThumbnailWidget *>(item)) {
            if(tile->isLoaded)
                loaded++;
        }
    }
    return loaded;
}

void GridRepaintsAfterPopulateTest::gridPaintingIsReEnabledAfterEveryPopulate() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/child-folder"), "Gallery child folder should be created.");

    const QString galleryPath = root.filePath("gallery");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::red), "Parent image a should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/b.png"), Qt::blue), "Parent image b should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/child-folder/inside.png"), Qt::green), "Child image should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // --- first populate: opening the gallery -----------------------------
    // Wait for the populate to have happened (item count is the model-side
    // signal) before looking at the painting state, so the assertion below
    // cannot pass just by running before populate ever disabled updates.
    QTRY_COMPARE(grid->itemCount(), 4); // ".." + child-folder + a.png + b.png
    QTRY_VERIFY2(grid->updatesEnabled(),
                 "Painting must be re-enabled after populate(); otherwise the grid stays blank forever.");
    QTRY_COMPARE(loadedTileCount(grid), 4);
    QVERIFY2(grid->updatesEnabled(), "Painting must still be enabled once the tiles have loaded.");

    // --- second populate: navigating into the child folder ---------------
    // Covers the re-armed-timer path: populate() disables painting again and
    // restarts the same single-shot timer.
    grid->select(1); // index 0 is the ".." parent tile, folders sort ahead of files
    QTest::keyClick(grid, Qt::Key_Return);

    QTRY_COMPARE(grid->itemCount(), 2); // ".." + inside.png
    QTRY_VERIFY2(grid->updatesEnabled(),
                 "Painting must be re-enabled after a second populate() too.");
    QTRY_COMPARE(loadedTileCount(grid), 2);
    QVERIFY2(grid->updatesEnabled(), "Painting must still be enabled after navigating into a folder.");

    // --- direct populate: no navigation involved -------------------------
    // Calls the view API straight so the "returns with painting off" window is
    // observed without depending on how navigation is wired. The mid-populate
    // state is only logged (it is an implementation choice); what must hold is
    // that the very next event-loop turn brings painting back.
    grid->populate(2);
    qInfo() << "updatesEnabled immediately after populate() returned:" << grid->updatesEnabled();
    QTRY_VERIFY2(grid->updatesEnabled(),
                 "Painting must be re-enabled on the event-loop turn after a direct populate().");
    QTRY_COMPARE(loadedTileCount(grid), 2);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(GridRepaintsAfterPopulateTest)

#include "test_grid_repaints_after_populate.moc"
