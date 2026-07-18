# Settings / Dialog UI Consistency Action Plan

Issues reported 2026-07-18. The reference look the user likes is the **frameless
confirm popup** (e.g. "Move to trash" confirmation = `CustomMessageBox` spawned
from the main window): compact buttons, themed colors, accent on the default
button. Every item below carries an **Execution model**: the best-fit AI model
(with rationale), concrete steps, and verification.

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work.

Verification for all items: build, then the click-driven screenshot flow
(`import` + the XTest `xclick` helper; rebuild it from
`docs/` notes or the previous session's recipe if the job tmp copy is gone —
source is ~20 lines, `cc xclick.c -o xclick -lX11 -lXtst`).

---

## S1. Settings window OK / Apply / Cancel — wrong style and order

**Symptom:** the settings window's bottom-row buttons don't match the liked
CustomMessageBox buttons, and the order is OK, Apply, Cancel.

**Diagnosis:**
- `src/gui/dialogs/settingsdialog.ui:5359-5378`: row order is `OK`,
  `pushButton` (= Apply), `Cancel`. Wanted order: **OK, Cancel, Apply**.
- The only styling they get is `SettingsDialog QPushButton { min-height:
  %button_height%; }` (`style-template.qss:32`) — none of the CustomMessageBox
  look (bg/hover/pressed/radius/padding/accent-default).

**Fix:**
1. In `settingsdialog.ui`, move the `Cancel` `<item>` block before the Apply
   one, and rename `pushButton` → `applyButton` (update every
   `ui->pushButton` reference in `settingsdialog.cpp` — grep first).
2. In `style-template.qss`, add the three bottom-row buttons (scope by
   objectName: `SettingsDialog QPushButton#OK`, `#applyButton`, `#Cancel`) to
   the shared dialog-button groups (base/hover/pressed at ~1192/1207/1218)
   and add `SettingsDialog QPushButton#OK:default` to the accent group
   (~1226). Do NOT restyle other settings buttons (Reset zoom levels, script
   editor buttons, "Reset current context") in this item.
