#include "support/thumbgrid_test_support.h"

#include "gui/dialogs/settingsdialog.h"

#include <QTableWidget>

// Space toggles the Enabled checkbox of the current row in the shortcuts table,
// whichever of the row's cells is current (rows are selected whole), and the row
// stays current after the toggle so Space can switch it straight back.

class ShortcutsTableSpaceTogglesEnabledTest : public QObject {
    Q_OBJECT

private slots:
    void spaceTogglesTheCurrentRowAndKeepsItCurrent();
};

namespace {

constexpr int kActionColumn = 1;
constexpr int kEnabledColumn = 4;

int rowFor(const SettingsDialog &dialog, const QTableWidget *table, const QString &action, ViewMode context) {
    for(int row = 0; row < table->rowCount(); row++) {
        if(table->item(row, kActionColumn)->data(Qt::UserRole).toString() == action &&
           dialog.shortcutContextAtRow(row) == context)
            return row;
    }
    return -1;
}

Qt::CheckState enabledState(const QTableWidget *table, int row) {
    return table->item(row, kEnabledColumn)->checkState();
}

} // namespace

void ShortcutsTableSpaceTogglesEnabledTest::spaceTogglesTheCurrentRowAndKeepsItCurrent() {
    SettingsDialog dialog;
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY(table);
    QVERIFY(table->rowCount() > 0);

    int row = 0;
    for(; row < table->rowCount() && enabledState(table, row) != Qt::Checked; row++) {}
    QVERIFY2(row < table->rowCount(), "Some binding should start enabled.");
    const QString action = table->item(row, kActionColumn)->data(Qt::UserRole).toString();
    const ViewMode context = dialog.shortcutContextAtRow(row);

    table->setFocus();
    table->setCurrentCell(row, kActionColumn);
    QTest::keyClick(table, Qt::Key_Space);
    row = rowFor(dialog, table, action, context);
    QVERIFY(row != -1);
    QCOMPARE(enabledState(table, row), Qt::Unchecked);
    QCOMPARE(table->currentRow(), row);

    QTest::keyClick(table, Qt::Key_Space);
    row = rowFor(dialog, table, action, context);
    QVERIFY(row != -1);
    QCOMPARE(enabledState(table, row), Qt::Checked);
    QCOMPARE(table->currentRow(), row);
}

TG_BEHAVIOR_TEST_MAIN(ShortcutsTableSpaceTogglesEnabledTest)

#include "test_shortcuts_table_space_toggles_enabled.moc"
