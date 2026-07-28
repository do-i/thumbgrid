#include "support/thumbgrid_test_support.h"

#include "gui/dialogs/settingsdialog.h"

#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>

// The shortcuts page used to show one context at a time behind a Global/Grid/
// Document dropdown, which hid two thirds of the bindings and made "where does
// this key work?" a question about a control above the table. Now every context
// is listed together and each row states its own context with an icon, so the
// answer is on the row - and everything that acts on a row (the enabled
// checkbox, the key editor, Move to.../Copy to...) has to take the context from
// that row rather than from a filter.
class TheShortcutsListShowsEveryContextAtOnceTest : public QObject {
    Q_OBJECT

private slots:
    void everyContextIsListedTogether();
    void eachRowCarriesItsContextIcon();
    void searchStandsInForTheRemovedContextFilter();
    void togglingARowActsOnThatRowsContext();

private:
    // First row belonging to `context`, or -1. The actions each context ships
    // with are the default keymap's business, so the tests below take whatever
    // is actually listed rather than naming actions that may move.
    static int firstRowIn(const SettingsDialog &dialog, const QTableWidget *table, ViewMode context) {
        for(int row = 0; row < table->rowCount(); row++) {
            if(dialog.shortcutContextAtRow(row) == context)
                return row;
        }
        return -1;
    }
};

// The point of the redesign: one list, all three contexts.
void TheShortcutsListShowsEveryContextAtOnceTest::everyContextIsListedTogether() {
    SettingsDialog dialog;
    QTableWidget *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY2(table, "The shortcuts page should have its table.");

    QSet<ViewMode> contexts;
    for(int row = 0; row < table->rowCount(); row++)
        contexts.insert(dialog.shortcutContextAtRow(row));

    QVERIFY2(contexts.contains(MODE_GLOBAL), "Global bindings should be listed.");
    QVERIFY2(contexts.contains(MODE_FOLDERVIEW), "Grid bindings should be listed.");
    QVERIFY2(contexts.contains(MODE_DOCUMENT), "Document bindings should be listed.");

    // And the dropdown that used to gate them is gone: the only combo box left
    // on the page is the preset selector.
    for(QComboBox *combo : dialog.findChildren<QComboBox *>()) {
        const bool isContextFilter = combo->findText(QObject::tr("Grid")) != -1 &&
                                     combo->findText(QObject::tr("Document")) != -1;
        QVERIFY2(!isContextFilter, "The context dropdown should no longer exist.");
    }
}

// The context is shown as an icon, so the cell has no text - the name has to
// stay reachable as a tooltip or the icon is a guessing game.
void TheShortcutsListShowsEveryContextAtOnceTest::eachRowCarriesItsContextIcon() {
    SettingsDialog dialog;
    QTableWidget *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY2(table, "The shortcuts page should have its table.");
    QVERIFY2(table->columnCount() >= 5, "The table should have a context column next to the rest.");
    QVERIFY(table->rowCount() > 0);

    for(int row = 0; row < table->rowCount(); row++) {
        QTableWidgetItem *item = table->item(row, 0);
        QVERIFY2(item, "Every row needs a context cell.");
        QVERIFY2(!item->icon().isNull(), "The context column should show an icon.");
        QVERIFY2(!item->toolTip().isEmpty(), "The icon should name its context on hover.");
    }

    // Different contexts must not all get the same picture.
    const int gridRow = firstRowIn(dialog, table, MODE_FOLDERVIEW);
    const int documentRow = firstRowIn(dialog, table, MODE_DOCUMENT);
    QVERIFY(gridRow != -1 && documentRow != -1);
    QVERIFY2(table->item(gridRow, 0)->toolTip() != table->item(documentRow, 0)->toolTip(),
             "Each context should be labelled distinctly.");
    QVERIFY2(table->item(gridRow, 0)->icon().cacheKey() != table->item(documentRow, 0)->icon().cacheKey(),
             "Grid and Document rows should not share one icon.");
}

// Narrowing to one context was the dropdown's only real job; the search box
// takes it over, which needs the context name to be searchable even though the
// cell only draws an icon.
void TheShortcutsListShowsEveryContextAtOnceTest::searchStandsInForTheRemovedContextFilter() {
    SettingsDialog dialog;
    QTableWidget *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QLineEdit *search = nullptr;
    for(QLineEdit *edit : dialog.findChildren<QLineEdit *>()) {
        if(edit->placeholderText().contains(QStringLiteral("Search"), Qt::CaseInsensitive))
            search = edit;
    }
    QVERIFY2(search, "The shortcuts page should have a search box.");

    search->setText(QStringLiteral("document"));

    int visible = 0;
    for(int row = 0; row < table->rowCount(); row++) {
        if(table->isRowHidden(row))
            continue;
        visible++;
        QVERIFY2(dialog.shortcutContextAtRow(row) == MODE_DOCUMENT ||
                     table->item(row, 1)->text().contains(QStringLiteral("document"), Qt::CaseInsensitive),
                 "Searching for a context should leave only that context's rows (plus name matches).");
    }
    QVERIFY2(visible > 0, "Searching for a context should still show its rows.");
}

// The regression the redesign invites: a row command that still reads some
// single "current context" would switch the wrong binding off.
void TheShortcutsListShowsEveryContextAtOnceTest::togglingARowActsOnThatRowsContext() {
    SettingsDialog dialog;
    QTableWidget *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY2(table, "The shortcuts page should have its table.");

    const int row = firstRowIn(dialog, table, MODE_FOLDERVIEW);
    QVERIFY2(row != -1, "The grid context should have rows.");
    const QString action = dialog.shortcutActionAtRow(row);
    QVERIFY(dialog.shortcutEnabled(MODE_FOLDERVIEW, action));
    const bool globalBefore = dialog.shortcutEnabled(MODE_GLOBAL, action);

    table->item(row, 4)->setCheckState(Qt::Unchecked);

    QVERIFY2(!dialog.shortcutEnabled(MODE_FOLDERVIEW, action),
             "Unchecking the row must disable the binding in that row's context.");
    QCOMPARE(dialog.shortcutEnabled(MODE_GLOBAL, action), globalBefore);
}

TG_BEHAVIOR_TEST_MAIN(TheShortcutsListShowsEveryContextAtOnceTest)

#include "test_the_shortcuts_list_shows_every_context_at_once.moc"
