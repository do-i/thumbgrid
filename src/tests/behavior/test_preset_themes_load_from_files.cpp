#include "support/thumbgrid_test_support.h"

#include <QColor>

#include "themestore.h"

// Presets used to be hardcoded in a C++ switch; they now live in INI files
// (installed to /etc/thumbgrid/themes, embedded in resources.qrc as a fallback).
// This test exercises the file-loading path and guards the extracted hex values
// against typos. In the test environment /etc is absent, so it verifies the
// embedded qrc fallback loads correctly.
class PresetThemesLoadFromFilesTest : public QObject {
    Q_OBJECT

private slots:
    void presetsLoadTheirDocumentedColors();
    void unknownPresetDoesNotCrash();
};

void PresetThemesLoadFromFilesTest::presetsLoadTheirDocumentedColors() {
    const ColorScheme light = ThemeStore::colorScheme(COLORS_LIGHT);
    QCOMPARE(light.tid, static_cast<int>(COLORS_LIGHT));
    QCOMPARE(light.accent.name(), QStringLiteral("#719ccd"));
    QCOMPARE(light.background.name(), QStringLiteral("#1a1a1a"));
    QCOMPARE(light.widget.name(), QStringLiteral("#ffffff"));

    const ColorScheme black = ThemeStore::colorScheme(COLORS_BLACK);
    QCOMPARE(black.background.name(), QStringLiteral("#000000"));
    QCOMPARE(black.text.name(), QStringLiteral("#b0b0b0"));

    const ColorScheme dark = ThemeStore::colorScheme(COLORS_DARK);
    QCOMPARE(dark.accent.name(), QStringLiteral("#8c9b81"));
    QCOMPARE(dark.folderview_topbar.name(), QStringLiteral("#383838"));

    const ColorScheme darkblue = ThemeStore::colorScheme(COLORS_DARKBLUE);
    QCOMPARE(darkblue.accent.name(), QStringLiteral("#336ca5"));
    QCOMPARE(darkblue.icons.name(), QStringLiteral("#babec3"));

    // Light Blue is the one preset that sets the optional folderview_* keys.
    const ColorScheme lightBlue = ThemeStore::colorScheme(COLORS_LIGHT_YELLOW);
    QCOMPARE(lightBlue.tid, static_cast<int>(COLORS_LIGHT_YELLOW));
    QCOMPARE(lightBlue.icons.name(), QStringLiteral("#006394"));
    QCOMPARE(lightBlue.widget.name(), QStringLiteral("#e4e3f7"));
    QCOMPARE(lightBlue.folderview_parent_icon.name(), QStringLiteral("#26a269"));
    QCOMPARE(lightBlue.folderview_selection.name(), QStringLiteral("#badff8"));
}

void PresetThemesLoadFromFilesTest::unknownPresetDoesNotCrash() {
    // A non file-backed, non palette-derived id yields an (invalid) scheme
    // carrying the requested tid rather than crashing.
    const ColorScheme scheme = ThemeStore::colorScheme(static_cast<ColorSchemes>(99));
    QCOMPARE(scheme.tid, 99);
}

TG_BEHAVIOR_TEST_MAIN(PresetThemesLoadFromFilesTest)

#include "test_preset_themes_load_from_files.moc"
