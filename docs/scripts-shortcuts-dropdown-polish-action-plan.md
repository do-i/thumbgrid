# Scripts / Shortcuts / Dropdown Polish Action Plan

Issues reported 2026-07-18 (follow-up to the settings-ui-consistency plan).
Every item carries an **Execution model**: best-fit AI model + rationale +
steps + verification.

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work.

Verification: build + `import`/xclick screenshot flow. Dark-theme checks run
under a sandboxed HOME (`env HOME=<tmp>/home5 XDG_CONFIG_HOME=... `) so the
user's live theme config is never modified. Use `pkill -x thumbgrid` (never
`pkill -f`, which suicides the shell running it).

---

## T1. Script editor dialog buttons — standard set, accept is default

**Symptom:** the script Add dialog (Create/Cancel) and Edit dialog
(Save/Cancel) buttons don't match the standard dialog button set; Create/Save
should be the accented default.

**Diagnosis:** both are one dialog — `ScriptEditorDialog`
(`scripteditordialog.ui:216` `acceptButton`, text switched to "Create"/"Save"
in `scripteditordialog.cpp:61-71`). Order is already accept-first (S3 sweep).
Its buttons are in the shared base/hover/pressed QSS groups but there is **no
`ScriptEditorDialog QPushButton:default` accent rule**, no `setDefault`, no
hand cursor, and stock icons aren't suppressed.

**Fix:** in `scripteditordialog.cpp` constructor: `acceptButton->setDefault
(true)`, pointing-hand cursor + `setIcon(QIcon())` on both buttons. In
`style-template.qss`, add `ScriptEditorDialog QPushButton:default` to the
accent group (the one with `CustomMessageBox QPushButton:default`).

**Execution model**

*Model:* **Haiku 4.5** — both edits are pinned above; identical pattern
already exists in shortcutcreatordialog.cpp (commit d1003ff1) to copy from.

Steps: apply the two edits → build → reviewer screenshots the Add and Edit
dialogs (Settings → Scripts → Add / double-click a script): Create/Save
accented + defaulted, Cancel plain, both hand-cursor.

## T2. Scripts screen redesign — "+ New" button, per-row edit/delete icons

**Symptom:** the Scripts tab's Add/Edit/Remove button row is clunky. Wanted:
a single "+ New" button at the top; each list row shows a pencil (edit) icon
and an x (delete) icon.

**Diagnosis:** Scripts tab = `pushButton_9` (Add), `pushButton_6` (Edit),
`pushButton_7` (Remove) above `scriptsListWidget` (QListWidget,
alternatingRowColors, double-click already wired to `editScript()` —
`settingsdialog.ui:4513-4553, 5751`).

**Fix:**
1. Replace the three buttons in the .ui with one `newScriptButton` ("+ New")
   at the top of the tab (left-aligned; keep the row layout + spacer).
2. Per-row controls: attach a small row widget via
   `QListWidget::setItemWidget()` — script name label + stretch + pencil
   `IconButton`/flat tool button (`:/res/icons/common/overlay/edit16.png`)
   + x button (`:/res/icons/common/overlay/close16.png`); both recolor via
   the IconWidget mechanism if `IconButton` is used (see
   `src/gui/customwidgets/iconbutton.*` / `actionbutton.*` for the existing
   pattern). Pencil → existing `editScript` path; x → existing remove path
   (confirm-dialog behavior unchanged).
3. Keep double-click-to-edit. Keep keyboard delete if currently supported.
4. Rebuild list rows on add/edit/remove refresh (the current populate logic
   in `settingsdialog.cpp` — find `scriptsListWidget` usages).
5. QSS: hover styling for the row icon buttons if needed; respect
   alternating row colors.

**Execution model**

*Model:* **Opus 4.8** — this is an interaction redesign, not a styling fix:
custom item widgets interact with QListWidget selection/alternate-row
painting, icon buttons need hover/recolor behavior, and the result must be
visually judged and iterated at 16 px in both light and dark themes.

Steps: implement → build → screenshot loop on Scripts tab (rows with pencil/x,
"+ New" top), exercise add/edit/delete end-to-end in the sandbox HOME
instance, verify list refresh and no regression to script execution config
saving.

