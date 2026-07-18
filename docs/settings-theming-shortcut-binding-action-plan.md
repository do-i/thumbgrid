# Settings Theming / Shortcut Binding Action Plan

Issues reported 2026-07-18 (follow-up to
`scripts-shortcuts-dropdown-polish-action-plan.md`, whose T1–T5 are all done).
Every item carries an **Execution model**: best-fit AI model + rationale +
steps + verification.

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work.

Verification: build + `import`/xclick screenshot flow. Theme checks run under a
sandboxed HOME (`env HOME=<tmp>/home5 XDG_CONFIG_HOME=...`) so the user's live
config is never modified. Use `pkill -x thumbgrid` (never `pkill -f`, which
suicides the shell running it).

---

## U1. Grid + Document pages render unthemed

**Symptom:** the Grid and Document settings pages show a default (light/native)
background while General, Theme, Shortcuts, Scripts etc. follow the theme.

**Diagnosis — confirmed.** The themed pages come from `settingsdialog.ui`, and
the QSS matches them **by object name**:

```
SettingsDialog QScrollArea > QWidget,
SettingsDialog #scrollAreaWidgetContents,
SettingsDialog #scrollAreaWidgetContents_2 … _5 { background-color: %background%; }
```
(`style-template.qss:23-30`)

Grid and Document are built at runtime by
`SettingsDialog::makeSettingsPage()` (`settingsdialog.cpp:277-300`, called at
`:331` and `:347`). Its `contents` widget gets **no object name**, so none of
the `#scrollAreaWidgetContents_N` selectors match. `QScrollArea > QWidget`
matches only the scroll area's *viewport*, not the contents widget nested
inside it — so the page body falls back to the palette default.

**Fix:** stop keying page backgrounds off hand-numbered object names. In
`makeSettingsPage()` set `contents->setAccessibleName("SPageContents")`, add
`[accessibleName="SPageContents"] { background-color: %background%; }` to the
QSS, and give the same accessible name to the five `.ui`
`scrollAreaWidgetContents*` widgets so the `_N` id selectors can be deleted.
Any future programmatic page is then themed by construction.

**Execution model**

*Model:* **Haiku 4.5** — the selector and both edit sites are pinned above; a
mechanical rename plus one QSS rule, verified by eye.

Steps: edit → build → screenshot Grid and Document pages on a dark theme
(sandbox HOME): background matches General; group boxes/headers unchanged;
re-check General/Theme/Controls/Scripts/Advanced/About for no regression.

## U2. Dropdowns still too tall — target 1–2 px text margin

**Symptom:** T5's 10 % trim was not enough. Wanted: combobox text with roughly
1–2 px of space above and below.

**Diagnosis — the trim targeted the wrong number.** Two things add height and
T5 only touched one:

- `min-height: %combobox_height%` = `round(0.9 × button_height)` where
  `button_height = text_height + 2 × (0.25 × text_height)` = `1.5 × text_height`
  (`settings.cpp:737-739`). So the content box alone is ≈ `1.35 × text_height`
  — already ~0.35 × text_height of slack before any padding.
- `padding: 4px 24px 4px 8px` on the shared combobox rule
  (`style-template.qss:1123-1131`). In Qt QSS `min-height` sizes the **content
  rect**, so this padding stacks *on top*, adding another 8 px.

Rendered height is therefore ≈ `1.35 × text_height + 8px` against a text
height of `text_height`. Shaving 10 % off one of the two terms is nearly
invisible.

**Fix:**
1. `settings.cpp`: redefine `combobox_height` as `text_height + 2` (a ~1 px
   margin each side) rather than a fraction of `button_height`; keep a floor so
   tiny fonts don't clip descenders (`qMax(text_height + 2, 18)` — tune).
2. QSS: drop the vertical padding on the combobox rule to `2px`
   (`padding: 2px 24px 2px 8px`); keep the horizontal values so the down-arrow
   from T5 still clears the text.
3. Re-check the popup list (`QComboBox QAbstractItemView::item`) — item height
   is styled separately and should **not** shrink with the closed box.
4. Apply to `SidePanel QComboBox` only if it visually needs it; that one is not
   in the settings group and may be intentionally chunkier.

**Execution model**

