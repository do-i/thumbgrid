# 001 — Shortcuts Page Audit Fixes Action Plan

Findings from the 2026-07-18 audit of commits `6f0d030a..5dc6c9df` (the
settings-theming / shortcut-binding batch, plan items U1–U7). Audit scope:
full diff review of all six commits, cross-check against the plan's claims,
clean rebuild, 31/31 behavior tests green, and an empirical Qt probe for the
one suspicious coordinate mapping. Everything the plan marked done is in fact
implemented; the items below are what the audit surfaced on top.

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work.

---

## R1. Row context menu acts on the wrong row — **regression, confirmed**

**Introduced by:** 15c2ecee (Move/Copy between contexts).

**Symptom:** right-clicking a shortcuts-table row opens the menu for the row
*above* the one clicked; right-clicking within the top ~22 px of the first
row opens no menu at all. The confirm dialog then names the wrong action, so
a user who doesn't re-read the text moves/edits the wrong binding.

**Diagnosis — coordinate double-mapping.** `showShortcutRowMenu()`
(`settingsdialog.cpp:1424-1427`) treats the incoming position as
table-widget coordinates and remaps it into the viewport:

```cpp
const QPoint local = viewport->mapFrom(ui->shortcutsTableWidget, pos);
const int row = ui->shortcutsTableWidget->rowAt(local.y());
```

But for `QAbstractScrollArea` subclasses, `customContextMenuRequested`
already delivers **viewport** coordinates (documented Qt exception: the
viewport's context-menu event is handed to the scroll area's event handler
untranslated). The `mapFrom` therefore subtracts the header height + frame a
second time, shifting the hit point up ~one row.

**Confirmed empirically** with a standalone offscreen Qt 6 probe: a
`QContextMenuEvent` sent at the middle of row 2 (viewport coords) reaches the
slot with `pos` unchanged — `rowAt(pos)` = 2 (correct), `rowAt(remapped)` = 1
(what the current code uses). The existing behavior test only asserts
`contextMenuPolicy` and cannot catch this.

**Fix:**
1. Use `pos` directly: `rowAt(pos.y())`, and `menu.exec(viewport->
   mapToGlobal(pos))` (`settingsdialog.cpp:1466`). Delete the `mapFrom` line.
2. Make it testable: factor the row resolution into a small helper (e.g.
   `int shortcutRowForMenuPos(const QPoint &viewportPos) const`) that
   `showShortcutRowMenu` calls before building the (blocking) `QMenu`, and
   extend `test_a_shortcut_can_be_moved_between_contexts.cpp` to assert the
   helper resolves the middle of row N's viewport rect to row N — that is the
   exact assertion the probe made and the one this bug violates.

**Execution model**

*Model:* **Sonnet 5** — the two-line fix is mechanical, but restructuring the
slot so the row resolution is testable without exec'ing a modal menu takes a
little care; the test must fail on the pre-fix code (mutation-check it).

Steps: fix → build → test red-on-revert/green-on-fix → offscreen sanity
render of the settings dialog unchanged.

## R2. Deleting a script leaves a ghost row via the disabled list — **minor**

**Introduced by:** the disabled-row seeding added in this batch
(`updateShortcutsTable()`, `settingsdialog.cpp:1094-1100`) interacting with
pre-existing script deletion.

**Diagnosis (inferred from code, not yet reproduced — verify first).**
`removeScript()` (`settingsdialog.cpp:~1575`) purges the script's bindings
via `actionManager->removeAllShortcuts("s:" + name)` but never removes
`"s:<name>"` from `mShortcutDisabled` (persisted through
`settings->saveDisabledShortcuts`, `:1628`). Sequence: uncheck a script's
Enabled box → Apply → delete the script. The stale disabled entry now seeds
a `<name>  (script)` row for a script that no longer exists, and the entry
lives in the config forever.

**Fix:** in `removeScript()`, purge `"s:" + name` from all three per-context
draft structures (`mShortcutDraft` via `setActionShortcuts(..., {})`,
`mShortcutPrimary`, `mShortcutDisabled`) before the `saveShortcuts()` /
`readShortcuts()` round-trip. Alternatively (defense in depth) skip seeding
disabled `s:` entries whose script no longer exists in
`scriptManager->scriptNames()`.

**Execution model**

*Model:* **Sonnet 5** — small, but the removeScript save/read ordering is
easy to get wrong and the repro needs the disable→apply→delete sequence
driven in a test (extend `test_a_script_can_be_bound_to_a_shortcut.cpp`).

Steps: write the failing repro first → fix → 31+ tests green.

## R3. Script-label formatting duplicated three times — **cleanup**

The anonymous-namespace helper `shortcutActionLabel()`
(`settingsdialog.cpp:959`) exists, but the identical `"%1  (script)"`
formatting is re-implemented inline in `updateShortcutsTable()`
(`:1124-1125`) and `openShortcutDetails()` (`:1189-1191`). Any future change
to the label (icon, badge, translation tweak) now has three sites to miss.

**Fix:** call the helper from both sites; keep the tooltip's distinct
`User script "%1"` string as-is.

**Execution model**

*Model:* **Haiku 4.5** — pinned mechanical substitution, verified by build +
existing label assertions in the script-binding behavior test.

---

## Suggested order

R1 first (user-visible regression), then R2, R3 folded into the same session.
All three live in `settingsdialog.cpp` — serialize, one session can take all.

## Status

All items done 2026-07-18. Full build warning-clean, 31/31 behavior tests
green, both new test cases mutation-verified (each fails when its fix is
reverted).

- [x] R1 — done 2026-07-18. Row resolution factored into public
      `shortcutActionAtMenuPos(viewportPos)`; the slot and the menu-exec
      position now use the signal's pos as-is. Covered by
      `theRowMenuResolvesTheClickedRow()` in the transfer test (middle of
      rows 0/1 + top edge of row 0); reintroducing the `mapFrom` fails it.
- [x] R2 — done 2026-07-18. `removeScript()` now purges `s:<name>` from
      `mShortcutDraft` / `mShortcutPrimary` / `mShortcutDisabled` in all
      three contexts before saving (the explicit
      `actionManager->removeAllShortcuts` call became redundant — the saved
      draft no longer contains the action). Repro confirmed the bug was
      real: with the purge mutated out, `deletingAScriptLeavesNoGhostRow()`
      shows the ghost row.
- [x] R3 — done 2026-07-18. Both inline `"%1  (script)"` copies replaced
      with `shortcutActionLabel()`.

**Build-tree gotcha found on the way:** the build cache had
`BUILD_TESTING=OFF` while the generated makefiles still contained the test
targets from an earlier configure — test binaries silently stopped
rebuilding (ctest ran stale executables that passed). Re-configured with
`-DBUILD_TESTING=ON`; if tests ever seem to ignore source edits, check this
cache flag first.
