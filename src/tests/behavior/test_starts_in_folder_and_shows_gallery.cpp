#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class StartsInFolderAndShowsGalleryTest : public QObject {
    Q_OBJECT

private slots:
    void startsInAFolderAndShowsTheGallery();
};

void StartsInFolderAndShowsGalleryTest::startsInAFolderAndShowsTheGallery() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/child-folder"), "Gallery child folder should be created.");

    const QString galleryPath = root.filePath("gallery");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/first.png"), Qt::red), "First test image should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/second.png"), Qt::blue), "Second test image should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent folder tile + one child folder + two image thumbnails.
    QTRY_COMPARE(grid->itemCount(), 4);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(StartsInFolderAndShowsGalleryTest)

#include "test_starts_in_folder_and_shows_gallery.moc"
