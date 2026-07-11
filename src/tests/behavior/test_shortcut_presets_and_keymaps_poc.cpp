#include "support/thumbgrid_test_support.h"

#include "shortcutpresetstore.h"

// PoC coverage for the shortcuts/keymap redesign:
//   1. the native scan-code map loads from the per-OS keymap resource,
//   2. a fresh config is seeded into shortcuts.json from the qimgv preset,
//   3. the preset catalog is OS-filtered (built from _meta.os),
//   4. applying a preset overwrites the active mapping and drives the
//      [Shortcuts]/modified flag (self-correcting on save).
class ShortcutPresetsAndKeymapsPocTest : public QObject {
    Q_OBJECT

private slots:
    void keymapLoadsFromResource();
    void firstRunSeedsShortcutsFromQimgv();
    void availablePresetsAreOsFiltered();
    void applyingPresetSwitchesMappingAndTracksModified();
};

// (1) The scan-code map is now data, loaded from keymap_<os>.json.
void ShortcutPresetsAndKeymapsPocTest::keymapLoadsFromResource() {
    QVERIFY2(!inputMap->keys().isEmpty(), "Keymap resource should populate the scan-code map.");

    const quint32 scanCode = inputMap->keys().key("C");
    QVERIFY2(scanCode != 0, "Keymap should know a scancode for C.");
    QCOMPARE(inputMap->keys().value(scanCode), QStringLiteral("C"));

    // Modifier display names come from the same resource.
    QVERIFY(inputMap->modifiers().contains(InputMap::keyNameCtrl()));
    QCOMPARE(InputMap::keyNameCtrl(), QStringLiteral("Ctrl")); // non-macOS build
}

// (2) No shortcuts.json on first boot -> materialized from the qimgv seed, with
// the [Shortcuts] pointers recorded.
void ShortcutPresetsAndKeymapsPocTest::firstRunSeedsShortcutsFromQimgv() {
    QVERIFY2(settings->shortcutsJsonExists(),
             "First boot should have written shortcuts.json from the seed preset.");
    QCOMPARE(settings->selectedPreset(), QStringLiteral("qimgv"));
    QVERIFY2(!settings->shortcutsModified(),
             "A freshly seeded mapping equals its preset, so modified is false.");

    // Representative qimgv bindings survived materialization.
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "Right"), QStringLiteral("nextImage"));
    QCOMPARE(actionManager->actionForShortcut(MODE_GLOBAL, "Esc"), QStringLiteral("closeFullScreenOrExit"));
}

// (3) The catalog offered on this OS is filtered by each preset's _meta.os.
void ShortcutPresetsAndKeymapsPocTest::availablePresetsAreOsFiltered() {
    QStringList ids;
    for(const PresetInfo &p : ActionManager::availablePresets())
        ids << p.id;

    QVERIFY2(ids.contains("qimgv"), "qimgv is cross-platform and always present.");
    QVERIFY2(ids.contains("gwenview"), "gwenview targets linux and should be offered here.");
    QVERIFY2(!ids.contains("irfanview"),
             "irfanview targets windows only; excluded on this (linux) build.");
}

// (4) Applying a preset replaces the mapping; modified tracks divergence and
// clears when back in sync.
void ShortcutPresetsAndKeymapsPocTest::applyingPresetSwitchesMappingAndTracksModified() {
    actionManager->applyPreset("gwenview");

    // Gwenview-style binding is now active; the qimgv-only F12 settings key is gone.
    QCOMPARE(actionManager->selectedPreset(), QStringLiteral("gwenview"));
    QCOMPARE(actionManager->actionForShortcut(MODE_GLOBAL, "Ctrl+Shift+Comma"), QStringLiteral("openSettings"));
    QCOMPARE(actionManager->actionForShortcut(MODE_GLOBAL, "F12"), QString());
    QVERIFY2(!settings->shortcutsModified(), "Right after applyPreset the mapping equals the preset.");

    // A user edit marks the mapping modified on save.
    actionManager->addShortcut(MODE_DOCUMENT, "Z", "crop");
    actionManager->saveShortcuts();
    QVERIFY2(settings->shortcutsModified(), "Editing a binding diverges from the preset.");
}

TG_BEHAVIOR_TEST_MAIN(ShortcutPresetsAndKeymapsPocTest)

#include "test_shortcut_presets_and_keymaps_poc.moc"
