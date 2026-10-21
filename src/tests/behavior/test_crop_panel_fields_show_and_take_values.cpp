#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QLineEdit>
#include <QSpinBox>
#include <QTemporaryDir>

#include "core.h"
#include "gui/mainwindow.h"
#include "gui/panels/croppanel/croppanel.h"

// The crop panel's number fields show the selection (starting as the whole
// image) and take typed values. Under Qt 6.12 a style-sheeted NoButtons spin
// box got a 1px-wide edit field, so the fields looked empty and ignored typing.

class CropPanelFieldsTest : public QObject {
    Q_OBJECT

private slots:
    void fieldsShowTheSelectionAndTakeTypedValues();
};

namespace {

// SpinBoxInputFix accepts keys by scancode name (QTest key events carry none),
// so press keys with the scancode the keymap assigns to that key name.
void pressNamedKey(QWidget *target, const QString &name, Qt::Key key, const QString &text = QString()) {
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, inputMap->keys().key(name), 0, 0, text);
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, inputMap->keys().key(name), 0, 0, text);
    QApplication::sendEvent(target, &release);
    QCoreApplication::processEvents();
}

} // namespace

void CropPanelFieldsTest::fieldsShowTheSelectionAndTakeTypedValues() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    {
        QImage image(640, 480, QImage::Format_RGB32);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(root.filePath("gallery/a.png"), "PNG"));
    }

    Core core;
    QVERIFY(core.loadPath(root.filePath("gallery/a.png")));
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    // the crop panel is built by the deferred full-UI setup
    QTRY_VERIFY2(window->findChild<CropPanel *>() != nullptr, "The crop panel should be created.");
    auto *panel = window->findChild<CropPanel *>();
    actionManager->invokeAction("crop");
    QTRY_VERIFY2(panel->isVisible(), "The crop panel should open.");
    auto *width = panel->findChild<QSpinBox *>("width");
    auto *height = panel->findChild<QSpinBox *>("height");
    auto *posX = panel->findChild<QSpinBox *>("posX");
    QVERIFY(width && height && posX);
    QTest::qWait(100);

    const QString shot = qEnvironmentVariable("TG_CROP_PANEL_SHOT");
    if(!shot.isEmpty())
        panel->window()->grab().save(shot);

    QCOMPARE(width->value(), 640);
    QCOMPARE(height->value(), 480);
    QCOMPARE(width->text(), QStringLiteral("640"));
    for(QSpinBox *field : {width, height, posX}) {
        auto *edit = field->findChild<QLineEdit *>();
        QVERIFY(edit);
        QVERIFY2(edit->width() > field->width() / 2,
                 qPrintable(QStringLiteral("%1's edit field is only %2px wide")
                                .arg(field->objectName()).arg(edit->width())));
    }

    width->setFocus();
    width->selectAll();
    pressNamedKey(width, "3", Qt::Key_3, "3");
    pressNamedKey(width, "0", Qt::Key_0, "0");
    pressNamedKey(width, "0", Qt::Key_0, "0");
    QCOMPARE(width->text(), QStringLiteral("300"));
    // keyboard tracking is off: the value commits when the field loses focus
    height->setFocus();
    QCoreApplication::processEvents();
    QCOMPARE(width->value(), 300);
}

TG_BEHAVIOR_TEST_MAIN(CropPanelFieldsTest)

#include "test_crop_panel_fields_show_and_take_values.moc"
