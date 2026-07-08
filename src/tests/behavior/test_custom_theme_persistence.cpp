#include "support/thumbgrid_test_support.h"

#include <QColor>

#include "settings.h"
#include "themestore.h"

// The custom theme's colors are persisted to theme.ini under [Colors], mirroring
// the system theme files. The active selection is a separate pointer in
// thumbgrid.conf [General]/theme, so the theme file never stores which theme is
// picked. This verifies the color save/load round-trip, and that selecting a
// preset moves the pointer without clobbering the stored custom palette.
class CustomThemePersistenceTest : public QObject {
    Q_OBJECT

private slots:
    void customPaletteRoundTripsThroughThemeIni();
    void selectingAPresetKeepsTheStoredCustomPalette();
};

void CustomThemePersistenceTest::customPaletteRoundTripsThroughThemeIni() {
    settings->setSelectedThemeTid(COLORS_CUSTOM);

    ColorScheme custom = ThemeStore::colorScheme(COLORS_DARK);
    custom.tid = COLORS_CUSTOM;
    custom.accent = QColor("#123456");
    custom.text = QColor("#abcdef");
    custom.folderview_parent_icon = QColor("#0f0f0f");
    settings->setColorScheme(custom);
    settings->saveTheme();

    const ColorScheme reloaded = settings->customColorScheme();
    QCOMPARE(reloaded.tid, static_cast<int>(COLORS_CUSTOM));
    QCOMPARE(reloaded.accent.name(), QStringLiteral("#123456"));
    QCOMPARE(reloaded.text.name(), QStringLiteral("#abcdef"));
    QCOMPARE(reloaded.folderview_parent_icon.name(), QStringLiteral("#0f0f0f"));
}

void CustomThemePersistenceTest::selectingAPresetKeepsTheStoredCustomPalette() {
    settings->setSelectedThemeTid(COLORS_CUSTOM);

    // Store a custom palette first.
    ColorScheme custom = ThemeStore::colorScheme(COLORS_DARK);
    custom.tid = COLORS_CUSTOM;
    custom.accent = QColor("#654321");
    settings->setColorScheme(custom);
    settings->saveTheme();

    // Switch to a preset and persist: only the [General]/theme pointer should
    // change, the stored [Colors] custom palette must be left untouched.
    settings->setColorScheme(ThemeStore::colorScheme(COLORS_LIGHT));
    settings->saveTheme();

    // The selection pointer now names the preset...
    QCOMPARE(settings->selectedThemeTid(), static_cast<int>(COLORS_LIGHT));
    // ...while the stored custom palette under [Colors] survived intact.
    const ColorScheme stored = settings->customColorScheme();
    QCOMPARE(stored.tid, static_cast<int>(COLORS_CUSTOM));
    QCOMPARE(stored.accent.name(), QStringLiteral("#654321"));
}

TG_BEHAVIOR_TEST_MAIN(CustomThemePersistenceTest)

#include "test_custom_theme_persistence.moc"
