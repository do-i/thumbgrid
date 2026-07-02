#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QGraphicsItem>
#include <QTemporaryDir>

#include "core.h"
#include "gui/customwidgets/thumbnailwidget.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// Regression test for the root-folder thumbnail-loading bug: navigating into a
// folder (the same code path used when a root child is activated) must leave the
// grid cells loaded, not stuck on the loading icon. The defect was that
// DirectoryPresenter::generateThumbnails() cleared all pending thumbnail jobs on
// every visible-thumbnail request, so a freshly opened folder's jobs were
// cancelled before they could run.
class OpeningAFolderLoadsItsThumbnailsTest : public QObject {
    Q_OBJECT

private slots:
    void openingAFolderLoadsItsThumbnails();
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

void OpeningAFolderLoadsItsThumbnailsTest::openingAFolderLoadsItsThumbnails() {
    // The feature that made this bug reachable; enabling it here documents the
    // interaction even though the fixture is reached via the temp dir.
    settings->setAllowBrowseRoot(true);

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

    // Parent tile + child folder + two images: every visible cell should load.
    QTRY_COMPARE(grid->itemCount(), 4);
    QTRY_COMPARE(loadedTileCount(grid), 4);

    // Activate the child folder (index 0 is the ".." parent tile, folders sort
    // ahead of files). This is the navigation that previously left cells stuck.
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);

    // Inside the child folder: parent tile + its single image, both loaded.
    QTRY_COMPARE(grid->itemCount(), 2);
    QTRY_COMPARE(loadedTileCount(grid), 2);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(OpeningAFolderLoadsItsThumbnailsTest)

#include "test_opening_a_folder_loads_its_thumbnails.moc"
