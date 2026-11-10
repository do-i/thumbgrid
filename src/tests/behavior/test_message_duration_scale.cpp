#include "support/thumbgrid_test_support.h"

#include "constants.h"
#include "gui/dialogs/settingsdialog.h"
#include "gui/overlays/floatingmessage.h"

#include <QElapsedTimer>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>

// On-screen messages last as long as callers ask (scale 100%) until the user sets
// a duration scale in Settings; the scale stretches or shortens every message.
class MessageDurationScaleTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void scaleIsOneHundredPercentByDefault();
    void pickedScaleRoundTripsAndClamps();
    void settingsDialogSavesTheScale();
    void messageStaysVisibleInProportionToTheScale();
};

using namespace tg::bounds;

namespace {

// ms from showing a message until it has faded out completely
qint64 visibleMs(int scalePct, int durationMs) {
    settings->setMessageDurationScale(scalePct);
    emit settings->settingsChanged();
    FloatingMessage message(nullptr);
    QElapsedTimer clock;
    clock.start();
    message.showMessage("hello", NO_ICON, durationMs);
    while(message.isVisible() && clock.elapsed() < 5000)
        QTest::qWait(10);
    return clock.elapsed();
}

} // namespace

void MessageDurationScaleTest::init() {
    QSettings conf;
    conf.remove("messageDurationScale");
    conf.sync();
}

void MessageDurationScaleTest::scaleIsOneHundredPercentByDefault() {
    QCOMPARE(settings->messageDurationScale(), 100);
}

void MessageDurationScaleTest::pickedScaleRoundTripsAndClamps() {
    QSettings conf;
    conf.setValue("messageDurationScale", 9999);
    conf.sync();
    QCOMPARE(settings->messageDurationScale(), kMessageDurationScaleMaxPct);
    conf.setValue("messageDurationScale", 1);
    conf.sync();
    QCOMPARE(settings->messageDurationScale(), kMessageDurationScaleMinPct);
    settings->setMessageDurationScale(150);
    QCOMPARE(settings->messageDurationScale(), 150);
}

void MessageDurationScaleTest::settingsDialogSavesTheScale() {
    settings->setMessageDurationScale(150);
    SettingsDialog dialog;
    auto spin = dialog.findChild<QSpinBox *>("messageDurationScaleSpinBox");
    QVERIFY(spin);
    QCOMPARE(spin->minimum(), kMessageDurationScaleMinPct);
    QCOMPARE(spin->maximum(), kMessageDurationScaleMaxPct);
    QCOMPARE(spin->value(), 150);
    spin->setValue(200);
    dialog.findChild<QPushButton *>("applyButton")->click();
    QCOMPARE(settings->messageDurationScale(), 200);
}

void MessageDurationScaleTest::messageStaysVisibleInProportionToTheScale() {
    const qint64 shortMs = visibleMs(50, 400);
    const qint64 longMs = visibleMs(250, 400);
    // the hide timer differs by 800 ms (200 vs 1000); allow generous slack
    QVERIFY2(longMs > shortMs + 500, qPrintable(QString("short %1 ms, long %2 ms").arg(shortMs).arg(longMs)));
}

TG_BEHAVIOR_TEST_MAIN(MessageDurationScaleTest)

#include "test_message_duration_scale.moc"