## T3. Shortcuts screen — drop "Reset current context"

**Symptom:** with preset switching in place, the button is redundant —
selecting another preset resets bindings anyway.

**Diagnosis:** the button is `pushButton_3` (`settingsdialog.ui:4106`, .ui
text "Reset to defaults", retitled "Reset current context" at
`settingsdialog.cpp:385`). Sits in the Shortcuts tab header row next to that
tab's own Add/Edit/Remove trio (`pushButton_2/_8/_4`, ui:4051-4077 — NOT the
scripts trio; leave those alone).

**Fix:** remove the `pushButton_3` widget from the .ui, its retitle line, its
connect/slot wiring in `settingsdialog.cpp` (find the clicked handler; remove
the slot only if nothing else calls it — the underlying reset logic may be
reachable elsewhere, in which case keep the function).

**Execution model**

*Model:* **Haiku 4.5** — a pinned widget deletion plus dead-wiring cleanup;
grep-verifiable. Escalate to Sonnet 5 only if the slot turns out to be shared
with other callers in a non-obvious way.

Steps: remove widget + wiring → build → screenshot Shortcuts tab: button gone,
Add/Edit/Remove trio and search/preset controls intact and correctly laid out.

## T4. Shortcuts table — per-preset color scheme

**Symptom:** `shortcutsTableWidget` (`settingsdialog.ui:4121`) has **zero**
QSS rules, so it renders native white even on dark themes.

**Diagnosis / plumbing:** theme colors flow: preset conf files
(`src/res/theme-presets/theme-{dark,dark-v2,light}.conf`, `[Colors]`
key=value; system-installed copies preferred — see `themestore.cpp` fixed
preset→file mapping, mirrors the default-shortcuts pattern) → ColorScheme →
`Settings::loadStylesheet()` `%placeholder%` replacement (`settings.cpp:~730+`).
Note: the preset picker shows more entries (Black, Dark, Dark Blue, Light,
Light Blue) than there are conf files in-repo — locate the full preset file
set (likely `/etc/thumbgrid/` + qrc fallbacks) before adding keys.

**Fix:**
1. New color keys (suggested): `table_bg`, `table_bg_alt` (alternating rows),
   `table_header`, `table_text`, `table_border`. Add to ColorScheme
   parsing/defaults, with **derived fallbacks** when a preset file lacks them
   (derive from `widget`/`background` HSV like the sys_window_tinted family
   in settings.cpp:770-781) so third-party/custom presets don't break.
2. Populate every preset file: light presets keep today's effective look
   (white-ish bg, current text); dark presets get colors slightly darker
   than the tab background (e.g. bg ≈ background −2..6% value, alt row +4%,
   header from folderview_topbar family) — final values by visual judgment.
3. QSS: `SettingsDialog QTableWidget` rules — background, alternate row,
   gridline/border, text, header sections (`QHeaderView::section`), selected
   row (`%accent_hover_rgba%`), corner button.

**Execution model**

*Model:* **Opus 4.8** — spans conf format, ColorScheme plumbing, derived-color
fallbacks, and per-preset palette design the user delegated to "best
judgement"; the palette must be tuned by looking at screenshots of all five
presets, which is exactly the judgment-iteration loop Opus is assigned.

Steps: plumbing + keys + QSS → build → sandbox-HOME instance: screenshot the
Shortcuts table under Black, Dark, Dark Blue (darker scheme, readable text,
alternating rows visible) and Light, Light Blue (unchanged look); verify
selection highlight and header styling.

## T5. Dropdowns — 10% shorter, with a down-arrow indicator

**Symptom:** the newly themed dropdowns are ~10% too tall and give no visual
cue that they're dropdowns.

**Diagnosis:** height comes from `SettingsDialog QComboBox { min-height:
%button_height%; }` (added in S4). Arrow: the app-wide themed comboboxes
suppress the native arrow (`::drop-down { border: 0 }`) and the only arrow
asset (`res/icons/common/other/dropDownArrow.png`) is a magenta
recolor-placeholder unusable raw in QSS. But the stylesheet template already
supports theme-variant image paths — `url(:/res/icons/%icontheme%/other/...)`
(style-template.qss:922-928) — with `res/icons/light/other/` and
`res/icons/dark/other/` dirs existing; `%icontheme%` is currently hardcoded
to `"light"` (`settings.cpp:783`).

