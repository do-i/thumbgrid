#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/viewers/textviewer.h"

class OtherFileTypesToggleTest : public QObject {
    Q_OBJECT

private slots:
    void otherFileTypesToggleShowsAndOpensTextFiles();
};

void OtherFileTypesToggleTest::otherFileTypesToggleShowsAndOpensTextFiles() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");

    QVERIFY2(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::red), "Image should be written.");
    {
        QFile py(root.filePath("gallery/script.py"));
        QVERIFY(py.open(QIODevice::WriteOnly));
        py.write("import os\n\n# a comment\ndef main():\n    return \"hello\"\n");
    }
    {
        QFile bin(root.filePath("gallery/data.bin"));
        QVERIFY(bin.open(QIODevice::WriteOnly));
        const char blob[] = {0x00, 0x01, 0x02, char(0xff), char(0xfe), 0x00, 0x7f, 0x10};
        bin.write(blob, sizeof(blob));
    }

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Default: other file types are hidden. Parent ".." tile + one image.
    QTRY_COMPARE(grid->itemCount(), 2);

    // Flip the toggle; the folder rescans and the text + binary files appear.
    settings->setShowOtherFileTypes(true);
    settings->sendChangeNotification();
    QTRY_COMPARE(grid->itemCount(), 4);

    // Files sort by name: a.png(1), data.bin(2), script.py(3) after the parent tile.
    // Activating the python file opens it in the text viewer.
    grid->select(3);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    auto textViewer = window->findChild<TextViewer *>();
    QVERIFY2(textViewer != nullptr, "The text viewer should exist.");
    QTRY_VERIFY2(textViewer->isVisible(), "The text viewer should display the text file.");
    QTRY_VERIFY2(window->windowTitle().contains("script.py"),
                 "The window title should show the opened text file.");

    // Back to the folder view; activating the unviewable binary keeps the user
    // in the folder view (with a message) instead of a blank document view.
    window->enableFolderView();
    QTRY_COMPARE(window->currentViewMode(), MODE_FOLDERVIEW);
    grid->select(2);
    QTest::keyClick(grid, Qt::Key_Return);
    QTest::qWait(200);
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    // Toggle off again: the extra files disappear.
    settings->setShowOtherFileTypes(false);
    settings->sendChangeNotification();
    QTRY_COMPARE(grid->itemCount(), 2);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(OtherFileTypesToggleTest)

#include "test_other_file_types_toggle.moc"
