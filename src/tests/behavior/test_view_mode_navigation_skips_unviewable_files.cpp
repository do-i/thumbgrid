#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// With showOtherFileTypes enabled the file list contains entries thumbgrid
// cannot render. Arrow-key navigation in the picture view must skip those
// instead of landing on them (blank view or worse).
class ViewModeNavigationSkipsUnviewableFilesTest : public QObject {
    Q_OBJECT

private slots:
    void viewModeNavigationSkipsUnviewableFiles();
};

void ViewModeNavigationSkipsUnviewableFilesTest::viewModeNavigationSkipsUnviewableFiles() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");

    // Sorted by name: a.png, b.bin (unviewable), c.png. Navigating right from
    // a.png must land on c.png.
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::red), "First image should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/c.png"), Qt::blue), "Second image should be written.");
    {
        QFile bin(root.filePath("gallery/b.bin"));
        QVERIFY(bin.open(QIODevice::WriteOnly));
        const char blob[] = {0x00, 0x01, 0x02, char(0xff), char(0xfe), 0x00, 0x7f, 0x10};
        bin.write(blob, sizeof(blob));
    }

    settings->setShowOtherFileTypes(true);

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." tile + a.png, b.bin, c.png.
    QTRY_COMPARE(grid->itemCount(), 4);

    // Open the first image (index 0 is the ".." parent tile).
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    QTRY_VERIFY2(window->windowTitle().contains("a.png"),
                 "The picture view should display the opened image.");

    // Navigate right: the unviewable b.bin is skipped, c.png is shown.
    QMetaObject::invokeMethod(&core, "nextImage");
    QTRY_VERIFY2(window->windowTitle().contains("c.png"),
                 "Navigating right should skip the unviewable file.");
    QCOMPARE(window->currentViewMode(), MODE_DOCUMENT);

    // And back left: b.bin is skipped again, a.png is shown.
    QMetaObject::invokeMethod(&core, "prevImage");
    QTRY_VERIFY2(window->windowTitle().contains("a.png"),
                 "Navigating left should skip the unviewable file.");
    QCOMPARE(window->currentViewMode(), MODE_DOCUMENT);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(ViewModeNavigationSkipsUnviewableFilesTest)

#include "test_view_mode_navigation_skips_unviewable_files.moc"
