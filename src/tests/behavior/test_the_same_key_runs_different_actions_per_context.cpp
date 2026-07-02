#include "support/thumbgrid_test_support.h"

#include <QKeyEvent>
#include <QSignalSpy>

// The point of context-aware shortcuts: one key bound in two contexts runs a
// different action depending on which screen is active, with no global fallback.
class TheSameKeyRunsDifferentActionsPerContextTest : public QObject {
    Q_OBJECT

private slots:
    void theSameKeyRunsDifferentActionsPerContext();
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

TG_BEHAVIOR_TEST_MAIN(TheSameKeyRunsDifferentActionsPerContextTest)

#include "test_the_same_key_runs_different_actions_per_context.moc"