3. Make OK the default button (`setDefault(true)` in settingsdialog.cpp if
   the .ui doesn't already) so the accent rule fires.

**Execution model**

*Model:* **Sonnet 5** — the .ui reorder is mechanical, but the rename touches
`settingsdialog.cpp` call sites and the QSS scoping must not leak onto the
dozen other QPushButtons in the settings window; needs care, not brilliance.

Steps: edit .ui + .cpp + .qss as above → build → screenshot the settings
bottom row next to a "Move to trash" confirm popup → same height, padding,
colors, accent on OK; order reads OK, Cancel, Apply.

## S2. Color picker buttons too big

**Symptom:** after the P1 unification the picker's buttons match the popups'
*style* but render taller/chunkier than the liked trash-confirm buttons.

**Diagnosis:** `SettingsDialog QPushButton { min-height: %button_height%; }`
(`style-template.qss:32`) matches by *descendant* chain, so it inflates
buttons in child dialog windows too: the color picker and any
CustomMessageBox spawned from settings. The trash confirm is spawned from the
main window, escapes the rule, and stays compact — that's the size the user
wants everywhere.

**Fix:** in the shared dialog-button group (~line 1192, which already lists
`CustomMessageBox QPushButton` and `QDialog#ColorPickerDialog QPushButton`),
add `min-height: 0px;`. Equal specificity + later in the file beats the line
32 rule, so nested dialogs collapse back to padding-driven height while the
settings window's own buttons keep `%button_height%` until S1 restyles them.

**Execution model**

*Model:* **Haiku 4.5** — one QSS declaration; the specificity reasoning is
already done above. Escalate to Sonnet 5 only if the override provably
doesn't win (paste computed sizes in the report).

Steps: add the declaration → build → screenshots: picker buttons vs
trash-confirm buttons same height; settings-spawned popups (preset switch)
also match. Run S2 before S1 so S1's parity check is against final sizes.

## S3. Global button order: OK, Cancel, [Apply]

**Symptom:** button order is inconsistent across dialogs. Wanted everywhere:
**OK, Cancel, Apply** (Apply only where it exists).

**Diagnosis / sites:**
- `CustomMessageBox::confirm()` and `getText()`
  (`src/gui/dialogs/custommessagebox.cpp:85-116`) add reject first → renders
  "Cancel OK". Must become accept-first ("OK Cancel" / "Trash Cancel").
- `colorselectorbutton.cpp:46`: `QDialogButtonBox(Ok | Apply | Cancel)`
  renders Apply, Cancel, OK (style-driven order that QSS can't change).
  Replace the button box with a plain `QHBoxLayout` + three QPushButtons
  added in OK, Cancel, Apply order (keeps the existing accept/reject/apply
  connects; keep cursor + no-icon + OK-default behavior from commit
  76968e57).
- Sweep the remaining dialog .ui files for row order: `resizedialog.ui`,
  `scripteditordialog.ui`, `shortcutcreatordialog.ui`, `printdialog.ui`,
  `filereplacedialog.ui` (its buttons are semantic — Replace/Skip/Cancel —
  reorder only if it has a plain OK/Cancel/Apply-style row).
- Settings window row itself is handled in S1 — excluded here.

**Execution model**

*Model:* **Sonnet 5** — replacing a QDialogButtonBox without breaking the
Apply-preview/revert logic in the color picker requires reading that signal
flow; the .ui sweep needs judgment about which rows are semantic vs standard.

Steps: change custommessagebox.cpp order → rework colorselectorbutton.cpp
button row → sweep .ui files → build → screenshot every dialog's button row
(trash confirm, getText, picker, resize, script editor, shortcut creator,
print) and confirm OK-first ordering with unchanged behavior (Apply still
live-previews, Cancel still reverts).

## S4. Settings dropdowns (QComboBox) not themed

**Symptom:** every dropdown in settings — theme preset, shortcuts preset,
language, grid default sorting, panel position, scaling quality, image
scrolling — renders with default/native styling (light popup, unthemed box).

**Diagnosis:** `SettingsDialog QComboBox` (`style-template.qss:36`) is an
empty rule (only a commented-out min-height). The only global combobox rule
is `QComboBox QAbstractItemView { outline: 0; }` (line 210). Themed combobox
QSS exists but is scoped to `ResizeDialog`/`ShortcutCreatorDialog`
(lines ~1117-1155) and `SidePanel` (~411-445).

**Fix:** fill in `SettingsDialog QComboBox` (+ `:hover`, `::drop-down`,
`QAbstractItemView`, `::item`, `::item:selected`) mirroring the
ResizeDialog/ShortcutCreatorDialog block (colors `%button%`, `%button_hover%`,
popup `%widget%` + `%accent%` border, item selected `%accent_hover_rgba%`).
Consider merging all three scoped blocks into one shared selector group while
touching it — same pattern as the dialog-button unification — rather than a
fourth copy. Min-height should match `%button_height%` so boxes align with
neighboring controls.

**Execution model**

*Model:* **Sonnet 5** — QSS-only but across seven comboboxes on five settings
tabs plus a popup view; combobox QSS has interaction traps (`::drop-down`
resets native arrow, item view padding), so it needs the screenshot loop
across multiple tabs, not a one-shot edit.

Steps: write QSS → build → open every settings tab with a dropdown, open each
popup, screenshot; closed box, hover, open list, selected item all themed;
re-check ResizeDialog/ShortcutCreator dialogs for regressions if blocks were
merged.

## S5. About screen text unreadable on dark / dark-blue themes

**Symptom:** body text on Settings → About is nearly invisible on the dark
and dark-blue theme presets.

**Diagnosis:** `aboutAppTextBrowser` (`settingsdialog.ui:5249`) is a
QTextBrowser whose viewport background is made transparent in
`settingsdialog.cpp:111`, but **no rule sets its text color** — the only
themed QTextBrowser is `ChangelogWindow QTextBrowser`
(`style-template.qss:454`). Rich-text body spans carry no inline color, so
they fall back to the default (near-black) palette text on the themed dark
background. Link spans are hardcoded `#007af4` in the .ui HTML.

**Fix:**
1. Add `SettingsDialog QTextBrowser { color: %text%; background:
   transparent; }` to the QSS (covers the body text; rich text without
   inline color follows the widget palette).
2. Check the same page's plain QLabels (`versionLabel`, `qtVersionLabel`,
   "About thumbgrid" header) on both dark presets; add `color: %text%` rules
   if any are also palette-defaulted.
3. Optional polish if the hardcoded `#007af4` links look muddy on dark-blue:
   strip inline link colors from the .ui HTML and set link color via
   `document()->setDefaultStyleSheet("a { color: %accent%; }")`-style wiring
   in settingsdialog.cpp (needs the theme color at runtime, not .ui).

**Execution model**

*Model:* **Haiku 4.5** for steps 1-2 (pinned QSS additions + two-theme
screenshot check). If step 3 turns out to be needed (links illegible),
escalate that step to **Sonnet 5** — it moves color handling from static .ui
HTML into runtime code next to the theme plumbing.

Steps: add QSS → build → switch theme preset to dark, screenshot About; to
dark-blue (theme-dark-v2), screenshot About; body text must read clearly on
both; links legible; light theme unregressed.

---

## Suggested order

S2 → S1 → S3 → S4 → S5. S2 first so S1's size parity check is against final
button sizes; S3 builds on S1's row; S4 and S5 are independent.

## Status

- [ ] S1 — settings OK/Cancel/Apply style + order (Sonnet 5)
- [ ] S2 — shrink nested-dialog buttons / min-height leak (Haiku 4.5)
- [ ] S3 — global button order OK, Cancel, [Apply] (Sonnet 5)
- [ ] S4 — theme all settings dropdowns (Sonnet 5)
- [ ] S5 — About screen text color on dark themes (Haiku 4.5, step 3 → Sonnet 5)
