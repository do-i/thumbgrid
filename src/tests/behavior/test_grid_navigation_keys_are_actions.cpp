#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core.h"
#include "gui/customwidgets/thumbnailwidget.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// The grid's arrows, PgUp/PgDown and Home/End move the selection (Shift extends
// it; Alt or Meta change nothing; Ctrl hands the key to the bindings). They
// used to be hard-coded; they are now the grid actions selectPrevious,
// selectNext, selectAbove, selectBelow, selectPageUp, selectPageDown,
// selectFirst and selectLast, and behave exactly as before by default - while
// another key can take a move, and unbinding a key turns its move off.
// Ctrl+wheel resizing the thumbnails is a grid binding too (zoomIn/zoomOut), and
// so are a tile's double-click (LMB_TileDoubleClick: openSelected) and dragging
// the selection out (the grid's LMB_Drag: dragFileOut).

class GridNavigationKeysAreActionsTest : public QObject {
    Q_OBJECT

private slots:
    void navigationKeysMoveTheSelectionAsBefore();
};

namespace {

FolderGridView *gGrid = nullptr;

// Keys are named by native scancode (QTest key events carry none).
void press(Qt::Key key, const QString &name, Qt::KeyboardModifiers mods = Qt::NoModifier, quint32 scanCode = 0) {
    if(!scanCode)
        scanCode = inputMap->keys().key(name);
    QKeyEvent down(QEvent::KeyPress, key, mods, scanCode, 0, 0);
    QApplication::sendEvent(gGrid, &down);
    QKeyEvent up(QEvent::KeyRelease, key, mods, scanCode, 0, 0);
    QApplication::sendEvent(gGrid, &up);
    QCoreApplication::processEvents();
}

// Shift is a key of its own: holding it is what arms range selection.
void pressWithShift(Qt::Key key, const QString &name) {
    QKeyEvent shiftDown(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier, 50, 0, 0);
    QApplication::sendEvent(gGrid, &shiftDown);
    press(key, name, Qt::ShiftModifier);
    QKeyEvent shiftUp(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier, 50, 0, 0);
    QApplication::sendEvent(gGrid, &shiftUp);
    QCoreApplication::processEvents();
}

void sendMouse(QEvent::Type type, QPoint pos, Qt::MouseButtons held) {
    QMouseEvent event(type, pos, gGrid->viewport()->mapToGlobal(pos), Qt::LeftButton, held, Qt::NoModifier);
    QApplication::sendEvent(gGrid->viewport(), &event);
    QCoreApplication::processEvents();
}

// the viewport position of a tile showing a file (not the ".." tile)
QPoint fileTilePos() {
    QList<ThumbnailWidget *> tiles;
    for(QGraphicsItem *item : gGrid->items())
        if(auto tile = qgraphicsitem_cast<ThumbnailWidget *>(item))
            tiles << tile;
    std::sort(tiles.begin(), tiles.end(), [](ThumbnailWidget *a, ThumbnailWidget *b) {
        return std::make_pair(a->y(), a->x()) < std::make_pair(b->y(), b->x());
    });
    return gGrid->mapFromScene(tiles.at(2)->sceneBoundingRect().center());
}

void ctrlWheel(int angleY) {
    QWidget *viewport = gGrid->viewport();
    const QPointF local = QRectF(viewport->rect()).center();
    QWheelEvent wheel(local, viewport->mapToGlobal(local), QPoint(), QPoint(0, angleY), Qt::NoButton,
                      Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(viewport, &wheel);
    QCoreApplication::processEvents();
}

} // namespace

void GridNavigationKeysAreActionsTest::navigationKeysMoveTheSelectionAsBefore() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    for(int i = 0; i < 30; i++)
        QVERIFY(tgtest::writeImage(root.filePath(QString("gallery/%1.png").arg(i, 2, 10, QChar('0'))), Qt::darkCyan));

