#include "support/thumbgrid_test_support.h"

#include "gui/dialogs/settingsdialog.h"

#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>

// What a scroll action does when there is nothing to scroll (Left on a picture
// that fits: the previous file) used to be fixed in code. The action's key
// dialog now has a "When there is nothing to scroll" choice, which Apply saves.

class ScrollFallbackIsEditableInSettingsTest : public QObject {
    Q_OBJECT

private slots:
    void theKeyDialogEditsTheFallback();
};

namespace {

constexpr int kActionColumn = 1;
constexpr int kKeyColumn = 2;

int rowFor(const SettingsDialog &dialog, const QTableWidget *table, const QString &action, ViewMode context) {
    for(int row = 0; row < table->rowCount(); row++) {
        if(table->item(row, kActionColumn)->data(Qt::UserRole).toString() == action &&
           dialog.shortcutContextAtRow(row) == context)
            return row;
    }
    return -1;
}

// Opens the action's key dialog the way a click on its Key cell does, and hands
// the open dialog's fallback combo box to edit before pressing OK there.
void editKeyDialog(QTableWidget *table, int row, const std::function<void(QComboBox *)> &edit) {
    QTimer::singleShot(0, [edit]() {
        QWidget *keyDialog = QApplication::activeModalWidget();
        QVERIFY(keyDialog && keyDialog->objectName() == QLatin1String("ShortcutDetailsDialog"));
        const QString shots = qEnvironmentVariable("TG_SHOT_DIR");
        static int shot = 0;
        if(!shots.isEmpty())
            keyDialog->grab().save(shots + QString("/scroll-fallback-dialog-%1.png").arg(++shot));
        edit(keyDialog->findChild<QComboBox *>("shortcutFallbackComboBox"));
        for(auto button : keyDialog->findChildren<QPushButton *>()) {
            if(button->text() == QObject::tr("OK"))
                button->click();
        }
    });
    emit table->cellClicked(row, kKeyColumn);
}

} // namespace

void ScrollFallbackIsEditableInSettingsTest::theKeyDialogEditsTheFallback() {
    SettingsDialog dialog;
    dialog.show();
    auto table = dialog.findChild<QTableWidget *>("shortcutsTableWidget");
    QVERIFY(table);
    const int row = rowFor(dialog, table, "scrollLeft", MODE_DOCUMENT);
    QVERIFY2(row >= 0, "scrollLeft should have a document row.");

    QString shown;
    editKeyDialog(table, row, [&](QComboBox *fallback) {
        QVERIFY2(fallback, "A scroll action's key dialog should offer its fallback.");
        shown = fallback->currentText();
        fallback->setCurrentIndex(fallback->findData(QString()));    // Nothing
    });
    QCOMPARE(shown, QStringLiteral("prevImage (default)"));
    QCOMPARE(actionManager->scrollFallback("scrollLeft"), QStringLiteral("prevImage"));    // draft only

    dialog.findChild<QPushButton *>("applyButton")->click();
    QVERIFY2(actionManager->scrollFallback("scrollLeft").isEmpty(), "Apply should save the new fallback.");

    // Reopened, the dialog shows the saved choice; other actions have no picker.
    editKeyDialog(table, rowFor(dialog, table, "scrollLeft", MODE_DOCUMENT), [&](QComboBox *fallback) {
        shown = fallback ? fallback->currentText() : QString();
    });
    QCOMPARE(shown, QStringLiteral("Nothing"));
    bool hasPicker = true;
    editKeyDialog(table, rowFor(dialog, table, "nextImage", MODE_DOCUMENT), [&](QComboBox *fallback) {
        hasPicker = fallback != nullptr;
    });
    QVERIFY2(!hasPicker, "Only scroll actions have a fallback.");
}

TG_BEHAVIOR_TEST_MAIN(ScrollFallbackIsEditableInSettingsTest)

#include "test_scroll_fallback_is_editable_in_settings.moc"
