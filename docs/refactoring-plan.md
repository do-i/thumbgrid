# Refactoring plan

Findings from a codebase survey (2026-07-13), ordered by payoff. Each item is
committed separately; this file tracks progress.

## 1. Delete dead ThumbnailWidgetCmp — [x]

`src/gui/customwidgets/thumbnailwidgetcmp.{h,cpp}` (~570 lines) is referenced by
nothing and absent from `src/gui/CMakeLists.txt`, so it is not even compiled.
It is a stale copy of `ThumbnailWidget` (it redefines the `ThumbnailStyle`
enum). Delete both files.

## 2. Deduplicate Core file operations — [x]

All in `src/core.cpp`:

- [x] `doInteractiveCopy` / `doInteractiveMove`: ~85-line near-identical
      recursive functions. Merge into one that takes the operation as a
      parameter; move additionally removes the source dir afterwards.
- [x] Within the merged function, the symlink and single-file branches repeat
      the same "attempt → replace-dialog on DESTINATION_FILE_EXISTS → retry →
      report → reset flag" block four times. Extract a helper.
- [x] `removePermanent` / `moveToTrash`: identical except a `trash` bool and
      three strings. Merge.
- [x] `moveCurrentFile` / `copyCurrentFile`: same shape, share the
      exists/overwrite flow.
- [x] Replace the pasted `mw->showError(decodeResult(...)); qDebug() << ...`
      idiom with the existing `outputError()`.

## 3. Extract file-operation code from Core — [x]

Core (1964 lines) mixes navigation, slideshow, clipboard, DnD, file ops, image
editing, and dialog orchestration. Extract the interactive file-operation code
(copy/move/remove/rename/paste/convert and their confirm-dialog flows) into a
dedicated `FileOperationsController` owned by Core. Do this after item 2 so the
moved code is already deduplicated.

## 4. Reduce settings.cpp boilerplate — [x] (no change needed)

`src/settings.cpp` (2031 lines) hand-writes ~230 getters/setters around
QSettings. On closer inspection the typed-helper layer this item asked for
already exists: every trivial accessor is a one-liner over
`readSetting(key, default)` / `writeSetting(key, value)`, and the remaining
raw `settingsConf` accesses are deliberate (migration, theme tokens, the
Scripts array group). Collapsing the ~110 remaining one-line accessor
*methods* would require macros or codegen, which trades greppability and
debuggability for line count — not worth it. No change made.

## 5. Split SettingsDialog setup — [ ]

`src/gui/dialogs/settingsdialog.cpp`: 161-line constructor plus 134-line
`readSettings` and 129-line `saveSettings`. Split the constructor into
per-page `setupXxxPage()` methods so each page's wiring lives in one place.

## 6. Smaller items — [ ]

- [ ] `Settings::loadStylesheet()` (139 lines): split the stylesheet assembly
      into per-widget-group helpers.
- [ ] `ImageViewerV2::wheelEvent` (94 lines): split scroll/zoom dispatch into
      named handlers.
- [ ] `ContextMenu` vs `GridContextMenu`: evaluate a shared base class; they
      have diverged, so only do this if it genuinely simplifies. Record the
      decision here either way.
