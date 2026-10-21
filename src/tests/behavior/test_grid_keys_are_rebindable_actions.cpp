#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// Enter, Backspace, Ctrl+A and "/" used to be handled inside the grid ahead of
// any binding. They are now the openSelected, goUp, selectAll and startSearch
// actions: they still do the same thing by default, and unbinding one turns it
// off. Select all also no longer selects the ".." parent tile.

class GridKeysAreRebindableActionsTest : public QObject {
    Q_OBJECT

private slots:
    void gridKeysRunTheirBoundActions();
};

void GridKeysAreRebindableActionsTest::gridKeysRunTheirBoundActions() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/sub"), "Gallery folders should be created.");
    QVERIFY(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::red));
    QVERIFY(tgtest::writeImage(root.filePath("gallery/b.png"), Qt::green));
    QVERIFY(tgtest::writeImage(root.filePath("gallery/c.png"), Qt::blue));
    QVERIFY(tgtest::writeImage(root.filePath("gallery/sub/inner.png"), Qt::gray));

    Core core;
    QVERIFY(core.loadPath(root.filePath("gallery")));
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);
    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // ".." + sub + three images
    QTRY_COMPARE(grid->itemCount(), 5);
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Enter"), QStringLiteral("openSelected"));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Backspace"), QStringLiteral("goUp"));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Ctrl+A"), QStringLiteral("selectAll"));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "/"), QStringLiteral("startSearch"));

    // Enter opens the selected folder, Backspace comes back up.
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(grid->itemCount(), 2);
    QTest::keyClick(grid, Qt::Key_Backspace);
    QTRY_COMPARE(grid->itemCount(), 5);

    // Ctrl+A selects every entry but not the ".." tile.
    QTest::keyClick(grid, Qt::Key_A, Qt::ControlModifier);
    QTRY_COMPARE(grid->selection().count(), 4);
    QVERIFY2(!grid->selection().contains(0), "Select all must not include the parent tile.");

    // The search starts from its action.
    QVERIFY(!grid->searchMode());
    QVERIFY(actionManager->invokeAction("startSearch"));
    QVERIFY2(grid->searchMode(), "startSearch should enter type-to-search.");
    grid->exitSearchMode();

    // Unbinding a key really turns it off now.
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(grid->itemCount(), 2);
    actionManager->removeAllShortcuts("goUp");
    QTest::keyClick(grid, Qt::Key_Backspace);
    QTest::qWait(200);
    QCOMPARE(grid->itemCount(), 2);
}

TG_BEHAVIOR_TEST_MAIN(GridKeysAreRebindableActionsTest)

#include "test_grid_keys_are_rebindable_actions.moc"
