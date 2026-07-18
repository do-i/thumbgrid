# Dialog / Popup Theming Action Plan

Goal: every popup, modal, and secondary window should match the app's custom
color scheme instead of falling back to native OS chrome or default Qt styling.

The app themes widgets through one global stylesheet built from
`src/res/styles/style-template.qss` and applied via `qApp->setStyleSheet()` in
`Settings::loadStylesheet()`. That reaches any widget rendered by Qt, but it
does **not** reach:

1. **Native OS dialogs** (`QFileDialog`, `QColorDialog`, `QInputDialog` when
   native) — these are drawn by the desktop environment, so QSS can't touch them.
2. **Native window chrome** — even a fully color-themed `QDialog` is still a real
   top-level window wearing the window manager's title bar and frame, unlike the
   app's custom frameless overlays (context menus, floating messages).
3. **Standard severity icons** on `QMessageBox` (`Warning`/`Critical` pixmaps).

The reusable fix for #2 and #3 is `CustomMessageBox`
(`src/gui/dialogs/custommessagebox.{h,cpp}`): a frameless, translucent, rounded,
fully QSS-themed dialog matching the `ContextMenu` aesthetic. Its styles live in
the `CustomMessageBox` block of `style-template.qss`.

---

## Done

- [x] **`MW::showConfirmation`** (rename / overwrite prompt) — now
      `CustomMessageBox::confirm()`. `src/gui/mainwindow.cpp`
- [x] **`MW::showErrorDialog`** — now `CustomMessageBox::message()`.
      `src/gui/mainwindow.cpp`

---

## D1. Remaining stock `QMessageBox` calls — **easy, high value**

These are color-themed by the QSS `QMessageBox` rules but still show a native
title bar + native severity icon. Route them through `CustomMessageBox`.

- [x] `src/gui/dialogs/settingsdialog.cpp:129` — `QMessageBox::information` (version
      popup) → `CustomMessageBox::message(this, tr("Version"), ...)`.
- [x] `src/gui/dialogs/settingsdialog.cpp:404` — `QMessageBox::question` (switch
      shortcut preset confirmation) → `CustomMessageBox::confirm(this, ...)`.
- [x] `#include <QMessageBox>` dropped from `settingsdialog.cpp`.

## D2. `QInputDialog` text prompt — **easy, high value**

- [x] `src/core.cpp:1089` — `QInputDialog::getText(...)` for the "New Folder" name
      → `CustomMessageBox::getText(...)`. Added `addInput()` + `getText()` to
      `CustomMessageBox` (themed `QLineEdit` in the body, Enter accepts, initial
      text pre-selected) plus `CustomMessageBox QLineEdit` QSS rules. `QInputDialog`
      / `QLineEdit` includes dropped from `core.cpp`.

## D3. Native `QFileDialog` usage — **decided: keep native**

All file pickers use the **native** OS dialog (none set
`QFileDialog::DontUseNativeDialog`), so they follow the desktop theme, not the
app theme. **Decision (2026-07-17): keep them native** — native pickers preserve
Recent/Places integration users expect, and Qt's non-native dialog is only
partially covered by the app QSS. Left as-is intentionally; not a defect.

Call sites (for reference, should any future change revisit this):
`core.cpp:870` (Move to…), `mainwindow.cpp:594` (Save as), `mainwindow.cpp:601`
(Open), `folderview.cpp:333` (add bookmark), `scripteditordialog.cpp:75`,
`settingsdialog.cpp:1442`, `printdialog.cpp:72` (pdf location),
`pathselectormenuitem.cpp:12`.

## D4. `QColorDialog` — **low priority**

- [ ] `src/gui/customwidgets/colorselectorbutton.cpp:34` — already embedded with
      `DontUseNativeDialog` inside a custom dialog, so the outer shell is ours,
      but `QColorDialog`'s own internal controls aren't fully QSS-styled. Polish
      only if the settings color picker looks out of place.

## D5. Themed-but-native-framed `QDialog` subclasses — **optional consistency pass**

These already get scheme colors on their content, but each is a normal top-level
`QDialog` with the OS title bar and frame — so next to `CustomMessageBox` / the
context menu they still read as "system dialogs."

- [x] `FileReplaceDialog` (`src/gui/dialogs/filereplacedialog.*`) — used by
      `MW::fileReplaceDialog` for copy/move overwrite conflicts. **Converted** to
      frameless + translucent + rounded (`PE_Widget` paint pass), bold themed
      title, accent default button, `startSystemMove()` drag, centered on parent.
- [ ] `SettingsDialog`  (`src/gui/dialogs/settingsdialog.*`) — deferred (native frame)
- [ ] `ResizeDialog`    (`src/gui/dialogs/resizedialog.*`) — deferred (native frame)
- [ ] `ScriptEditorDialog` (`src/gui/dialogs/scripteditordialog.*`) — deferred
- [ ] `ShortcutCreatorDialog` (`src/gui/dialogs/shortcutcreatordialog.*`) — deferred
- [ ] `PrintDialog`     (`src/gui/dialogs/printdialog.*`) — deferred
- [ ] `ChangelogWindow` (`src/gui/overlays/changelogwindow.*`) — deferred

**Decision (2026-07-17): FileReplaceDialog only.** The rest keep their native
frames for now (content already themed). Full frameless theming means
`Qt::FramelessWindowHint` + `WA_TranslucentBackground` + a `PE_Widget` paint pass
+ a draggable title strip — bigger effort, cosmetic-only, and riskier on complex
dialogs like Settings; revisit if a fully bespoke window look is wanted.

---

## Status (2026-07-17)

- **D1** ✅ Settings version/preset-switch popups → `CustomMessageBox`.
- **D2** ✅ New Folder prompt → `CustomMessageBox::getText`.
- **D3** ✅ Decided: file pickers stay native.
- **D4** ✅ Color-picker wrapper body themed (native frame kept).
- **D5** ✅ FileReplaceDialog converted to frameless themed; other 5 dialogs +
  Changelog deferred by decision (native frames, content already themed).

Remaining open items are all **deferred by decision**, not pending work: the D5
frameless conversion of Settings / Resize / ScriptEditor / ShortcutCreator /
Print / Changelog. Revisit only if a fully bespoke (frameless) window look is
wanted across the app.
