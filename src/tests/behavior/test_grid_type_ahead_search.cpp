#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QLabel>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// Pressing `/` in the grid starts a type-ahead search: typed characters extend
// a name prefix and the cursor jumps to the first entry that starts with it.
// While the search is up it owns the keyboard, so regular shortcuts must not
// fire; Escape, backspacing over the leading `/`, and navigating away all put
// the grid back into normal mode.
class GridTypeAheadSearchTest : public QObject {
    Q_OBJECT

private slots:
    void slashSearchMovesTheCursorAndSuppressesShortcuts();

private:
    static QLabel *indicatorOf(FolderGridView *grid) {
        return grid->findChild<QLabel *>("gridSearchIndicator");
    }
};

void GridTypeAheadSearchTest::slashSearchMovesTheCursorAndSuppressesShortcuts() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/sample"), "Sample folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/alpha.png"), Qt::red),
             "Image alpha.png should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/test.png"), Qt::blue),
             "Image test.png should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), the sample folder (1), then alpha.png (2), test.png (3).
    QTRY_COMPARE(grid->itemCount(), 4);
    grid->select(0);

    // --- `/t` jumps to test.png ---
    QTest::keyClick(grid, Qt::Key_Slash);
    QVERIFY2(grid->searchMode(), "Slash should start the type-ahead search.");
    QCOMPARE(grid->searchQuery(), QString());

    QTest::keyClick(grid, Qt::Key_T);
    QCOMPARE(grid->searchQuery(), QString("t"));
    QCOMPARE(grid->selection(), QList<int>{3});

    QLabel *indicator = indicatorOf(grid);
    QVERIFY2(indicator != nullptr && indicator->isVisible(),
             "The typed query should be shown in the grid.");
    QCOMPARE(indicator->text(), QString("/t"));

    // --- shortcuts are inert while searching ---
    // Ctrl+A would normally select every cell in the grid.
    QTest::keyClick(grid, Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(grid->selection(), QList<int>{3});
    QVERIFY2(grid->searchMode(), "A shortcut combination should not end the search.");
    // The query is untouched: Ctrl+A is swallowed, not typed.
    QCOMPARE(grid->searchQuery(), QString("t"));

    // --- backspace deletes the query, then the leading `/` ---
    QTest::keyClick(grid, Qt::Key_Backspace);
    QCOMPARE(grid->searchQuery(), QString());
    QVERIFY2(grid->searchMode(), "An empty query should still be search mode.");
    QCOMPARE(grid->selection(), QList<int>{3});

    // Backspace is bound to "go up" outside the search; here it only removes
    // the `/` and returns to normal mode, leaving the directory alone.
    QTest::keyClick(grid, Qt::Key_Backspace);
    QVERIFY2(!grid->searchMode(), "Backspacing over the slash should end the search.");
    QVERIFY2(indicator->isHidden(), "The query indicator should be hidden again.");
    QTest::qWait(50);
    QCOMPARE(grid->itemCount(), 4);

    // --- `/s` jumps to the sample folder ---
    QTest::keyClick(grid, Qt::Key_Slash);
    QTest::keyClick(grid, Qt::Key_S);
    QCOMPARE(grid->selection(), QList<int>{1});

    // --- Escape cancels ---
    QTest::keyClick(grid, Qt::Key_Escape);
    QVERIFY2(!grid->searchMode(), "Escape should cancel the search.");
    QCOMPARE(grid->searchQuery(), QString());

    // --- a prefix that matches nothing keeps the cursor where it is ---
    QTest::keyClick(grid, Qt::Key_Slash);
    QTest::keyClick(grid, Qt::Key_Z);
    QCOMPARE(grid->searchQuery(), QString("z"));
    QCOMPARE(grid->selection(), QList<int>{1});

    // --- navigating away resets the mode ---
    QVERIFY2(core.loadPath(root.absolutePath()), "Opening the parent folder should succeed.");
    QTRY_VERIFY2(!grid->searchMode(), "Leaving the directory should end the search.");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(GridTypeAheadSearchTest)

#include "test_grid_type_ahead_search.moc"
