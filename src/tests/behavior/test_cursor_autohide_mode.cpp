#include "support/thumbgrid_test_support.h"

#include <QSettings>

#include "settings.h"

// The idle mouse pointer is hidden only in full screen unless the user picks
// Never or Always in Settings. The mode replaced an on/off "cursorAutohiding"
// (on by default): a config that had turned it off keeps never hiding, one
// that left it on gets the full-screen default.
class CursorAutohideModeTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void hidesOnlyInFullScreenByDefault();
    void turnedOffBeforeTheModeStaysNever();
    void leftOnBeforeTheModeHidesOnlyInFullScreen();
    void pickedModeRoundTrips();

private:
    // On Linux Settings uses a default QSettings(), so this one shares its store;
    // General keys are ungrouped there (QSettings puts them under [General]).
    static void writeGeneral(const QString &key, const QVariant &value);
};

void CursorAutohideModeTest::writeGeneral(const QString &key, const QVariant &value) {
    QSettings conf;
    if(value.isValid())
        conf.setValue(key, value);
    else
        conf.remove(key);
    conf.sync();
}

void CursorAutohideModeTest::init() {
    writeGeneral("cursorAutohideMode", QVariant());
    writeGeneral("cursorAutohiding", QVariant());
}

void CursorAutohideModeTest::hidesOnlyInFullScreenByDefault() {
    QCOMPARE(settings->cursorAutohide(), CURSOR_AUTOHIDE_FULLSCREEN);
}

void CursorAutohideModeTest::turnedOffBeforeTheModeStaysNever() {
    writeGeneral("cursorAutohiding", false);
    QCOMPARE(settings->cursorAutohide(), CURSOR_AUTOHIDE_NEVER);
}

void CursorAutohideModeTest::leftOnBeforeTheModeHidesOnlyInFullScreen() {
    writeGeneral("cursorAutohiding", true);
    QCOMPARE(settings->cursorAutohide(), CURSOR_AUTOHIDE_FULLSCREEN);
}

void CursorAutohideModeTest::pickedModeRoundTrips() {
    writeGeneral("cursorAutohiding", false);
    settings->setCursorAutohide(CURSOR_AUTOHIDE_ALWAYS);
    QCOMPARE(settings->cursorAutohide(), CURSOR_AUTOHIDE_ALWAYS);
    settings->setCursorAutohide(CURSOR_AUTOHIDE_NEVER);
    QCOMPARE(settings->cursorAutohide(), CURSOR_AUTOHIDE_NEVER);
}

TG_BEHAVIOR_TEST_MAIN(CursorAutohideModeTest)

#include "test_cursor_autohide_mode.moc"