    Core core;
    QVERIFY(core.loadPath(root.filePath("gallery")));
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);
    gGrid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY(gGrid);
    QTRY_VERIFY(gGrid->itemCount() > 20);
    const int count = gGrid->itemCount();
    gGrid->setFocus();

    const auto single = [](int index) {
        return gGrid->selection().count() == 1 && gGrid->lastSelected() == index;
    };

    gGrid->select(5);
    press(Qt::Key_Right, "Right");
    QVERIFY(single(6));
    press(Qt::Key_Left, "Left");
    QVERIFY(single(5));
    press(Qt::Key_Down, "Down");
    const int below = gGrid->lastSelected();
    QVERIFY(below > 6);
    press(Qt::Key_Up, "Up");
    QVERIFY(single(5));
    press(Qt::Key_PageDown, "PgDown");
    QVERIFY(gGrid->lastSelected() > 5);
    press(Qt::Key_PageUp, "PgUp");
    QVERIFY(gGrid->lastSelected() < count - 1);
    press(Qt::Key_End, "End");
    QVERIFY(single(count - 1));
    press(Qt::Key_Home, "Home");
    const int first = gGrid->lastSelected();
    QVERIFY(first <= 1);
    QCOMPARE(gGrid->selection().count(), 1);

    // Shift extends the selection - even over a grid binding on Shift+Right
    actionManager->addShortcut(MODE_FOLDERVIEW, "Shift+Right", "nextDirectory");
    disconnect(actionManager, &ActionManager::nextDirectory, nullptr, nullptr);
    QSignalSpy nextDirectory(actionManager, &ActionManager::nextDirectory);
    gGrid->select(5);
    pressWithShift(Qt::Key_Right, "Right");
    QCOMPARE(gGrid->selection().count(), 2);
    QVERIFY(gGrid->selection().contains(5) && gGrid->selection().contains(6));
    QCOMPARE(nextDirectory.count(), 0);

    // Alt and Meta change nothing
    gGrid->select(5);
    press(Qt::Key_Right, "Right", Qt::AltModifier);
    QVERIFY(single(6));
    press(Qt::Key_Right, "Right", Qt::MetaModifier);
    QVERIFY(single(7));

    // numpad 6 (X11 keycode 85, named "6") with NumLock off arrives as Right
    press(Qt::Key_Right, "6", Qt::KeypadModifier, 85);
    QVERIFY(single(8));

    // Ctrl hands the key to the bindings instead of moving
    actionManager->addShortcut(MODE_FOLDERVIEW, "Ctrl+Right", "toggleStatusFooter");
    disconnect(actionManager, &ActionManager::toggleStatusFooter, nullptr, nullptr);
    QSignalSpy footer(actionManager, &ActionManager::toggleStatusFooter);
    press(Qt::Key_Right, "Right", Qt::ControlModifier);
    QVERIFY(single(8));
    QCOMPARE(footer.count(), 1);

    // Configurable: another key can take a move, and Shift extends on it too.
    actionManager->addShortcut(MODE_FOLDERVIEW, "L", "selectNext");
    gGrid->select(5);
    press(Qt::Key_L, "L");
    QVERIFY(single(6));
    pressWithShift(Qt::Key_L, "L");
    QCOMPARE(gGrid->selection().count(), 2);
    QVERIFY(gGrid->selection().contains(6) && gGrid->selection().contains(7));

    // Unbinding Right turns its move off; rebinding it runs the new action.
    actionManager->removeShortcut(MODE_FOLDERVIEW, "Right");
    gGrid->select(5);
    press(Qt::Key_Right, "Right");
    QVERIFY(single(5));
    actionManager->addShortcut(MODE_FOLDERVIEW, "Right", "toggleStatusFooter");
    press(Qt::Key_Right, "Right");
    QVERIFY(single(5));
    QCOMPARE(footer.count(), 2);

    // Ctrl+wheel resizes the thumbnails through its grid binding (zoomIn or
    // zoomOut, depending on the fresh profile's wheel migration)...
    // (each resize is signalled more than once, so compare sizes, not counts)
    QSignalSpy sizes(gGrid, &FolderGridView::thumbnailSizeChanged);
    const auto lastSize = [&]() { return sizes.last().at(0).toInt(); };
    QVERIFY(QStringList({"zoomIn", "zoomOut"}).contains(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Ctrl+WheelUp")));
    ctrlWheel(120);
    QVERIFY2(!sizes.isEmpty(), "Ctrl+wheel should resize the thumbnails.");
    // ...and the zoom actions do it from any key bound in the grid.
    const int size0 = lastSize();
    actionManager->invokeAction("zoomOut");
    QVERIFY(lastSize() < size0);
    actionManager->invokeAction("zoomInCursor");
    QCOMPARE(lastSize(), size0);
    // Bound to another action, Ctrl+wheel runs that instead; unbound, nothing.
    sizes.clear();
    actionManager->addShortcut(MODE_FOLDERVIEW, "Ctrl+WheelUp", "toggleStatusFooter");
    ctrlWheel(120);
    QVERIFY(sizes.isEmpty());
    QCOMPARE(footer.count(), 3);
    actionManager->removeShortcut(MODE_FOLDERVIEW, "Ctrl+WheelUp");
    ctrlWheel(120);
    QVERIFY(sizes.isEmpty());
    QCOMPARE(footer.count(), 3);

    // Dragging the selection out is the grid's LMB_Drag: dragFileOut by
    // default; unbound, nothing. (The real drag-and-drop is cut off here.)
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "LMB_Drag"), QStringLiteral("dragFileOut"));
    disconnect(gGrid, &FolderGridView::draggedOut, nullptr, nullptr);
    QSignalSpy draggedOut(gGrid, &FolderGridView::draggedOut);
    const QPoint tile = fileTilePos();
    const auto dragTile = [&]() {
        sendMouse(QEvent::MouseButtonPress, tile, Qt::LeftButton);
        sendMouse(QEvent::MouseMove, tile + QPoint(30, 30), Qt::LeftButton);
        sendMouse(QEvent::MouseMove, tile + QPoint(60, 60), Qt::LeftButton);
        sendMouse(QEvent::MouseButtonRelease, tile + QPoint(60, 60), Qt::NoButton);
    };
    dragTile();
    QVERIFY2(!draggedOut.isEmpty(), "Dragging a selected tile should drag the selection out.");
    draggedOut.clear();
    actionManager->removeShortcut(MODE_FOLDERVIEW, "LMB_Drag");
    dragTile();
    QVERIFY2(draggedOut.isEmpty(), "With LMB_Drag unbound in the grid, nothing is dragged out.");

    // A tile's double-click is LMB_TileDoubleClick: another action can take it,
    // unbound it does nothing, and by default it opens the tile.
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "LMB_TileDoubleClick"), QStringLiteral("openSelected"));
    actionManager->addShortcut(MODE_FOLDERVIEW, "LMB_TileDoubleClick", "toggleStatusFooter");
    sendMouse(QEvent::MouseButtonDblClick, tile, Qt::LeftButton);
    QCOMPARE(footer.count(), 4);
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);
    actionManager->removeShortcut(MODE_FOLDERVIEW, "LMB_TileDoubleClick");
    sendMouse(QEvent::MouseButtonDblClick, tile, Qt::LeftButton);
    QTest::qWait(100);
    QCOMPARE(footer.count(), 4);
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);
    actionManager->addShortcut(MODE_FOLDERVIEW, "LMB_TileDoubleClick", "openSelected");
    sendMouse(QEvent::MouseButtonDblClick, tile, Qt::LeftButton);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
}

TG_BEHAVIOR_TEST_MAIN(GridNavigationKeysAreActionsTest)

#include "test_grid_navigation_keys_are_actions.moc"
