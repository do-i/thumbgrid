#include "support/thumbgrid_test_support.h"

#include <QColor>

#include "settings.h"
#include "themestore.h"

// The custom theme is persisted to theme.ini with its identity under a [Theme]
// group (tid + name) and colors under [Colors], mirroring the system theme
// files. This verifies the save/load round-trip in that layout, and that
// selecting a preset never clobbers the stored custom palette.
class CustomThemePersistenceTest : public QObject {
    Q_OBJECT

private slots:
    void customPaletteRoundTripsThroughThemeIni();
    void selectingAPresetKeepsTheStoredCustomPalette();
};

void CustomThemePersistenceTest::customPaletteRoundTripsThroughThemeIni() {
    settings->setUseSystemColorScheme(false);

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
    settings->setUseSystemColorScheme(false);

    // Store a custom palette first.
    ColorScheme custom = ThemeStore::colorScheme(COLORS_DARK);
    custom.tid = COLORS_CUSTOM;
    custom.accent = QColor("#654321");
    settings->setColorScheme(custom);
    settings->saveTheme();

    // Switch to a preset and persist: only the [Theme] identity should change,
    // the stored [Colors] custom palette must be left untouched.
    settings->setColorScheme(ThemeStore::colorScheme(COLORS_LIGHT));
    settings->saveTheme();

    const ColorScheme stored = settings->customColorScheme();
    // Persisted identity now names the preset (read back from [Theme]/tid)...
    QCOMPARE(stored.tid, static_cast<int>(COLORS_LIGHT));
    // ...while the custom colors under [Colors] survived intact.
    QCOMPARE(stored.accent.name(), QStringLiteral("#654321"));
}

TG_BEHAVIOR_TEST_MAIN(CustomThemePersistenceTest)

#include "test_custom_theme_persistence.moc"
