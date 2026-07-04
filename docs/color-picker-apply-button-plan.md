# Color picker "Apply" button — implementation plan

## Goal

In the theme color picker, let the user **apply a color and see it in the running
app without closing the picker**, so they can trial-and-error colors visually.
Only OK/Cancel should close the dialog.

## Why this is not trivial

Two independent gaps have to be closed:

1. **The dialog is the buttonless static call.**
   `ColorSelectorButton::showColorSelector()` uses the blocking
   `QColorDialog::getColor(...)`. That static helper only ever returns a final
   color on OK (or invalid on Cancel) — we cannot inject an "Apply" button or
   observe intermediate picks. We need a persistent, non-static `QColorDialog`
   instance (or a small wrapper dialog) with `QColorDialog::NoButtons` +
   our own OK / Cancel / **Apply** button box.
   Files: `src/gui/customwidgets/colorselectorbutton.{h,cpp}`

2. **`colorChanged` today does not re-theme the app.**
   Downstream, every `ColorSelectorButton::colorChanged` is wired only to
   `markThemeCustom()` in `settingsdialog.cpp` — it flips the theme combo to
   "Custom" but does **not** apply the palette. The palette is only pushed to
   the live app inside `saveSettings()` → `settings->setColorScheme(...)` →
   `emit settingsChanged()`. So "Apply" clicked inside the picker must reach a
   settings-dialog path that assembles the current palette and applies it live
   **without** persisting to disk or closing the dialog.
   Files: `src/gui/dialogs/settingsdialog.{h,cpp}`

Good news: the assembly logic already exists — `SettingsDialog::collectColorScheme()`
reads all selectors into a `ColorScheme`, and `settings->setColorScheme(scheme)` +
`emit settingsChanged()` is exactly the live-apply mechanism. We reuse both.

## Design

### ColorSelectorButton (the picker)

- Replace the static `getColor` call with a member `QColorDialog *mDialog`
  constructed lazily with `QColorDialog::NoButtons | DontUseNativeDialog`.
- Add our own `QDialogButtonBox` with **Apply**, **OK**, **Cancel** (either as a
  thin `QDialog` wrapper hosting the color dialog as a child widget, or by
  embedding the `QColorDialog` widget and adding the button box in a layout).
- New signal: `void colorApplied(QColor color);`
  - **Apply** → set `mColor = mDialog->currentColor(); update();` and
    `emit colorApplied(mColor)` — dialog stays open.
  - **OK** → same as Apply, then `emit colorChanged(mColor)` and close.
  - **Cancel** → close without changing `mColor`; caller reverts live preview
    (see revert handling below).
- Keep the existing `colorChanged` semantics for OK so nothing else regresses.

### SettingsDialog (live apply)

- Add a slot `applyColorSchemePreview()` that does:
  `settings->setColorScheme(collectColorScheme()); emit settingsChanged();`
  (note: **no** `settings->saveTheme()`, so nothing is written to disk).
- Wire every selector's new `colorApplied` signal to
  `markThemeCustom()` + `applyColorSchemePreview()`.
- **Revert-on-Cancel / dialog-cancel:** capture the active palette when the
  settings dialog opens (there is already `mCustomColors` / `readColorScheme()`
  seeding). If the user cancels the *settings* dialog, restore the original
  scheme + `emit settingsChanged()` so live previews don't leak. Per-color
  Cancel inside the picker is naturally covered because `mColor` is untouched
  and the next Apply/OK re-collects from selectors.

## Scope / risk

- **Touched files:** `colorselectorbutton.{h,cpp}` (new dialog + signal),
  `settingsdialog.{h,cpp}` (new slot + 17 connect lines).
- **Behavioral risk:** live `emit settingsChanged()` re-runs theme application
  on every Apply. That is the same code path as saving, so it is exercised
  already — main risk is doing it repeatedly (fine) and making sure Cancel of
  the settings dialog restores the pre-edit palette.
- **No disk writes** happen on Apply; persistence still only occurs on the
  settings dialog's Save/OK. This keeps trial-and-error non-destructive.
- **Effort:** ~M. One new small dialog wrapper + one settings-dialog slot +
  revert bookkeeping. No new data structures; reuses `collectColorScheme()`.

## Open questions for review

1. **Apply button, or live-on-drag?** An explicit Apply gives control and avoids
   re-theming the whole app on every slider tick. Alternative: connect
   `QColorDialog::currentColorChanged` for continuous live preview (no button).
   Plan above assumes the explicit **Apply** button you asked for.
2. **Revert scope:** should canceling the settings dialog after several Applies
   restore the palette that was active when the dialog opened? (Plan assumes
   yes.)
3. Wrapper `QDialog` vs. embedding `QColorDialog` widget directly — cosmetic;
   embedding avoids a second window frame.
