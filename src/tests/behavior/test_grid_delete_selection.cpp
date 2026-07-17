#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class GridDeleteSelectionTest : public QObject {
    Q_OBJECT

private slots:
    void removeFileActionDeletesSelectedFilePermanently();
};

void GridDeleteSelectionTest::removeFileActionDeletesSelectedFilePermanently() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString aPath = root.filePath("gallery/a.png");
    const QString bPath = root.filePath("gallery/b.png");

    QVERIFY2(tgtest::writeImage(aPath, Qt::red), "Image a should be written.");
    QVERIFY2(tgtest::writeImage(bPath, Qt::blue), "Image b should be written.");

    // Never trash in tests -- permanent delete only, and skip the confirmation
    // dialog (it would otherwise block waiting for user input under offscreen).
    settings->setConfirmDelete(false);
    settings->setConfirmTrash(false);

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), then a.png(1), b.png(2).
    QTRY_COMPARE(grid->itemCount(), 3);
    const int previousCount = grid->itemCount();

    grid->select(1);
    QCOMPARE(grid->selection(), QList<int>{1});

    QVERIFY2(actionManager->invokeAction("removeFile"), "removeFile action should be invocable.");

    QTRY_VERIFY2(!QFile::exists(aPath), "The permanently deleted file should be gone from disk.");
    QTRY_COMPARE(grid->itemCount(), previousCount - 1);
    QVERIFY2(QFile::exists(bPath), "The untouched file should remain.");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(GridDeleteSelectionTest)

#include "test_grid_delete_selection.moc"
