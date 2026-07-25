// Slideshow shipped with no default shortcut and no menu entry, so on a stock
// install there was no way to start one at all. The grid context menu is now
// the discoverable route in, which puts two new demands on Core::startSlideshow:
// it must begin at the file the user highlighted in the grid (not at whatever
// document happened to be open last), and it must survive being invoked on a
// folder that holds no viewable images.

#include "support/thumbgrid_test_support.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QMouseEvent>
#include <QTemporaryDir>

#include "core.h"
#include "gui/customwidgets/contextmenuitem.h"
#include "gui/folderview/foldergridview.h"
#include "gui/folderview/gridcontextmenu.h"
#include "gui/mainwindow.h"
#include "settings.h"

class SlideshowStartsFromTheGridTest : public QObject {
    Q_OBJECT

private slots:
    void theGridContextMenuStartsASlideshowAtTheSelectedFile();
};

namespace {

// Same approach as the context-menu gating test: QGraphicsView only routes
// context-menu events through contextMenuEvent() when they land on its viewport.
GridContextMenu *openContextMenuFor(FolderGridView *grid) {
    QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(0, 0), grid->mapToGlobal(QPoint(0, 0)));
    QApplication::sendEvent(grid->viewport(), &event);
    return grid->findChild<GridContextMenu *>();
}

// A real press is what drives ContextMenuItem::onPress() -> invokeAction(), so
// press the widget rather than calling the action by name: that keeps the menu
// wiring itself under test.
void pressItem(ContextMenuItem *item) {
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(1, 1), item->mapToGlobal(QPoint(1, 1)),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(item, &press);
}

} // namespace

void SlideshowStartsFromTheGridTest::theGridContextMenuStartsASlideshowAtTheSelectedFile() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "gallery folder should be created.");
    QVERIFY2(root.mkpath("noimg"), "noimg folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString noimgPath = root.filePath("noimg");

    QVERIFY2(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::red), "Image a should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/b.png"), Qt::blue), "Image b should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/c.png"), Qt::green), "Image c should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/d.png"), Qt::yellow), "Image d should be written.");
    {
        QFile notes(root.filePath("noimg/notes.txt"));
        QVERIFY(notes.open(QIODevice::WriteOnly));
        notes.write("just some notes");
    }

    // Core reads the interval once, in its constructor. It is deliberately kept
    // long relative to the "starts here" assertion below: with a short interval
    // a slideshow that wrongly began at a.png would simply *advance* onto the
    // expected file inside the wait window and the assertion would pass anyway.
    settings->setSlideshowInterval(3000);

    Core core;
    QVERIFY2(core.loadPath(noimgPath), "Opening the image-less folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Case: a folder with nothing viewable in it. The entry is still offered
    // (it is a view action, not a selection-gated file op) but starting must be
    // refused instead of switching to an empty document view.
    GridContextMenu *menu = openContextMenuFor(grid);
    QVERIFY2(menu != nullptr, "The grid context menu should exist.");
    auto *slideshowItem = menu->findChild<ContextMenuItem *>("menuSlideshow");
    QVERIFY2(slideshowItem != nullptr, "The grid context menu should offer a slideshow entry.");
    QVERIFY2(slideshowItem->isEnabled(), "The slideshow entry should be enabled.");
    pressItem(slideshowItem);
    menu->hide();
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    // Case: a real gallery. Open a.png first and go back to the grid, so a
    // document *is* already loaded - that is what makes the next assertion
    // discriminating. enableDocumentView() only fills in a path when nothing is
    // open at all, so before the fix the slideshow resumed at a.png here no
    // matter which tile the user had highlighted.
    QVERIFY2(core.loadPath(root.filePath("gallery/a.png")), "Opening a.png should succeed.");
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    QTRY_VERIFY(window->windowTitle().contains(QStringLiteral("a.png")));

    actionManager->invokeAction(QStringLiteral("toggleFolderView"));
    QTRY_COMPARE(window->currentViewMode(), MODE_FOLDERVIEW);
    // Parent ".." (0), then files sorted: a.png=1, b.png=2, c.png=3, d.png=4.
    QTRY_COMPARE(grid->itemCount(), 5);
    grid->select(3);

    menu = openContextMenuFor(grid);
    slideshowItem = menu->findChild<ContextMenuItem *>("menuSlideshow");
    QVERIFY(slideshowItem != nullptr);
    pressItem(slideshowItem);
    menu->hide();

    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    // Well inside one 3 s interval, so this can only be satisfied by starting on
    // c.png - not by starting on a.png and stepping forward.
    QTRY_VERIFY2_WITH_TIMEOUT(window->windowTitle().contains(QStringLiteral("c.png")),
                              "the slideshow must start on the highlighted file, not on the last-viewed one",
                              1000);

    // ...and it must actually be running, not merely have opened the image.
    QTRY_VERIFY2_WITH_TIMEOUT(window->windowTitle().contains(QStringLiteral("d.png")),
                              "the slideshow must advance to the next image on its own",
                              8000);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(SlideshowStartsFromTheGridTest)

#include "test_slideshow_starts_from_the_grid.moc"