**Fix:**
1. Height: introduce `%combobox_height%` = round(0.9 × button_height) in
   `settings.cpp` (next to the button_height computation, ~line 737) and use
   it in the `SettingsDialog QComboBox` min-height rule.
2. Arrow assets: from the magenta placeholder's alpha, generate
   `dropDownArrow.png` + `@2x` in a light-theme color (dark gray glyph) into
   `res/icons/light/other/` and a dark-theme color (light gray glyph) into
   `res/icons/dark/other/` (match the icon color the presets use for
   `icons`); register all four in `resources.qrc`.
3. QSS: on the merged combobox group (ResizeDialog/ShortcutCreator/Settings),
   set `::down-arrow { image: url(:/res/icons/%icontheme%/other/dropDownArrow.png); }`
   and give `::drop-down` a fixed width + the box extra right padding so text
   never underlaps the arrow. Apply to `SidePanel QComboBox` too for
   consistency (same group or mirrored rule).
4. Make `%icontheme%` follow the theme instead of hardcoded "light": pick
   `dark` when the theme background is dark (a luminance check on
   `colorScheme().background` — same idiom as the dark/light branch at
   settings.cpp:770). This also fixes the existing collapsed/expanded branch
   icons on dark themes.

**Execution model**

*Model:* **Sonnet 5** — crosses four mechanisms (placeholder computation,
asset generation, qrc, QSS pseudo-element quirks) and needs light/dark
screenshot iteration, but every sub-step is specified; no open-ended design.

Steps: implement → build → screenshots light + dark (sandbox HOME): all
settings dropdowns ~10% shorter, arrow visible and theme-appropriate in both,
popup unchanged, ResizeDialog/ShortcutCreator/SidePanel comboboxes consistent;
places-panel branch icons still correct on both themes.

---

## Suggested order

T1 ∥ T3 (disjoint files) → T5 → T4 (both own the QSS/settings.cpp — serialize)
→ T2 last (biggest settingsdialog.ui/cpp surface, avoids conflicts with T3's
edits in the same files? — no: T3 also touches settingsdialog.{ui,cpp}; run
T3 strictly before T2).

## Status

- [x] T1 — script editor dialog standard buttons (Haiku 4.5) — done
      2026-07-18; setDefault + cursor + no icons in both constructors,
      accent :default QSS rule; verified Create dialog on light theme.
- [x] T2 — scripts screen redesign: + New / pencil / x (Opus 4.8) — done
      2026-07-18; new ScriptRowWidget (label + IconButton pencil/x, theme
      recolored) attached via setItemWidget, item text moved to
      Qt::UserRole (fixes doubled text), removeScript refactored to
      name-based, list themed with the T4 %table_*% placeholders; all
      flows (+ New / pencil / x / double-click) exercised in sandbox on
      dark + light. Agent hit a session limit mid-run and was resumed to
      finish the last two fixes.
- [x] T3 — remove "Reset current context" (Haiku 4.5) — done 2026-07-18;
      pushButton_3 + resetShortcuts() removed (sole caller), verified
      Shortcuts tab layout.
- [x] T4 — shortcuts table per-preset colors (Opus 4.8) — done
      2026-07-18; table_bg/bg_alt/header/text/border keys wired
      end-to-end with HSV-derived fallbacks; NOTE: live presets are
      src/res/themes/*.ini — the theme-presets/*.conf files are dead
      (unreferenced); verified Dark/Dark Blue/Black/Light/Light Blue.
      Finding: preset changes only apply to the open settings dialog
      after Apply/OK.
- [x] T5 — dropdowns: −10% height + down arrow (Sonnet 5) — done
      2026-07-18; %combobox_height% placeholder, light/dark
      dropDownArrow assets from the magenta placeholder alpha,
      %icontheme% now follows theme background luminance (also fixes
      branch icons on dark themes); verified both themes.