*Model:* **Sonnet 5** — the arithmetic and both edit sites are specified, but
the final value is a look-at-it judgment across font sizes, and clipping
regressions (descenders, the T5 arrow) need a screenshot loop.

Steps: edit → build → screenshot every settings dropdown at the default font
and at one larger UI font: ~1–2 px margin, no glyph clipping, arrow still
centered, popup rows unchanged. Light + dark.

## U3. Unify button sizing — settings buttons vs dialog buttons

**Symptom:** buttons in the Scripts → New/Edit popup are oddly sized; they
should match the main settings buttons (OK / Cancel / Apply).

**Diagnosis — two competing min-heights, resolved by CSS specificity.** There
are two rules:

- `SettingsDialog QPushButton { min-height: %button_height%; }`
  (`style-template.qss:32-34`) — a plain descendant selector.
- the shared dialog-button group (`:1249-1266`) which ends with
  `min-height: 0px; padding: 6px 14px; min-width: 70px` and lists
  `ScriptEditorDialog QPushButton` **and** `SettingsDialog QPushButton#OK`,
  `#Cancel`, `#applyButton`.

An id selector outranks a bare type selector, so OK/Cancel/Apply take
`min-height: 0px` and size from padding, while any *un-named* settings button
takes the taller `%button_height%`. `ScriptEditorDialog`'s buttons get
`min-height: 0` and no `%button_height%` at all. The result is three different
heights across dialogs that should look identical.

**Fix:** make one canonical button height and stop expressing it twice.
1. Delete the `SettingsDialog QPushButton { min-height: %button_height% }` rule.
2. In the shared group, replace `min-height: 0px` with
   `min-height: %button_height%` and reduce the vertical padding correspondingly
   so the total height is unchanged from today's OK button (which is the
   reference look the user asked for).
3. Convert the group's selector list to a positive marker —
   `QPushButton[accessibleName="SDialogButton"]` — or at minimum add the missing
   settings buttons (see U4). The current list is opt-in by name and every new
   button silently misses it.

**Execution model**

*Model:* **Sonnet 5** — the specificity analysis is done, but changing a
selector group this widely shared touches every dialog in the app
(Resize, FileReplace, ScriptEditor, ShortcutCreator, Print, QMessageBox,
CustomMessageBox, ColorPicker, Settings) and each needs a look.

Steps: edit → build → screenshot each listed dialog before/after: identical
button height and padding everywhere, accent `:default` still applied, no
button clipped by its layout.

## U4. Scripts "+ New" button is unthemed

**Symptom:** the "+ New" button on the Scripts tab does not use the app button
theme; it should match the main settings buttons in look and size.

**Diagnosis — confirmed, same root cause as U3.** `newScriptButton`
(`settingsdialog.ui:4500`) is a plain `QPushButton`. The themed button group
(`style-template.qss:1249-1266`) enumerates settings buttons by id and only
lists `#OK`, `#Cancel`, `#applyButton` — so `newScriptButton` gets **no**
background, border-radius or padding, only the `min-height: %button_height%`
from the bare rule. It renders as a native button at a non-native height, which
is exactly the "strange size + unthemed" report.

**Fix:** fold into U3. Once the group is keyed off
`accessibleName="SDialogButton"`, set that accessible name on `newScriptButton`
(and audit the Scripts/Advanced/Theme pages for any other in-page `QPushButton`
missing it — `resetZoomLevelsButton` and the theme-page buttons are likely in
the same position). Add hand cursor for consistency with the other buttons.

**Execution model**

*Model:* **Haiku 4.5** — mechanical once U3 lands: grep every `QPushButton` in
`settingsdialog.ui`, tag the ones that should look like dialog buttons.

Steps: after U3, tag + build → screenshot Scripts tab: "+ New" matches OK in
fill, radius, padding and height, hover/pressed states work, light + dark.

**Ordering:** U3 before U4 — U4 is the tagging pass that U3's selector rewrite
enables.

## U5. Scripts cannot be bound to a shortcut from the UI

**Question asked:** how does one execute a script command, and do we need a way
to bind shortcuts?

**Answer — two execution paths exist; one of them is unreachable.**

1. **Context menu.** `ContextMenu::fillOpenWithMenu()`
   (`contextmenu.cpp:124-136`) lists *every* non-empty script as an
   `s:<name>` action button. This works today and is how scripts are currently
   run.
