#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class GridRenameSelectionTest : public QObject {
    Q_OBJECT

private slots:
    void renameCurrentSelectionRenamesFileAndFolder();
};

void GridRenameSelectionTest::renameCurrentSelectionRenamesFileAndFolder() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/oldfolder"), "Gallery subfolder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString oldFilePath = root.filePath("gallery/a.png");
    const QString newFilePath = root.filePath("gallery/renamed.png");
    const QString oldDirPath = root.filePath("gallery/oldfolder");
    const QString newDirPath = root.filePath("gallery/newfolder");

    QVERIFY2(tgtest::writeImage(oldFilePath, Qt::red), "Image should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), oldfolder(1), a.png(2).
    QTRY_COMPARE(grid->itemCount(), 3);
    const int previousCount = grid->itemCount();

    // Rename the file.
    grid->select(2);
    bool invoked = QMetaObject::invokeMethod(&core, "renameCurrentSelection",
                                              Q_ARG(QString, QString("renamed.png")));
    QVERIFY2(invoked, "renameCurrentSelection should be invocable via the meta-object system.");

    QTRY_VERIFY2(QFile::exists(newFilePath), "The renamed file should exist on disk.");
    QVERIFY2(!QFile::exists(oldFilePath), "The old file name should be gone from disk.");
    QCOMPARE(grid->itemCount(), previousCount);

    // Rename the folder. Re-select it: index positions did not change, only names.
    grid->select(1);
    invoked = QMetaObject::invokeMethod(&core, "renameCurrentSelection",
                                         Q_ARG(QString, QString("newfolder")));
    QVERIFY2(invoked, "renameCurrentSelection should be invocable via the meta-object system.");

    QTRY_VERIFY2(QDir(newDirPath).exists(), "The renamed folder should exist on disk.");
    QVERIFY2(!QDir(oldDirPath).exists(), "The old folder name should be gone from disk.");
    QCOMPARE(grid->itemCount(), previousCount);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(GridRenameSelectionTest)

#include "test_grid_rename_selection.moc"
