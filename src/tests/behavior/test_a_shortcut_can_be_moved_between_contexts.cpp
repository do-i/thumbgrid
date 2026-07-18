#include "support/thumbgrid_test_support.h"

#include "gui/dialogs/settingsdialog.h"

#include <QTableWidget>

// A binding lives in exactly one context, and the shortcuts page must let the
// user retarget it: Move relocates it, Copy leaves the original in place. Both
// have to carry the two structures that hang off a binding - the chosen primary
// key and the enabled flag - or the shortcut arrives at the destination showing
// the wrong key, or dead on arrival, or leaves a ghost behind in the source.
class AShortcutCanBeMovedBetweenContextsTest : public QObject {
    Q_OBJECT

    static constexpr const char *kKey = "Ctrl+Alt+Y";       // not a system default
    static constexpr const char *kChosen = "Ctrl+Alt+Z";    // sorts after kKey
    static constexpr const char *kAction = "sortByName";
    static constexpr const char *kOtherAction = "zoomIn";

private slots:
    void movingABindingRetargetsItAndLeavesNothingBehind();
    void copyingABindingKeepsTheOriginalAndRevivesTheDestination();
    void aTakenKeyInTheDestinationIsReportedBeforeTheTransfer();
    void theShortcutsTableOffersARowContextMenu();
};

// Move: the key leaves the source context entirely, and so do the source's
// primary-key memory and disabled flag for that action.
void AShortcutCanBeMovedBetweenContextsTest::movingABindingRetargetsItAndLeavesNothingBehind() {
    actionManager->removeAllShortcuts();
    // Two keys, with the second explicitly chosen as primary: that choice is a
    // separate per-context structure and has to travel with the keys.
    actionManager->addShortcut(MODE_FOLDERVIEW, kKey, kAction);
    actionManager->addShortcut(MODE_FOLDERVIEW, kChosen, kAction);

    SettingsDialog dialog;
    // The key editor records an explicit primary choice; without one,
    // primaryShortcut() just falls back to the first candidate and the
    // migration of that choice would be untestable.
    dialog.setPrimaryShortcut(MODE_FOLDERVIEW, kAction, kChosen);
    QCOMPARE(dialog.candidateShortcuts(MODE_FOLDERVIEW, kAction), (QStringList{kKey, kChosen}));
    QCOMPARE(dialog.primaryShortcut(MODE_FOLDERVIEW, kAction), QString(kChosen));

    dialog.applyShortcutTransfer(kAction, MODE_FOLDERVIEW, MODE_GLOBAL, true);

    // Arrived, with the primary choice intact - the table's Key column reads off
    // primaryShortcut(), so losing it would silently show kKey instead.
    QCOMPARE(dialog.candidateShortcuts(MODE_GLOBAL, kAction), (QStringList{kKey, kChosen}));
    QCOMPARE(dialog.primaryShortcut(MODE_GLOBAL, kAction), QString(kChosen));
    QVERIFY2(dialog.shortcutEnabled(MODE_GLOBAL, kAction),
             "A moved binding must be live in its new context.");

    // Gone from the source.
    QVERIFY2(dialog.candidateShortcuts(MODE_FOLDERVIEW, kAction).isEmpty(),
             "A move must not leave the key behind in the source context.");

    // The source's primary memory must go too, and that is observable: switching
    // the action off and back on in the source re-adds the remembered primary
    // key, so a stale entry would resurrect the key that was just moved away.
    dialog.setShortcutEnabled(MODE_FOLDERVIEW, kAction, false);
    dialog.setShortcutEnabled(MODE_FOLDERVIEW, kAction, true);
    QVERIFY2(!dialog.candidateShortcuts(MODE_FOLDERVIEW, kAction).contains(QString(kChosen)),
             "Re-enabling the action in the source must not bring the moved key back.");

    // And the table now shows the binding under Global, not Grid.
    QTableWidget *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY2(table, "The shortcuts page should have its table.");
    bool found = false;
    for(int row = 0; row < table->rowCount(); row++) {
        QTableWidgetItem *item = table->item(row, 0);
        if(item && item->data(Qt::UserRole).toString() == QString(kAction)) {
            found = true;
            QCOMPARE(table->item(row, 1)->text(), QString(kChosen));    // Global is the shown context
        }
    }
    QVERIFY2(found, "The moved action should still have a row in the Global context.");
}

