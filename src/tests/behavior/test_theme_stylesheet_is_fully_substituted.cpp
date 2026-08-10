#include "support/thumbgrid_test_support.h"

#include <QRegularExpression>

#include "gui/dialogs/custommessagebox.h"
#include "settings.h"
#include "themestore.h"

// style-template.qss is a token template: every %token% is replaced with a
// color or metric before the result reaches qApp->setStyleSheet(). A token with
// no replacement is not a local defect - Qt fails to parse the *whole* sheet
// ("Could not parse application stylesheet") and the entire app falls back to
// unstyled widgets. That is nearly invisible for widgets that also set colors
// in code, but fatal for the frameless, translucent dialogs (CustomMessageBox,
// FileReplaceDialog) whose only background comes from the stylesheet: they
// render as a black rectangle with unreadable text. This guards both halves -
// no token survives substitution, and a confirmation dialog really does paint
// the active theme's widget color.
class ThemeStylesheetIsFullySubstitutedTest : public QObject {
    Q_OBJECT

private slots:
    void everyTokenIsSubstitutedForEveryPreset();
    void aConfirmationDialogPaintsTheThemedBackground();
};

static const ColorSchemes presets[] = {
    COLORS_SYSTEM, COLORS_LIGHT, COLORS_BLACK,
    COLORS_DARK,   COLORS_DARKBLUE, COLORS_LIGHT_YELLOW,
};

void ThemeStylesheetIsFullySubstitutedTest::everyTokenIsSubstitutedForEveryPreset() {
    // Matches the %token% form used by the template. Anchored to the same
    // charset replaceStylesheetColors()/replaceStylesheetMetrics() use.
    static const QRegularExpression token(QStringLiteral("%[a-z_0-9]+%"));

    for(ColorSchemes tid : presets) {
        settings->setColorScheme(ThemeStore::colorScheme(tid));
        settings->loadStylesheet();

        const QString sheet = qApp->styleSheet();
        QVERIFY2(!sheet.isEmpty(),
                 qPrintable(QStringLiteral("theme %1 produced an empty stylesheet").arg(tid)));

        const QRegularExpressionMatch leftover = token.match(sheet);
        QVERIFY2(!leftover.hasMatch(),
                 qPrintable(QStringLiteral("theme %1 left %2 unsubstituted in the stylesheet")
                                .arg(tid).arg(leftover.captured())));
    }
}

void ThemeStylesheetIsFullySubstitutedTest::aConfirmationDialogPaintsTheThemedBackground() {
    for(ColorSchemes tid : presets) {
        settings->setColorScheme(ThemeStore::colorScheme(tid));
        settings->loadStylesheet();

        // Built the same way CustomMessageBox::confirm() builds a trash/delete
        // prompt, but shown non-modally so the test can grab it.
        CustomMessageBox box(nullptr);
        box.setTitle(QStringLiteral("Move to trash"));
        box.setText(QStringLiteral("Move item to trash?"));
        box.addButton(QStringLiteral("Yes"), true, true, true);
        box.addButton(QStringLiteral("No"), false);
        box.resize(320, 150);
        box.show();
        QVERIFY(QTest::qWaitForWindowExposed(&box));

        const QImage rendered = box.grab().toImage();
        // Sample inside the rounded body, clear of the corner radius and of
        // any text or button.
        const QColor body = rendered.pixelColor(rendered.width() / 2, 8);
        QCOMPARE(body.name(), settings->colorScheme().widget.name());

        box.hide();
    }
}

TG_BEHAVIOR_TEST_MAIN(ThemeStylesheetIsFullySubstitutedTest)
#include "test_theme_stylesheet_is_fully_substituted.moc"
