#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core.h"
#include "gui/mainwindow.h"
#include "gui/overlays/renameoverlay.h"

// The rename overlay swallows keys while you type, except the ones bound to
// exit and rename. It used to copy those keys once, when the overlay was first
// created, so rebinding rename afterwards left the new key swallowed (and the
// old one still passing through).

class RenameOverlayFollowsReboundKeysTest : public QObject {
    Q_OBJECT

private slots:
    void reboundRenameKeyStillReachesTheShortcuts();
};

namespace {

// ShortcutBuilder names keys by native scancode (QTest key events carry none),
// so press keys with the scancode the keymap assigns to that key name.
void pressNamedKey(QWidget *target, const QString &name, Qt::Key key) {
    const quint32 scanCode = inputMap->keys().key(name);
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, scanCode, 0, 0);
    QApplication::sendEvent(target, &press);
    QCoreApplication::processEvents();
}

} // namespace

void RenameOverlayFollowsReboundKeysTest::reboundRenameKeyStillReachesTheShortcuts() {
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
    QVERIFY2(inputMap->keys().key("F3") != 0, "The keymap should name the F3 scancode.");

    // Open the overlay first, so it exists before the rebinding below.
    QVERIFY(actionManager->invokeAction("renameFile"));
    auto overlay = window->findChild<RenameOverlay *>();
    QVERIFY(overlay);
    QTRY_VERIFY2(overlay->isVisible(), "The rename overlay should open.");

    actionManager->removeAllShortcuts("renameFile");
    actionManager->addShortcut(MODE_GLOBAL, "F3", "renameFile");
    QSignalSpy renameAction(actionManager, &ActionManager::renameFile);

    // The new key passes through the overlay to the shortcuts, which close it.
    pressNamedKey(overlay, "F3", Qt::Key_F3);
    QCOMPARE(renameAction.count(), 1);
    QTRY_VERIFY2(overlay->isHidden(), "The rebound rename key should close the overlay.");
}

TG_BEHAVIOR_TEST_MAIN(RenameOverlayFollowsReboundKeysTest)

#include "test_rename_overlay_follows_rebound_keys.moc"
