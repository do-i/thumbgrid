#include "support/thumbgrid_test_support.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/customwidgets/contextmenuitem.h"
#include "gui/folderview/foldergridview.h"
#include "gui/folderview/gridcontextmenu.h"
#include "gui/mainwindow.h"

// A context menu row used to read its shortcut once, when the menu was built.
// Hints are now re-read every time the menu opens, so a rebound key shows up.

class ContextMenuShortcutHintsTest : public QObject {
    Q_OBJECT

private slots:
    void reboundShortcutShowsOnTheNextOpen();
};

void ContextMenuShortcutHintsTest::reboundShortcutShowsOnTheNextOpen() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QVERIFY(tgtest::writeImage(QDir(fixture.path()).filePath("a.png"), Qt::red));

    Core core;
    QVERIFY(core.loadPath(fixture.path()));
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY(grid != nullptr);
    QTRY_COMPARE(grid->itemCount(), 2); // ".." and a.png
    grid->select(1);

    auto openMenu = [grid]() {
        QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(0, 0), grid->mapToGlobal(QPoint(0, 0)));
        QApplication::sendEvent(grid->viewport(), &event);
        return grid->findChild<GridContextMenu *>();
    };

    GridContextMenu *menu = openMenu();
    QVERIFY(menu != nullptr);
    auto *move = menu->findChild<ContextMenuItem *>("menuMove");
    QVERIFY(move != nullptr);
    const QString before = move->shortcut();
    menu->hide();

    const QString newKey = QStringLiteral("Ctrl+Alt+F9");
    QVERIFY2(before != newKey, "The probe key must be new to this action.");
    actionManager->removeAllShortcuts("moveFile");
    actionManager->addShortcut(actionManager->currentContext(), newKey, "moveFile");

    menu = openMenu();
    QVERIFY(menu != nullptr);
    QCOMPARE(move->shortcut(), newKey);
}

TG_BEHAVIOR_TEST_MAIN(ContextMenuShortcutHintsTest)
#include "test_context_menu_shortcut_hints_follow_rebinding.moc"