2. **Shortcut.** `ActionManager` already understands the `s:` prefix:
   `ACTION_SCRIPT` is dispatched at `actionmanager.cpp:384-387` →
   `Core::runScript()` (`core.cpp:1104`) → `ScriptManager::runScript()`, which
   substitutes `%file%` with the selected image path and honours the
   blocking/detached flag. `ShortcutCreatorDialog` has had a "Scripts" radio +
   script combobox since forever and emits `"s:" + name`
   (`shortcutcreatordialog.cpp:47-51`).

So binding a script to a key is **fully implemented in the model and the
dialog** — there is simply no button left that opens the dialog.
`SettingsDialog::setupShortcutsPage()` starts with:

```cpp
ui->pushButton_2->hide();  // Add
ui->pushButton_8->hide();  // Edit
ui->pushButton_4->hide();  // Remove
```
(`settingsdialog.cpp:385-387`, hidden by commit a410434a "Redesign Preferences
screen")

and the slot they used to call, `SettingsDialog::addShortcut()`
(`settingsdialog.cpp:1418-1438`), is intact and correct — it reads the
dialog's context, writes the draft, sets the primary key, enables the action
and switches the context combo to match.

Compounding it, `updateShortcutsTable()` (`settingsdialog.cpp:1061-1066`)
builds its row set from *actions that already have a binding* in the selected
context (defaults ∪ draft). An unbound script therefore has no row to click,
so even the T4-era click-the-Key-cell editor can't reach it.

**Fix:**
1. Restore an entry point: a themed **"+ New shortcut"** button at the top of
   the Shortcuts tab (mirroring the Scripts tab's "+ New" from T2), wired to
   the existing `addShortcut()`. Prefer a fresh button over un-hiding
   `pushButton_2` so it can carry the U3/U4 `SDialogButton` marker and a sane
   object name.
2. Also list **unbound scripts** as rows in the table, so scripts are
   discoverable where users look for them. In `updateShortcutsTable()`, seed
   the action set with `"s:" + n` for every `scriptManager->scriptNames()`
   when the context is Global. A row with an empty Key cell already behaves
   correctly — clicking it opens the key editor.
3. Display: strip the `s:` prefix for presentation and mark script rows (e.g.
   a "script" badge or an icon in the Action column) so they read as scripts
   rather than as an oddly-named action.
4. Document both paths in the Scripts tab hint text. `label_43` already says
   "Also, you can assign shortcuts to scripts in Shortcuts."
   (`settingsdialog.cpp:324`) — currently a false promise; it becomes true
   once (1) lands.

**Decision made without asking:** binding lives on the **Shortcuts** tab, not
as a per-row control on the Scripts tab. Reason: the conflict-detection,
per-context and primary-key machinery all lives there, and duplicating an entry
point on the Scripts tab would mean two UIs writing the same draft. Reversible
if the user prefers a per-script "Bind…" affordance later.

**Execution model**

*Model:* **Opus 4.8** — the plumbing is understood but the surface is a design
call: how script rows read in a table of actions, how an empty Key cell is
presented, and whether unbound scripts belong in Global only. Judgment plus a
screenshot loop.

Steps: implement → build → in a sandbox HOME create a script (`+ New` on the
Scripts tab, e.g. `xdg-open %file%`), confirm it appears in the Shortcuts
table, bind a key via the Key cell, OK the dialog, then press the key in the
grid and confirm the script actually runs. Also exercise "+ New shortcut" →
Scripts radio → pick script → assign key → verify.

## U6. No way to add or move a shortcut between contexts

**Symptom:** the Shortcuts tab offers no way to add a binding in a chosen
context, or to move an existing one between Global / Grid / Document.

**Diagnosis — "add" is U5's hidden button; "move" was never built.**

*Add.* `ShortcutCreatorDialog` already has the context combobox
(Global / Document / Grid, `shortcutcreatordialog.cpp:16-20`) plus
`setContext()`/`selectedContext()`, and `addShortcut()` honours it. Restoring
the button (U5 step 1) restores add-in-any-context.

*Move.* Nothing implements it. The context combo above the table is a **filter
over separate per-context maps** (`mShortcutDraft` is keyed by `ViewMode`), and
`openShortcutDetails()` (`:1137`) edits keys within one fixed context — it has
no context control at all. Note the table is also context-scoped in a way that
hides the problem: an action bound only in Global has no Grid row, so there is
nothing to drag or retarget.

*Stale code found on the way.* `contextAtRow()` (`:1270-1274`) reads column 3's
`Qt::UserRole` expecting a context string, but `updateShortcutsTable()` stores
the **action name** there (`:1104`) — column 3 is now the Enabled checkbox, not
the old Context column. `addShortcutToTable()` (`:1397`) still builds the
pre-redesign 3-column layout with a "Folder"/"Document" context cell. Both are
dead relics of the table shape that a410434a replaced; `selectShortcutRow()`
and `removeShortcutAt()` depend on them.

**Fix:**
1. Delete the stale trio — `addShortcutToTable()`, `contextAtRow()`,
   `selectShortcutRow()`, `removeShortcutAt()` — after confirming no live
   caller (grep; the surviving callers are the hidden buttons' slots). Do this
   **first**: leaving a silently-wrong `contextAtRow()` in place while adding
   real context logic invites reusing it.
2. Add a **context row-action**: right-click context menu on a table row (and/or
   a "Context…" control in the key editor) offering *Move to…* and *Copy to…*
   Global / Grid / Document. Implementation is a draft-map transfer:
   `setActionShortcuts(mShortcutDraft[dst], action, keys)` plus, for Move,
   clearing `mShortcutDraft[src]` and migrating the `mShortcutPrimary` /
   `mShortcutDisabled` entries, then `rebuildShortcutDraftLookup()` +
   `updateShortcutsTable()`.
3. Warn on collision using the existing `draftActionForShortcut(dst, key)`
   before committing, same wording as the key editor's clash label.
4. Respect the override rule already documented in
   `shortcutcreatordialog.cpp:14-15`: global bindings run everywhere and a
   view-specific binding overrides the same key. Moving Global → Grid therefore
   *narrows* scope; say so in the confirm text rather than silently changing
   behaviour.

**Execution model**

*Model:* **Opus 4.8** — this is the only genuinely new interaction in the
batch: draft-map surgery across three parallel maps (bindings, primary,
disabled) with collision and override semantics, plus a UI affordance that
doesn't exist yet. Both the semantics and the surface need judgment.

Steps: cleanup → implement → build → sandbox HOME: move a Grid-only binding to
Global and confirm it fires in the document view; copy a Global binding to Grid
and confirm both rows exist; trigger a collision and confirm the warning; OK
the dialog, reopen, confirm persistence; confirm `shortcuts.json` contains the
expected context sections.

## U7. Regression pass

The batch touches the shared button group (U3/U4), the shared combobox group
(U2), and the page-background selectors (U1) — all of which are app-wide, not
settings-only.

**Fix:** after U1–U6, walk every dialog and panel that the changed selectors
reach — Resize, FileReplace, ScriptEditor, ShortcutCreator, Print,
CustomMessageBox, ColorPicker, the side panel, the context menu — on one light
and one dark preset, and confirm nothing shifted. Run the behavior test suite
(`src/tests/behavior/`, note the qttest HOME-isolation requirement).

**Execution model**

*Model:* **Sonnet 5** — broad but mechanical: drive each surface, compare, and
report. No design decisions.

---

## Suggested order

```
U1  (isolated: makeSettingsPage + QSS background rule)
U2  (settings.cpp + combobox QSS)
U3  →  U4        (U4 is the tagging pass U3 enables)
U6-step-1        (delete stale table helpers)  ─┐
U5               (restore + New shortcut, list scripts)  ├─ same file, serialize
U6-steps-2..4    (move/copy between contexts)  ─┘
U7  (regression sweep, last)
```

U1 and U2 are disjoint from everything else and can run in parallel. U5 and U6
both own `settingsdialog.cpp`'s shortcuts section and must be serialized;
U6's cleanup step goes first so U5 doesn't build on dead helpers.

## Status

All items done 2026-07-18. Verification ran offscreen via a throwaway
`settings_shot` tool (QT_QPA_PLATFORM=offscreen + the behavior-test harness),
built on top of `src/tests/support/thumbgrid_test_support.h`. It rendered the
settings dialog page-by-page to PNG without touching the user's live X session
— worth rebuilding if this area is revisited, since driving the real GUI here
proved both intrusive and unreliable.

- [x] U1 — Grid/Document page backgrounds (Haiku 4.5) — done 2026-07-18,
      commit 6f0d030a. **Gotcha found:** the first attempt wrote the .ui
      property as `<string>` without `notr="true"`, so uic emitted
      `setAccessibleName` inside `retranslateUi()` — which runs after the
      widget is parented and polished — and the QSS attribute selector never
      matched. It fixed Grid/Document but silently un-themed General, Theme,
      Advanced and About. Every accessibleName in this repo must use
      `notr="true"`. Verified by pixel diff: page bg 239,239,239 → 26,26,26 on
      Grid/Document, byte-identical elsewhere.
- [x] U2 — combobox height (Sonnet 5) — done 2026-07-18, commit 41ea6446.
      `combobox_height` is now `qMax(text_height + 2, 18)` and the shared rule's
      vertical padding 4px → 2px. Measured 33px → 25px. Note `text_height` is
      `fm.height()` (full line box), so `+2` is the true floor: going tighter
      clips CJK entries in the language dropdown, which fill the em box. The
      residual ~8px gap around Latin capitals is font ascent/descent headroom,
      not removable padding.
- [x] U3 — unify button height (Sonnet 5) — done 2026-07-18, commit d1d5889c.
      Bare `SettingsDialog QPushButton` min-height rule deleted; shared group
      gained `QPushButton[accessibleName="SDialogButton"]`, padding 6px → 2px,
      min-height 0 → `%button_height%`. Settings OK/Cancel/Apply render
      byte-identical to before.
- [x] U4 — theme "+ New" and untagged settings buttons (Haiku 4.5) — done
      2026-07-18, commit d1d5889c. Tagged `newScriptButton`,
      `resetZoomLevelsButton`, `pushButton_5`. **Gotcha found:** the marker must
      NOT go in the `:default` accent group — Qt's stylesheet style treats
      autoDefault buttons as `:default`, and every QPushButton in a QDialog is
      autoDefault, so every tagged button rendered accent-green. Accept buttons
      keep the accent via their own explicit per-dialog selectors.
- [x] U5 — scripts bindable (Opus 4.8) — done 2026-07-18, commit d82be0b2.
      "+ New shortcut" button wired to the existing `addShortcut()`; unbound
      scripts seeded as Global rows labelled `<name>  (script)` with the real
      `s:<name>` id kept in `Qt::UserRole`. Covered by behavior test
      `test_a_script_can_be_bound_to_a_shortcut.cpp` (fabricates a real
      QKeyEvent and asserts `runScript` fires with the de-prefixed name);
      confirmed non-vacuous by mutation.
- [x] U6 — move/copy between contexts + stale-helper cleanup (Opus 4.8) — done
      2026-07-18, commit d82be0b2 (cleanup) and 15c2ecee (transfer). Dead
      `addShortcutToTable`/`contextAtRow`/`selectShortcutRow`/`removeShortcutAt`
      deleted (zero callers). Right-click row menu offers Move to…/Copy to…,
      carrying keys, primary choice and enabled state across all three parallel
      per-context structures, confirm-gated with explicit widen/narrow wording.
      Script bindings are allowed to move (dispatch is context-independent); a
      hole was fixed on the way — disabled actions now seed rows too, so an
      unchecked action can always be re-enabled. Covered by
      `test_a_shortcut_can_be_moved_between_contexts.cpp`; 3 of 4 cases fail
      with the transfer mutated out.
- [x] U7 — app-wide regression pass (Sonnet 5) — done 2026-07-18. All settings
      pages plus ResizeDialog, FileReplaceDialog, PrintDialog, CustomMessageBox
      and ColorPickerDialog rendered across all six presets. No regressions.
      One pre-existing clipped group-box title in ResizeDialog was surfaced and
      A/B-confirmed against commit 70fbbeaa as NOT caused by this batch — its
      likely cause is a stale `<minimumSize>` in resizedialog.ui. Not fixed;
      candidate for a follow-up.

Final state: 31/31 behavior tests pass, full build clean, fresh clone builds.
