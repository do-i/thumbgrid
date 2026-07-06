#include "support/thumbgrid_test_support.h"

#include "gui/dialogs/settingsdialog.h"
#include "gui/dialogs/shortcutcreatordialog.h"

#include <QComboBox>
#include <QKeyEvent>
#include <QSignalSpy>

// The point of context-aware shortcuts: one key bound in two contexts runs a
// different action depending on which screen is active, and a global binding is
// used only when the active screen does not override that key.
class TheSameKeyRunsDifferentActionsPerContextTest : public QObject {
    Q_OBJECT

private slots:
    void theSameKeyRunsDifferentActionsPerContext();
    void contextSpecificShortcutOverridesGlobalShortcut();
    void shortcutContextDropdownsDefaultToGlobal();
    void resettingDefaultsCanTargetOneContext();
};

void TheSameKeyRunsDifferentActionsPerContextTest::theSameKeyRunsDifferentActionsPerContext() {
    // Start from a clean slate and bind the same key to a different action in
    // each context.
    actionManager->removeAllShortcuts();
    actionManager->addShortcut(MODE_DOCUMENT, "C", "nextImage");
    actionManager->addShortcut(MODE_FOLDERVIEW, "C", "sortByName");

    // The binding is genuinely keyed by (context, key): same key, two actions.
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "C"), QStringLiteral("nextImage"));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "C"), QStringLiteral("sortByName"));

    // Dispatch routes through ShortcutBuilder, which on Linux/Windows keys off the
    // native scancode. Look up the scancode the running platform maps to "C" so the
    // fabricated key event resolves to the same "C" sequence a real press would.
    const quint32 scanCode = inputMap->keys().key("C");
    QVERIFY2(scanCode != 0, "Platform input map should know a scancode for the C key.");

    QSignalSpy nextImageSpy(actionManager, &ActionManager::nextImage);
    QSignalSpy sortByNameSpy(actionManager, &ActionManager::sortByName);
    QVERIFY(nextImageSpy.isValid() && sortByNameSpy.isValid());

    // In the document context, "C" runs the document action only.
    actionManager->setContext(MODE_DOCUMENT);
    {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, scanCode, 0, 0, "c");
        QVERIFY2(actionManager->processEvent(&press), "C should be handled in the document context.");
    }
    QCOMPARE(nextImageSpy.count(), 1);
    QCOMPARE(sortByNameSpy.count(), 0);

    // Switching context flips what the very same key does.
    actionManager->setContext(MODE_FOLDERVIEW);
    {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, scanCode, 0, 0, "c");
        QVERIFY2(actionManager->processEvent(&press), "C should be handled in the folder context.");
    }
    QCOMPARE(nextImageSpy.count(), 1);
    QCOMPARE(sortByNameSpy.count(), 1);
}

void TheSameKeyRunsDifferentActionsPerContextTest::contextSpecificShortcutOverridesGlobalShortcut() {
    actionManager->removeAllShortcuts();
    actionManager->addShortcut(MODE_GLOBAL, "C", "copyFile");
    actionManager->addShortcut(MODE_FOLDERVIEW, "C", "sortByName");

    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "C"), QStringLiteral("copyFile"));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "C"), QStringLiteral("sortByName"));

    const quint32 scanCode = inputMap->keys().key("C");
    QVERIFY2(scanCode != 0, "Platform input map should know a scancode for the C key.");

    QSignalSpy copyFileSpy(actionManager, &ActionManager::copyFile);
    QSignalSpy sortByNameSpy(actionManager, &ActionManager::sortByName);
    QVERIFY(copyFileSpy.isValid() && sortByNameSpy.isValid());

    actionManager->setContext(MODE_DOCUMENT);
    {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, scanCode, 0, 0, "c");
        QVERIFY2(actionManager->processEvent(&press), "C should fall back to the global shortcut.");
    }
    QCOMPARE(copyFileSpy.count(), 1);
    QCOMPARE(sortByNameSpy.count(), 0);

    actionManager->setContext(MODE_FOLDERVIEW);
    {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, scanCode, 0, 0, "c");
        QVERIFY2(actionManager->processEvent(&press), "C should use the grid-specific shortcut.");
    }
    QCOMPARE(copyFileSpy.count(), 1);
    QCOMPARE(sortByNameSpy.count(), 1);
}

void TheSameKeyRunsDifferentActionsPerContextTest::shortcutContextDropdownsDefaultToGlobal() {
    SettingsDialog settingsDialog;
    QComboBox *settingsContext = nullptr;
    for(QComboBox *combo : settingsDialog.findChildren<QComboBox *>()) {
        if(combo->count() >= 3 && combo->itemData(0).toString() == ActionManager::contextToString(MODE_GLOBAL)) {
            settingsContext = combo;
            break;
        }
    }
    QVERIFY2(settingsContext, "The shortcuts page should expose a context dropdown with Global.");
    QCOMPARE(settingsContext->currentData().toString(), ActionManager::contextToString(MODE_GLOBAL));

    ShortcutCreatorDialog creatorDialog;
    QComboBox *creatorContext = creatorDialog.findChild<QComboBox *>("contextComboBox");
    QVERIFY2(creatorContext, "The shortcut creator should expose its context dropdown.");
    QCOMPARE(creatorContext->itemData(0).toString(), ActionManager::contextToString(MODE_GLOBAL));
    QCOMPARE(creatorContext->currentData().toString(), ActionManager::contextToString(MODE_GLOBAL));
}

void TheSameKeyRunsDifferentActionsPerContextTest::resettingDefaultsCanTargetOneContext() {
    actionManager->removeAllShortcuts();
    actionManager->addShortcut(MODE_DOCUMENT, "C", "nextImage");
    actionManager->addShortcut(MODE_FOLDERVIEW, "C", "sortByName");

    actionManager->resetDefaults(MODE_FOLDERVIEW);

    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "C"), QStringLiteral("nextImage"));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "C"), QString());
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Enter"), QStringLiteral("documentView"));
    QVERIFY2(!actionManager->allDefaultShortcuts().value(MODE_GLOBAL).isEmpty(),
             "Global defaults should be available to restored contexts.");
}

TG_BEHAVIOR_TEST_MAIN(TheSameKeyRunsDifferentActionsPerContextTest)

#include "test_the_same_key_runs_different_actions_per_context.moc"
