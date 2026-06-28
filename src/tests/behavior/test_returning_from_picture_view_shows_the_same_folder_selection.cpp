#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QKeyEvent>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "utils/inputmap.h"

class ReturningFromPictureViewShowsTheSameFolderSelectionTest : public QObject {
    Q_OBJECT

private slots:
    void returningFromPictureViewShowsTheSameFolderSelection();
};

void ReturningFromPictureViewShowsTheSameFolderSelectionTest::returningFromPictureViewShowsTheSameFolderSelection() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/child-folder"), "Gallery child folder should be created.");

    const QString galleryPath = root.filePath("gallery");
    // Multiple images plus one child folder.
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::red), "Image a should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/b.png"), Qt::blue), "Image b should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." tile + one child folder + two image tiles.
    QTRY_COMPARE(grid->itemCount(), 4);

    // Open an image: index 0 is the ".." parent tile, the child folder sorts
    // ahead of files at index 1, so the first image sits at index 2.
    const int openedIndex = 2;
    grid->select(openedIndex);
    QTest::keyClick(grid, Qt::Key_Return);

    // We are now in the picture/content view showing that image.
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    QTRY_VERIFY2(window->windowTitle().contains("a.png"),
                 "The picture view should display the opened image.");

    // Return to the folder view the same way a user does: the default Backspace
    // shortcut triggers the "folderView" action. The action manager resolves
    // shortcuts from the native scan code, so look it up for this platform
    // rather than hard-coding one, and deliver it to the main window.
    const quint32 backspaceScanCode = inputMap->keys().key("Backspace");
    QVERIFY2(backspaceScanCode != 0, "Backspace should be mapped for this platform.");
    QKeyEvent backspacePress(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier,
                             backspaceScanCode, 0, 0);
    QApplication::sendEvent(window, &backspacePress);

    // We are back in the original folder: same parent grid, same item count.
    QTRY_COMPARE(window->currentViewMode(), MODE_FOLDERVIEW);
    QCOMPARE(grid->itemCount(), 4);

    // The previously opened image remains the selected/focused tile.
    QTRY_VERIFY2(grid->selection().contains(openedIndex),
                 "The previously opened image should remain selected.");
    QCOMPARE(grid->lastSelected(), openedIndex);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(ReturningFromPictureViewShowsTheSameFolderSelectionTest)

#include "test_returning_from_picture_view_shows_the_same_folder_selection.moc"
