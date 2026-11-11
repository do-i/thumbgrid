#include "support/thumbgrid_test_support.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>

#include "core.h"
#include "gui/dialogs/settingsdialog.h"
#include "gui/mainwindow.h"

// shortcuts.json holds every preset binding: one missing from the file comes back at startup,
// unless it is listed in _unbound (removed on purpose) or _disabled. A binding whose preset keys
// are all taken is recorded as unbound, and any unbound binding is announced after start.

namespace {

constexpr const char *kKey = "LMB_Drag";
constexpr const char *kAction = "dragFileOut";

QJsonObject shortcutsJson() {
    QFile file(QFileInfo(QSettings().fileName()).absolutePath() + "/shortcuts.json");
    if(!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QStringList unboundInGrid() {
    return actionManager->unboundActions().value(MODE_FOLDERVIEW);
}

// the preset's grid drag-out binding, as if it had been lost from shortcuts.json
void loseGridDragOut() {
    actionManager->applyPreset(actionManager->selectedPreset());
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, kKey), QString(kAction));
    actionManager->removeShortcut(MODE_FOLDERVIEW, kKey);
    actionManager->saveShortcuts();
}

} // namespace

class MissingPresetBindingsAreRestoredUnlessUnboundTest : public QObject {
    Q_OBJECT

private slots:
    void aLostBindingIsRestored();
    void anUnboundBindingStaysUnbound();
    void aDisabledBindingIsNeitherRestoredNorUnbound();
    void aBindingWhosePresetKeyIsTakenIsRecordedUnbound();
    void removingInSettingsRecordsUnbound();
    void applyingAPresetClearsUnbound();
    void unboundBindingsAreAnnouncedAfterStart();
};

void MissingPresetBindingsAreRestoredUnlessUnboundTest::aLostBindingIsRestored() {
    loseGridDragOut();
    const QStringList restored = actionManager->restoreMissingPresetBindings();
    QVERIFY2(restored.contains(QStringLiteral("grid: dragFileOut [LMB_Drag]")), qPrintable(restored.join(" | ")));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, kKey), QString(kAction));
    QCOMPARE(actionManager->unboundActionCount(), 0);
    QCOMPARE(shortcutsJson().value("grid").toObject().value(kAction).toArray().at(0).toString(), QString(kKey));
    QVERIFY2(actionManager->restoreMissingPresetBindings().isEmpty(), "A complete file restores nothing.");
}

void MissingPresetBindingsAreRestoredUnlessUnboundTest::anUnboundBindingStaysUnbound() {
    loseGridDragOut();
    actionManager->recordUnboundPresetActions({});
    QCOMPARE(unboundInGrid(), QStringList{kAction});
    QCOMPARE(shortcutsJson().value("_unbound").toObject().value("grid").toArray().at(0).toString(), QString(kAction));

    QVERIFY(actionManager->restoreMissingPresetBindings().isEmpty());
    QVERIFY(actionManager->actionForShortcut(MODE_FOLDERVIEW, kKey).isEmpty());
    QCOMPARE(actionManager->unboundActionCount(), 1);

    // binding it again (here by hand) clears the record
    actionManager->addShortcut(MODE_FOLDERVIEW, "Ctrl+Alt+Y", kAction);
    actionManager->restoreMissingPresetBindings();
    QCOMPARE(actionManager->unboundActionCount(), 0);
    QVERIFY(shortcutsJson().value("_unbound").toObject().value("grid").toArray().isEmpty());
}

void MissingPresetBindingsAreRestoredUnlessUnboundTest::aDisabledBindingIsNeitherRestoredNorUnbound() {
    loseGridDragOut();
    QMap<ViewMode, QStringList> disabled;
    disabled[MODE_FOLDERVIEW] = QStringList{kAction};
    settings->saveDisabledShortcuts(disabled);
    QVERIFY(actionManager->restoreMissingPresetBindings().isEmpty());
    QVERIFY(actionManager->actionForShortcut(MODE_FOLDERVIEW, kKey).isEmpty());
    QCOMPARE(actionManager->unboundActionCount(), 0);
}

void MissingPresetBindingsAreRestoredUnlessUnboundTest::aBindingWhosePresetKeyIsTakenIsRecordedUnbound() {
    loseGridDragOut();
    actionManager->addShortcut(MODE_FOLDERVIEW, kKey, "selectAll");
    actionManager->saveShortcuts();
    const QStringList restored = actionManager->restoreMissingPresetBindings();
    QVERIFY2(!restored.join(" ").contains(kAction), qPrintable(restored.join(" | ")));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, kKey), QStringLiteral("selectAll"));
    QCOMPARE(unboundInGrid(), QStringList{kAction});
}

void MissingPresetBindingsAreRestoredUnlessUnboundTest::removingInSettingsRecordsUnbound() {
    actionManager->applyPreset(actionManager->selectedPreset());
    SettingsDialog dialog;
    auto *table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY(table);
    int row = -1;
    for(int r = 0; r < table->rowCount() && row < 0; r++) {
        if(dialog.shortcutActionAtRow(r) == QString(kAction) && dialog.shortcutContextAtRow(r) == MODE_FOLDERVIEW)
            row = r;
    }
    QVERIFY2(row >= 0, "The grid drag-out binding should have a row.");
    table->setCurrentCell(row, 0);
    QVERIFY(QMetaObject::invokeMethod(&dialog, "removeShortcut"));
    dialog.findChild<QPushButton *>("applyButton")->click();

    QVERIFY(actionManager->actionForShortcut(MODE_FOLDERVIEW, kKey).isEmpty());
    QCOMPARE(unboundInGrid(), QStringList{kAction});
    QVERIFY2(actionManager->restoreMissingPresetBindings().isEmpty(), "A removed binding must not come back.");
}

void MissingPresetBindingsAreRestoredUnlessUnboundTest::applyingAPresetClearsUnbound() {
    loseGridDragOut();
    actionManager->recordUnboundPresetActions({});
    QCOMPARE(actionManager->unboundActionCount(), 1);
    actionManager->applyPreset(actionManager->selectedPreset());
    QCOMPARE(actionManager->unboundActionCount(), 0);
    QVERIFY(shortcutsJson().value("_unbound").toObject().isEmpty());
}

void MissingPresetBindingsAreRestoredUnlessUnboundTest::unboundBindingsAreAnnouncedAfterStart() {
    // an up-to-date install: no upgrade step runs that could bind the gesture again
    settings->setFirstRun(false);
    settings->setLastVersion(appVersion);
    loseGridDragOut();
    actionManager->recordUnboundPresetActions({});

    Core core;
    core.showGui();
    QTRY_VERIFY(tgtest::mainWindow() != nullptr);
    MW *window = tgtest::mainWindow();
    const auto noticeShown = [window] {
        for(QLabel *label : window->findChildren<QLabel *>()) {
            if(label->isVisible() && label->text().contains(QStringLiteral("1 shortcut(s) unbound")))
                return true;
        }
        return false;
    };
    // after the welcome/update message when one is shown first
    QTRY_VERIFY_WITH_TIMEOUT(noticeShown(), 8000);
}

TG_BEHAVIOR_TEST_MAIN(MissingPresetBindingsAreRestoredUnlessUnboundTest)

#include "test_missing_preset_bindings_are_restored_unless_unbound.moc"
