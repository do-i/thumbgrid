#include "support/thumbgrid_test_support.h"

#include "gui/dialogs/settingsdialog.h"
#include "gui/dialogs/shortcutcreatordialog.h"

#include <QComboBox>
#include <QKeyEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>

// A user script is a first-class action ("s:<name>"): it can be bound to a key
// like any built-in action, pressing that key runs it, and every script is
// listed on the shortcuts page so it can be bound there in the first place.
class AScriptCanBeBoundToAShortcutTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void pressingTheBoundKeyRunsTheScript();
    void unboundScriptsAreListedOnTheShortcutsPage();
    void theShortcutsPageOffersAnEntryPointForNewBindings();
    void theShortcutCreatorCanProduceAScriptAction();
    // Deletes the shared script - must stay the last test in this binary.
    void deletingAScriptLeavesNoGhostRow();
};

void AScriptCanBeBoundToAShortcutTest::initTestCase() {
    scriptManager->addScript(QStringLiteral("open externally"),
                             Script(QStringLiteral("xdg-open %file%"), false));
}

void AScriptCanBeBoundToAShortcutTest::pressingTheBoundKeyRunsTheScript() {
    actionManager->removeAllShortcuts();
    actionManager->addShortcut(MODE_GLOBAL, "C", "s:open externally");

    QCOMPARE(actionManager->actionForShortcut(MODE_GLOBAL, "C"),
             QStringLiteral("s:open externally"));

    const quint32 scanCode = inputMap->keys().key("C");
    QVERIFY2(scanCode != 0, "Platform input map should know a scancode for the C key.");

    QSignalSpy runScriptSpy(actionManager, &ActionManager::runScript);
    QVERIFY(runScriptSpy.isValid());

    // The key resolves to the script action and dispatch strips the "s:" prefix
    // before asking the core to run it.
    actionManager->setContext(MODE_FOLDERVIEW);
    {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, scanCode, 0, 0, "c");
        QVERIFY2(actionManager->processEvent(&press), "C should be handled as a script shortcut.");
    }
    QCOMPARE(runScriptSpy.count(), 1);
    QCOMPARE(runScriptSpy.takeFirst().at(0).toString(), QStringLiteral("open externally"));

    // Global bindings fire in the document context too.
    actionManager->setContext(MODE_DOCUMENT);
    {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, scanCode, 0, 0, "c");
        QVERIFY(actionManager->processEvent(&press));
    }
    QCOMPARE(runScriptSpy.count(), 1);
}

void AScriptCanBeBoundToAShortcutTest::unboundScriptsAreListedOnTheShortcutsPage() {
    // No binding at all: the script must still show up, otherwise there is no
    // row to click and no way to give it a key.
    actionManager->removeAllShortcuts();

    SettingsDialog dialog;
    QTableWidget *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY2(table, "The shortcuts page should have its table.");

    int scriptRow = -1;
    for(int row = 0; row < table->rowCount(); row++) {
        QTableWidgetItem *item = table->item(row, 0);
        if(item && item->data(Qt::UserRole).toString() == QStringLiteral("s:open externally")) {
            scriptRow = row;
            break;
        }
    }
    QVERIFY2(scriptRow != -1, "An unbound script should still get a row in the Global context.");

    // Presentation: the raw "s:" id is never shown, but it stays in UserRole
    // because every downstream lookup keys off it.
    const QString label = table->item(scriptRow, 0)->text();
    QVERIFY2(!label.startsWith(QStringLiteral("s:")), "The 's:' prefix should not be displayed.");
    QVERIFY2(label.contains(QStringLiteral("open externally")), "The script name should be shown.");
    QVERIFY2(label.contains(QStringLiteral("script")), "Script rows should be marked as scripts.");
    QVERIFY2(table->item(scriptRow, 1)->text().isEmpty(), "An unbound script has no key yet.");
}

void AScriptCanBeBoundToAShortcutTest::theShortcutsPageOffersAnEntryPointForNewBindings() {
    SettingsDialog dialog;
    QPushButton *button = dialog.findChild<QPushButton *>("newShortcutButton");
    QVERIFY2(button, "The shortcuts page should offer a 'new shortcut' button.");
    QVERIFY2(!button->isHidden(), "The 'new shortcut' button must be visible.");
    // Carries the shared dialog-button style marker so it is themed, not native.
    QCOMPARE(button->accessibleName(), QStringLiteral("SDialogButton"));
}

void AScriptCanBeBoundToAShortcutTest::theShortcutCreatorCanProduceAScriptAction() {
    ShortcutCreatorDialog creator;
    QComboBox *scriptCombo = creator.findChild<QComboBox *>("scriptsComboBox");
    QVERIFY2(scriptCombo, "The shortcut creator should list scripts.");
    QVERIFY2(scriptCombo->findText(QStringLiteral("open externally")) != -1,
             "A registered script should be offered by the shortcut creator.");
}

// Deleting a script must leave nothing behind in the persisted shortcut state.
// The dangerous leftover is the per-context disabled flag: the shortcuts table
// seeds rows from it, so a stale entry resurrects the deleted script as a
// ghost "(script)" row that can never run.
void AScriptCanBeBoundToAShortcutTest::deletingAScriptLeavesNoGhostRow() {
    const QString action = QStringLiteral("s:open externally");
    actionManager->removeAllShortcuts();
    {
        SettingsDialog dialog;
        // The worst-case leftover: the script's row is switched off, which
        // clears its draft keys and records it in the disabled list.
        dialog.setShortcutEnabled(MODE_GLOBAL, action, false);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "removeScript",
                                          Q_ARG(QString, QStringLiteral("open externally"))));
    }
    QVERIFY(!scriptManager->scriptExists(QStringLiteral("open externally")));

    // A fresh dialog re-reads the persisted state; the deleted script must not
    // resurface from the disabled list (or anywhere else).
    SettingsDialog dialog;
    QTableWidget *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY2(table, "The shortcuts page should have its table.");
    for(int row = 0; row < table->rowCount(); row++)
        QVERIFY2(table->item(row, 0)->data(Qt::UserRole).toString() != action,
                 "A deleted script must not keep a row on the shortcuts page.");
}

TG_BEHAVIOR_TEST_MAIN(AScriptCanBeBoundToAShortcutTest)

#include "test_a_script_can_be_bound_to_a_shortcut.moc"
