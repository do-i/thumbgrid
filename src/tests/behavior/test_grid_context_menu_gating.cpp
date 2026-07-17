#include "support/thumbgrid_test_support.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "core.h"
#include "gui/customwidgets/contextmenuitem.h"
#include "gui/folderview/foldergridview.h"
#include "gui/folderview/gridcontextmenu.h"
#include "gui/mainwindow.h"

class GridContextMenuGatingTest : public QObject {
    Q_OBJECT

private slots:
    void gridContextMenuGatesFileOpsOnSelection();
};

namespace {

// Opens the grid's context menu for the current selection and returns it. The
// menu is a child of the grid, created once in FolderGridView's constructor,
// so repeated calls return the same instance (already positioned/hidden by the
// previous case).
GridContextMenu *openContextMenuFor(FolderGridView *grid) {
    // QGraphicsView routes context-menu events delivered to its viewport (the
    // widget real right-clicks land on) through to contextMenuEvent(); sending
    // straight to the view widget itself does not reach the override.
    QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(0, 0), grid->mapToGlobal(QPoint(0, 0)));
    QApplication::sendEvent(grid->viewport(), &event);
    auto *menu = grid->findChild<GridContextMenu *>();
    return menu;
}

} // namespace

void GridContextMenuGatingTest::gridContextMenuGatesFileOpsOnSelection() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/withimg"), "withimg subfolder should be created.");
    QVERIFY2(root.mkpath("gallery/noimg"), "noimg subfolder should be created.");
    const QString galleryPath = root.filePath("gallery");

    QVERIFY2(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::red), "Image a should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/b.png"), Qt::blue), "Image b should be written.");
    {
        // Extension-only predicate: contents don't need to be an actual gif.
        QFile gif(root.filePath("gallery/anim.gif"));
        QVERIFY(gif.open(QIODevice::WriteOnly));
        gif.write("not a real gif but that's fine");
    }
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/withimg/c.png"), Qt::green),
             "Image c should be written.");
    {
        QFile notes(root.filePath("gallery/noimg/notes.txt"));
        QVERIFY(notes.open(QIODevice::WriteOnly));
        notes.write("just some notes");
    }

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), dirs sorted (noimg=1, withimg=2), files sorted
    // (a.png=3, anim.gif=4, b.png=5).
    QTRY_COMPARE(grid->itemCount(), 6);
    const int parentTile = 0;
    const int noimgDir = 1;
    const int withimgDir = 2;
    const int aPng = 3;
    const int animGif = 4;
    const int bPng = 5;

    // Case: single png selected -> everything enabled.
    grid->select(aPng);
    GridContextMenu *menu = openContextMenuFor(grid);
    QVERIFY2(menu != nullptr, "The grid context menu should exist.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuConvert")->isEnabled(), "Convert enabled for single png.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuRename")->isEnabled(), "Rename enabled for single png.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuMove")->isEnabled(), "Move enabled for single png.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuTrash")->isEnabled(), "Trash enabled for single png.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuDelete")->isEnabled(), "Delete enabled for single png.");
    menu->hide();

    // Case: single gif selected -> Convert disabled, rest enabled.
    grid->select(animGif);
    menu = openContextMenuFor(grid);
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuConvert")->isEnabled(), "Convert disabled for gif.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuRename")->isEnabled(), "Rename enabled for single gif.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuMove")->isEnabled(), "Move enabled for single gif.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuTrash")->isEnabled(), "Trash enabled for single gif.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuDelete")->isEnabled(), "Delete enabled for single gif.");
    menu->hide();

    // Case: two pngs selected -> Convert enabled, Rename disabled (multi), rest enabled.
    grid->select(QList<int>{aPng, bPng});
    menu = openContextMenuFor(grid);
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuConvert")->isEnabled(), "Convert enabled for two pngs.");
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuRename")->isEnabled(), "Rename disabled for multi-selection.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuMove")->isEnabled(), "Move enabled for two pngs.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuTrash")->isEnabled(), "Trash enabled for two pngs.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuDelete")->isEnabled(), "Delete enabled for two pngs.");
    menu->hide();

    // Case: png + gif selected -> Convert disabled (not all convertible).
    grid->select(QList<int>{aPng, animGif});
    menu = openContextMenuFor(grid);
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuConvert")->isEnabled(), "Convert disabled for png+gif.");
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuRename")->isEnabled(), "Rename disabled for png+gif (multi).");
    menu->hide();

    // Case: folder containing an image -> Convert enabled, Rename enabled (single).
    grid->select(withimgDir);
    menu = openContextMenuFor(grid);
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuConvert")->isEnabled(), "Convert enabled for folder with image.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuRename")->isEnabled(), "Rename enabled for single folder.");
    menu->hide();

    // Case: folder without a convertible image -> Convert disabled, Rename enabled.
    grid->select(noimgDir);
    menu = openContextMenuFor(grid);
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuConvert")->isEnabled(), "Convert disabled for folder without image.");
    QVERIFY2(menu->findChild<ContextMenuItem *>("menuRename")->isEnabled(), "Rename enabled for single folder.");
    menu->hide();

    // Case: only the ".." parent tile selected -> counts as an empty selection
    // (parentOffset skips it), so all file-op items are disabled.
    grid->select(parentTile);
    menu = openContextMenuFor(grid);
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuConvert")->isEnabled(), "Convert disabled for empty selection.");
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuRename")->isEnabled(), "Rename disabled for empty selection.");
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuMove")->isEnabled(), "Move disabled for empty selection.");
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuTrash")->isEnabled(), "Trash disabled for empty selection.");
    QVERIFY2(!menu->findChild<ContextMenuItem *>("menuDelete")->isEnabled(), "Delete disabled for empty selection.");
    menu->hide();

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(GridContextMenuGatingTest)

#include "test_grid_context_menu_gating.moc"