// Copy: the source keeps its binding, and the destination is switched back on
// so the copy is not dead on arrival.
void AShortcutCanBeMovedBetweenContextsTest::copyingABindingKeepsTheOriginalAndRevivesTheDestination() {
    actionManager->removeAllShortcuts();
    actionManager->addShortcut(MODE_GLOBAL, kKey, kAction);

    SettingsDialog dialog;

    // The destination has this action explicitly switched off.
    dialog.setShortcutEnabled(MODE_FOLDERVIEW, kAction, false);
    QVERIFY(!dialog.shortcutEnabled(MODE_FOLDERVIEW, kAction));

    dialog.applyShortcutTransfer(kAction, MODE_GLOBAL, MODE_FOLDERVIEW, false);

    QCOMPARE(dialog.candidateShortcuts(MODE_FOLDERVIEW, kAction), QStringList{kKey});
    QCOMPARE(dialog.primaryShortcut(MODE_FOLDERVIEW, kAction), QString(kKey));
    QVERIFY2(dialog.shortcutEnabled(MODE_FOLDERVIEW, kAction),
             "Copying into a context where the action was disabled must re-enable it, "
             "otherwise the new binding never fires.");

    // The original is untouched: that is what makes this a copy.
    QCOMPARE(dialog.candidateShortcuts(MODE_GLOBAL, kAction), QStringList{kKey});
    QVERIFY(dialog.shortcutEnabled(MODE_GLOBAL, kAction));
}

// Collision: the destination already gave this key to a different action, and
// the user has to be told before the transfer takes it over.
void AShortcutCanBeMovedBetweenContextsTest::aTakenKeyInTheDestinationIsReportedBeforeTheTransfer() {
    actionManager->removeAllShortcuts();
    actionManager->addShortcut(MODE_GLOBAL, kKey, kAction);
    actionManager->addShortcut(MODE_FOLDERVIEW, kKey, kOtherAction);

    SettingsDialog dialog;

    const QMap<QString, QString> clashes =
        dialog.shortcutTransferClashes(kAction, MODE_GLOBAL, MODE_FOLDERVIEW);
    QCOMPARE(clashes.size(), 1);
    QCOMPARE(clashes.value(kKey), QString(kOtherAction));

    // A destination that only resolves the key through this very action's own
    // global binding is not a clash - there is nothing to warn about.
    QVERIFY2(dialog.shortcutTransferClashes(kAction, MODE_GLOBAL, MODE_DOCUMENT).isEmpty(),
             "The action's own global binding must not be reported as a conflict.");

    // Confirming takes the key over: one key maps to one action per context.
    dialog.applyShortcutTransfer(kAction, MODE_GLOBAL, MODE_FOLDERVIEW, false);
    QCOMPARE(dialog.candidateShortcuts(MODE_FOLDERVIEW, kAction), QStringList{kKey});
    QVERIFY2(dialog.candidateShortcuts(MODE_FOLDERVIEW, kOtherAction).isEmpty(),
             "The displaced action must lose the key it no longer owns.");
}

void AShortcutCanBeMovedBetweenContextsTest::theShortcutsTableOffersARowContextMenu() {
    SettingsDialog dialog;
    QTableWidget *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY2(table, "The shortcuts page should have its table.");
    // Move to.../Copy to... are row commands; without a custom policy there is
    // no affordance to reach them at all.
    QCOMPARE(table->contextMenuPolicy(), Qt::CustomContextMenu);
}

TG_BEHAVIOR_TEST_MAIN(AShortcutCanBeMovedBetweenContextsTest)

#include "test_a_shortcut_can_be_moved_between_contexts.moc"
