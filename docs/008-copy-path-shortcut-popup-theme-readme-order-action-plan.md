# 008 — Copy Path Menu Entry, Shortcut Popup Theming, README Install Order

Three independent items (2026-07-23): expose "copy absolute path" in both
right-click context menus, fix the unthemed key-edit popup on the Shortcuts
settings page, and reorder the README installation section to
AUR → custom pacman repo → compile from source.

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work.

---

## A1. "Copy path" entry in both context menus

Right-clicking should offer copying the selection's absolute path(s) to the
clipboard, in both the document view and the grid (folder) view. In grid
view, when the selected item is a folder, the copied path is that folder's
path.

**Most of the machinery already exists.** The action `copyPathClipboard` is
registered (`src/utils/actions.cpp:68`), wired
(`src/core.cpp:213`) and implemented (`Core::copyPathClipboard`,
`src/core.cpp:597`): it copies `currentSelection()` joined by newlines and
shows a "Path copied" message. `currentSelection()` (`src/core.cpp:1033`)
already does the right thing per view — in folder view it returns
`folderViewPresenter.selectedPaths()`, which includes folders (so the
folder-path requirement is free); in document view it returns the current
file path. The action is only missing from the two menus.

Steps:

- [ ] **Document view menu** — add a `ContextMenuItem` to
      `src/gui/contextmenu.ui` (main page, near `showLocation` /
      "Show in folder") and configure it in `src/gui/contextmenu.cpp` the
      same way as its neighbours: `setAction("copyPathClipboard")`,
      `setText(tr("Copy path"))`, icon
      `:/res/icons/common/menuitem/copy16.png`. Add it to
      `setImageEntriesEnabled()` so it greys out with the other
      file-dependent entries when nothing is loaded.
- [ ] **Grid view menu** — in `src/gui/folderview/gridcontextmenu.cpp`,
      add a `copyPathItem = makeItem(tr("Copy path"), ...)` with
      `setAction("copyPathClipboard")` on the main page (suggested slot:
      after "Rename...", before "Move to..."), declare the member in
      `gridcontextmenu.h`, and gate it in `setSelectionInfo()` with
      `info.total() > 0`.
- [ ] **Fix the empty-dir gate** — `Core::copyPathClipboard` early-returns
      on `model->isEmpty()`, which counts only image files (see the comment
      in `Core::showRenameDialog`, `src/core.cpp:1065`): copying a folder's
      path in a directory containing only folders would silently do nothing.
      Drop the `model->isEmpty()` check; the existing `paths.isEmpty()`
      check is the correct gate.
- [ ] **Tests** — extend
      `src/tests/behavior/test_grid_context_menu_gating.cpp` to cover the
      new item's enabled/disabled gating, and add a behavior check that
      invoking the action with a folder selected puts the folder's absolute
      path on the clipboard.
- [ ] **Translations** — new `tr()` strings; refresh the `.ts` files the
      same way as commit `fdacfa1b` (lupdate pass).

*Model:* **Sonnet 5** — standard coding; the pattern is established by the
neighbouring menu items, but it spans `.ui` + two menus + a gating fix +
tests. No design judgment needed, so Opus is overkill; too many coordinated
touch points for Haiku.

- [x] A1 done

## A2. Shortcut key-edit popup is unthemed

Settings → Shortcuts → click a row's Key cell → the "shortcut details"
popup renders with native/unthemed styling instead of the app theme.

**Root cause.** `SettingsDialog::openShortcutDetails`
(`src/gui/dialogs/settingsdialog.cpp:1358`) builds an anonymous inline
`QDialog` (plain `QLabel`s, `QRadioButton`s, `QToolButton` "Remove"
buttons, a `KeySequenceEdit`, a `QDialogButtonBox`). The theme stylesheet
is applied app-wide (`qApp->setStyleSheet`, `src/settings.cpp:962`) but
`src/res/styles/style-template.qss` scopes its dialog rules by class or
objectName (`SettingsDialog QLabel`, `ShortcutCreatorDialog ...`,
`QDialog#ColorPickerDialog`), so nothing matches this dialog's own window
background or its controls.

Steps:

- [ ] Give the dialog an identity:
      `dialog.setObjectName("ShortcutDetailsDialog")`.
- [ ] Add a `QDialog#ShortcutDetailsDialog` block to
      `style-template.qss`, mirroring the existing
      `QDialog#ColorPickerDialog` / `ShortcutCreatorDialog` blocks:
      window background, `QLabel` / `QRadioButton` text colors, the
      `QToolButton` remove buttons, `KeySequenceEdit` (check whether the
      existing `ShortcutCreatorDialog` rules for it can be shared by
      extending their selector lists rather than duplicating values),
      and the `QDialogButtonBox` push buttons.
- [ ] Mind the known QSS gotchas: autoDefault buttons match `:default`
      (see [[qt-qss-uic-gotchas]] memory); the OK button in a
      `QDialogButtonBox` is a default button.
- [ ] Verify offscreen on light + dark presets with a pixel-diff render,
      per the established workflow — do not drive the live X session.

*Model:* **Sonnet 5** — the fix pattern (objectName + scoped QSS block) is
already established twice in the codebase; the work is careful selector
bookkeeping plus offscreen verification. Escalate to Opus 4.8 only if the
popup turns out to need structural rework (e.g. converting it into a real
dialog class).

- [x] A2 done

## A3. README installation order: AUR → custom repo → compile from source

`README.md`'s Installation section currently reads: intro → "Build from
source (GNU+Linux)" → "Arch Linux package" (custom pacman repo first,
then AUR, then manual `.pkg.tar.zst`, then PKGBUILD). Reorder so readers
meet the options as: **AUR** first, **custom pacman repo** second,
**compile from source** last.

Steps:

- [ ] Restructure the Installation section: AUR (`thumbgrid-bin`) first,
      then the custom pacman repo (`[thumbgrid]` in `pacman.conf`), then
      the manual `.pkg.tar.zst` fallback, then "Build from source"
      (generic CMake + `run.sh`) with the `packaging/arch/` PKGBUILD note
      folded in as the source-build path for Arch users.
- [ ] Rewrite the intro paragraph to match the new order (it currently
      leads with the pacman repo). Keep all factual claims intact — the
      AUR package lags stable promotions while the pacman repo tracks
      every release and ABI rebuild; whether the "Recommended" tag stays
      on the pacman repo or moves is presentation, but the trade-off text
      must survive the reorder.
- [ ] Check intra-document anchors still resolve (the intro links to
      `#arch-linux-package`; renamed headings need their links updated).

*Model:* **Haiku 4.5** — mechanical documentation restructure with no code
impact; all content already exists and is only being reordered/re-worded.

- [x] A3 done
