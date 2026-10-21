#include "support/thumbgrid_test_support.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpontaneKeyEvent>
#include <QWheelEvent>

#include "gui/customwidgets/keysequenceedit.h"

// The shortcut capture button records whatever is pressed - including Tab,
// Enter and Esc, which a dialog would otherwise take for focus, accept and
// close - plus clicks and the wheel. The click that focuses the button only
// starts recording: it used to be recorded as an LMB binding itself.

class ShortcutCaptureTest : public QObject {
    Q_OBJECT
private slots:
    void recordsKeysClicksAndWheelButNotTheFocusingClick();
};

namespace {

// Keys are named by native scancode (QTest key events carry none).
void pressNamedKey(QWidget *target, const QString &name, Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QKeyEvent press(QEvent::KeyPress, key, mods, inputMap->keys().key(name), 0, 0);
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, mods, inputMap->keys().key(name), 0, 0);
    QApplication::sendEvent(target, &release);
    QCoreApplication::processEvents();
}

void wheelNotch(QWidget *target, int angleY) {
    const QPointF local = QRectF(target->rect()).center();
    QWheelEvent wheel(local, target->mapToGlobal(local), QPoint(), QPoint(0, angleY), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QSpontaneKeyEvent::setSpontaneous(&wheel);
    qApp->notify(target, &wheel);
}

} // namespace

void ShortcutCaptureTest::recordsKeysClicksAndWheelButNotTheFocusingClick() {
    // the shape of the key details dialog: the capture button next to a default OK
    QDialog dialog;
    auto *layout = new QHBoxLayout(&dialog);
    auto *edit = new KeySequenceEdit(&dialog);
    auto *ok = new QPushButton("OK", &dialog);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(edit);
    layout->addWidget(ok);
    QStringList recorded;
    connect(edit, &KeySequenceEdit::edited, this, [&]() { recorded << edit->sequence(); });
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QVERIFY(QApplication::focusWidget() != edit);

    QTest::mouseClick(edit, Qt::LeftButton);
    QCOMPARE(QApplication::focusWidget(), edit);
    QVERIFY2(recorded.isEmpty(), "The click that starts recording must not become a binding.");

    pressNamedKey(edit, "Tab", Qt::Key_Tab);
    pressNamedKey(edit, "Tab", Qt::Key_Backtab, Qt::ShiftModifier);
    pressNamedKey(edit, "Enter", Qt::Key_Return);
    pressNamedKey(edit, "K", Qt::Key_K, Qt::ControlModifier);
    pressNamedKey(edit, "Esc", Qt::Key_Escape);
    wheelNotch(edit, 120);
    QTest::mouseClick(edit, Qt::LeftButton);
    QTest::mouseClick(edit, Qt::RightButton);
    QCOMPARE(recorded, QStringList({"Tab", "Shift+Tab", "Enter", "Ctrl+K", "Esc", "WheelUp", "LMB", "RMB"}));
    QCOMPARE(edit->text(), QStringLiteral("RMB"));
    QVERIFY2(dialog.isVisible() && dialog.result() == 0, "Enter and Esc must not accept or close the dialog.");
    QCOMPARE(QApplication::focusWidget(), edit);

    QTest::mouseDClick(edit, Qt::LeftButton);
    QCOMPARE(recorded.last(), QStringLiteral("LMB_DoubleClick"));

    // a right click can start recording too, and is not recorded either
    ok->setFocus();
    recorded.clear();
    QTest::mouseClick(edit, Qt::RightButton);
    QCOMPARE(QApplication::focusWidget(), edit);
    QVERIFY2(recorded.isEmpty(), "A right click that starts recording must not become a binding.");
}

TG_BEHAVIOR_TEST_MAIN(ShortcutCaptureTest)

#include "test_shortcut_capture_records_any_input.moc"
