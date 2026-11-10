#include "support/thumbgrid_test_support.h"

#include "constants.h"
#include "gui/dialogs/settingsdialog.h"

#include <QPushButton>
#include <QSettings>
#include <QSpinBox>

// The idle pointer hides after one second unless the user sets another delay in
// Settings; stored values outside the allowed range are clamped.
class CursorHideDelayTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void hidesAfterOneSecondByDefault();
    void pickedDelayRoundTripsAndClamps();
    void settingsDialogSavesTheDelay();
};

using namespace tg::bounds;

void CursorHideDelayTest::init() {
    QSettings conf;
    conf.remove("cursorHideDelay");
    conf.sync();
}

void CursorHideDelayTest::hidesAfterOneSecondByDefault() {
    QCOMPARE(settings->cursorHideDelay(), 1000);
    QCOMPARE(kCursorHideDelayDefaultMs, 1000);
}

void CursorHideDelayTest::pickedDelayRoundTripsAndClamps() {
    QSettings conf;
    conf.setValue("cursorHideDelay", 999999);
    conf.sync();
    QCOMPARE(settings->cursorHideDelay(), kCursorHideDelayMaxMs);
    conf.setValue("cursorHideDelay", 5);
    conf.sync();
    QCOMPARE(settings->cursorHideDelay(), kCursorHideDelayMinMs);
    settings->setCursorHideDelay(2500);
    QCOMPARE(settings->cursorHideDelay(), 2500);
    settings->setCursorHideDelay(5);
    QCOMPARE(settings->cursorHideDelay(), kCursorHideDelayMinMs);
}

void CursorHideDelayTest::settingsDialogSavesTheDelay() {
    settings->setCursorHideDelay(2500);
    SettingsDialog dialog;
    auto spin = dialog.findChild<QSpinBox *>("cursorHideDelaySpinBox");
    QVERIFY(spin);
    QCOMPARE(spin->minimum(), kCursorHideDelayMinMs);
    QCOMPARE(spin->maximum(), kCursorHideDelayMaxMs);
    QCOMPARE(spin->value(), 2500);
    spin->setValue(4000);
    dialog.findChild<QPushButton *>("applyButton")->click();
    QCOMPARE(settings->cursorHideDelay(), 4000);
}

TG_BEHAVIOR_TEST_MAIN(CursorHideDelayTest)

#include "test_cursor_hide_delay.moc"
