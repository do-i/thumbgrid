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
    void xnviewmpPresetLoadsRepresentativeBindings();
    void presetWithEmptyContextSurvivesDialogRoundTrip();
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
    QVERIFY2(ids.contains("xnviewmp"), "xnviewmp is cross-platform and should be offered here.");
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

// (5) The newly-authored xnviewmp preset parses and applies like any other.
void ShortcutPresetsAndKeymapsPocTest::xnviewmpPresetLoadsRepresentativeBindings() {
    actionManager->applyPreset("xnviewmp");

    QCOMPARE(actionManager->selectedPreset(), QStringLiteral("xnviewmp"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "Right"), QStringLiteral("nextImage"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "L"), QStringLiteral("rotateLeft"));
    QCOMPARE(actionManager->actionForShortcut(MODE_GLOBAL, "Del"), QStringLiteral("moveToTrash"));
    QVERIFY2(!settings->shortcutsModified(), "Right after applyPreset the mapping equals the preset.");
}

// (6) SettingsDialog::saveShortcuts() rebuilds the active mapping via
// removeAllShortcuts() + addShortcut() from its draft on every Apply/OK, even
// when the Controls tab was untouched. The rebuild loop never touches a
// context whose draft is empty, so removeAllShortcuts() must re-seed every
// context key itself; otherwise a preset with an empty or missing section
// (e.g. the windows-only irfanview.json has no "grid") comes back with fewer
// context keys than `defaults`, the shortcuts != defaults comparison in
// saveShortcuts() turns spuriously true, and the preset combo shows "Custom"
// right after a clean preset switch.
void ShortcutPresetsAndKeymapsPocTest::presetWithEmptyContextSurvivesDialogRoundTrip() {
    actionManager->applyPreset("leftie");
    QVERIFY2(!settings->shortcutsModified(), "Right after applyPreset the mapping equals the preset.");

    // Leftie's context split survived loading (not everything under global).
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "Right"), QStringLiteral("nextImage"));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "F7"), QStringLiteral("createDirectory"));
    QCOMPARE(actionManager->actionForShortcut(MODE_FOLDERVIEW, "Right"), QString());

    // Mirror SettingsDialog::saveShortcuts(): rebuild the active mapping from a
    // draft copy of the current shortcuts, as happens unconditionally on Apply/OK.
    const ActionManager::ShortcutMap draft = actionManager->allShortcuts();
    actionManager->removeAllShortcuts();
    for(ViewMode ctx : {MODE_GLOBAL, MODE_DOCUMENT, MODE_FOLDERVIEW})
        QVERIFY2(actionManager->allShortcuts().contains(ctx),
                 "removeAllShortcuts() must keep every context key present so a "
                 "bindingless context still round-trips equal to defaults.");
    for(auto ctx = draft.cbegin(); ctx != draft.cend(); ++ctx)
        for(auto it = ctx.value().cbegin(); it != ctx.value().cend(); ++it)
            actionManager->addShortcut(ctx.key(), it.key(), it.value());
    actionManager->saveShortcuts();

    QVERIFY2(!settings->shortcutsModified(),
             "A no-op dialog round-trip must not spuriously mark the mapping as Custom.");
}

TG_BEHAVIOR_TEST_MAIN(ShortcutPresetsAndKeymapsPocTest)

#include "test_shortcut_presets_and_keymaps_poc.moc"
