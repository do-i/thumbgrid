#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class OpeningAPictureShowsThePictureViewTest : public QObject {
    Q_OBJECT

private slots:
    void openingAPictureShowsThePictureView();
};

void OpeningAPictureShowsThePictureViewTest::openingAPictureShowsThePictureView() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");

    const QString galleryPath = root.filePath("gallery");
    // Two images, no child folders, so file indices are easy to reason about.
    const QString firstImagePath = root.filePath("gallery/a.png");
    QVERIFY2(tgtest::writeImage(firstImagePath, Qt::red), "First image should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/b.png"), Qt::blue), "Second image should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." tile + two image tiles.
    QTRY_COMPARE(grid->itemCount(), 3);

    // Open the first image: index 0 is the ".." parent tile, with no child
    // folders the first image sits at index 1. Selecting and pressing Enter is
    // the same path a user takes to open a picture.
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);

    // The app should switch from the grid to the picture/content view.
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    // The content being displayed should be the image we opened. In document
    // view the window title tracks the path of the currently shown file.
    QTRY_VERIFY2(window->windowTitle().contains("a.png"),
                 "The picture view should display the opened image.");
    QVERIFY2(!window->windowTitle().contains("b.png"),
             "The picture view should not display the other image.");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(OpeningAPictureShowsThePictureViewTest)

#include "test_opening_a_picture_shows_the_picture_view.moc"
