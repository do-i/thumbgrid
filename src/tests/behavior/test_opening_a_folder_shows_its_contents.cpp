#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class OpeningAFolderShowsItsContentsTest : public QObject {
    Q_OBJECT

private slots:
    void openingAFolderShowsWhatIsInsideIt();
};

void OpeningAFolderShowsItsContentsTest::openingAFolderShowsWhatIsInsideIt() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/child-folder"), "Gallery child folder should be created.");

    const QString galleryPath = root.filePath("gallery");
    // Parent folder: one child folder plus two images.
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::red), "Parent image a should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/b.png"), Qt::blue), "Parent image b should be written.");
    // Child folder: a single, distinct image.
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

    // Parent tile + one child folder + two parent images.
    QTRY_COMPARE(grid->itemCount(), 4);

    // Activate the child folder tile: index 0 is the ".." parent tile, folders
    // sort ahead of files, so index 1 is the child folder. Selecting and pressing
    // Enter is the same path a user takes.
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);

    // We are now inside the child folder: ".." parent tile + its single image.
    QTRY_COMPARE(grid->itemCount(), 2);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(OpeningAFolderShowsItsContentsTest)

#include "test_opening_a_folder_shows_its_contents.moc"
