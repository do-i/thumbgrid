#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core.h"
#include "gui/contextmenu.h"
#include "gui/mainwindow.h"
#include "gui/viewers/viewerwidget.h"

// Esc in the picture-view context menu closes the menu. It used to be passed on
// to the shortcuts as well, so the global Esc binding (leave fullscreen / exit)
// ran too and quit the app from a menu dismissal.

class PictureContextMenuEscTest : public QObject {
    Q_OBJECT

private slots:
    void escClosesTheMenuWithoutRunningTheEscBinding();
};

namespace {

// The menu names keys by native scancode (QTest key events carry none), so
// press keys with the scancode the keymap assigns to that key name.
void pressNamedKey(QWidget *target, const QString &name, Qt::Key key) {
    const quint32 scanCode = inputMap->keys().key(name);
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, scanCode, 0, 0);
    QApplication::sendEvent(target, &press);
    QCoreApplication::processEvents();
}

} // namespace

void PictureContextMenuEscTest::escClosesTheMenuWithoutRunningTheEscBinding() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    QVERIFY(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::darkCyan));

    Core core;
    QVERIFY(core.loadPath(root.filePath("gallery/a.png")));
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    QCOMPARE(actionManager->actionForShortcut(MODE_GLOBAL, "Esc"), QStringLiteral("closeFullScreenOrExit"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "I"), QStringLiteral("toggleImageInfo"));
    QVERIFY2(inputMap->keys().key("Esc") != 0 && inputMap->keys().key("I") != 0,
             "The keymap should name the Esc and I scancodes.");
    // Observe the bound actions without letting them act (exit would end the test).
    disconnect(actionManager, &ActionManager::closeFullScreenOrExit, nullptr, nullptr);
    disconnect(actionManager, &ActionManager::toggleImageInfo, nullptr, nullptr);
    QSignalSpy escAction(actionManager, &ActionManager::closeFullScreenOrExit);
    QSignalSpy infoAction(actionManager, &ActionManager::toggleImageInfo);

    auto viewerWidget = window->findChild<ViewerWidget *>();
    QVERIFY(viewerWidget);
    viewerWidget->showContextMenu(viewerWidget->mapToGlobal(QPoint(100, 100)));
    auto menu = window->findChild<ContextMenu *>();
    QVERIFY(menu);
    QTRY_VERIFY2(menu->isVisible(), "The context menu should open.");

    // Other keys still reach the shortcuts while the menu is open.
    pressNamedKey(menu, "I", Qt::Key_I);
    QCOMPARE(infoAction.count(), 1);

    pressNamedKey(menu, "Esc", Qt::Key_Escape);
    QTRY_VERIFY2(!menu->isVisible(), "Esc should close the context menu.");
    QTest::qWait(50);
    QCOMPARE(escAction.count(), 0);
}

TG_BEHAVIOR_TEST_MAIN(PictureContextMenuEscTest)

#include "test_picture_context_menu_esc_only_closes_the_menu.moc"
