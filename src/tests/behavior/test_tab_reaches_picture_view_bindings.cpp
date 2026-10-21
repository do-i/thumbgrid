#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core.h"
#include "gui/mainwindow.h"
#include "gui/viewers/viewerwidget.h"

// The picture view never lets Qt use Tab / Shift+Tab to move focus
// (ViewerWidget::focusNextPrevChild returns false), so both keys reach the
// shortcuts like any other key and can be bound.

class TabReachesPictureViewBindingsTest : public QObject {
    Q_OBJECT

private slots:
    void tabAndShiftTabRunTheirBindings();
};

namespace {

// Bindings name keys by native scancode (QTest key events carry none), so press
// Tab with the scancode the keymap assigns to it.
void pressTab(QWidget *target, Qt::Key key, Qt::KeyboardModifiers modifiers) {
    QKeyEvent press(QEvent::KeyPress, key, modifiers, inputMap->keys().key("Tab"), 0, 0);
    QApplication::sendEvent(target, &press);
    QCoreApplication::processEvents();
}

} // namespace

void TabReachesPictureViewBindingsTest::tabAndShiftTabRunTheirBindings() {
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

    QVERIFY2(inputMap->keys().key("Tab") != 0, "The keymap should name the Tab scancode.");
    actionManager->addShortcut(MODE_DOCUMENT, "Tab", "toggleImageInfo");
    actionManager->addShortcut(MODE_DOCUMENT, "Shift+Tab", "toggleImageInfo");
    // observe the action without letting it open the info panel
    disconnect(actionManager, &ActionManager::toggleImageInfo, nullptr, nullptr);
    QSignalSpy infoAction(actionManager, &ActionManager::toggleImageInfo);

    auto viewerWidget = window->findChild<ViewerWidget *>();
    QVERIFY(viewerWidget);
    // Give Tab somewhere to go: with a focusable control in the view, Qt's own
    // focus traversal would take the key and the binding would never run.
    auto *focusable = new QPushButton("focusable", viewerWidget);
    focusable->setFocusPolicy(Qt::StrongFocus);
    focusable->show();
    viewerWidget->setFocus();
    QCOMPARE(QApplication::focusWidget(), viewerWidget);

    pressTab(viewerWidget, Qt::Key_Tab, Qt::NoModifier);
    QCOMPARE(infoAction.count(), 1);
    pressTab(viewerWidget, Qt::Key_Backtab, Qt::ShiftModifier);
    QCOMPARE(infoAction.count(), 2);
    QCOMPARE(QApplication::focusWidget(), viewerWidget);
}

TG_BEHAVIOR_TEST_MAIN(TabReachesPictureViewBindingsTest)

#include "test_tab_reaches_picture_view_bindings.moc"
