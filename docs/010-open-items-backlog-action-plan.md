# 010 — Open Items Backlog

Every non-completed item, carried forward from plan 007 (2026-07-24
consolidation). Plans 007, 008 and 009 have since been completed in full
(ResizeDialog title clip, copy-path menu entry, shortcut popup theming,
README install order, CustomMessageBox danger role + destructive-confirm
audit, the File info popup redesign) and were removed as part of this
cleanup; their history lives in git.

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work.

---

## Opportunistic — do when already in the file, never in bulk

### C1. The 18 kept `processEvents()` sites

From `touch-ups-action-plan.md` T6 (residue of code-analysis §6), carried
via 007 B2. All carry the greppable marker `// FIXME: re-entrancy hazard
(processEvents)`. Each synchronizes with real async state and needs a
per-site restructure plus interactive verification. One site at a time.
Highest value first:

- [x] `src/components/directorymodel.cpp` ×3 — done 2026-07-24. Removed
      outright. The racing watcher event for `moveFileTo`/`moveSymLinkTo` sits
      behind a 150 ms inotify debounce (`linuxwatcher.cpp` `EVENT_MOVE_TIMEOUT`),
      so `processEvents()` provably never flushed it — it was pure re-entrancy
      hazard with no ordering benefit. `renameEntry` now mutates eagerly and
      lets the later watcher event be absorbed by DirectoryManager's idempotent
      guards; `updateFileEntry()` only emits `fileModified` when the entry
      actually changed, so that absorption no longer causes a spurious reload.
- [x] `src/gui/customwidgets/thumbnailview.cpp` ×3 — done 2026-07-24. Replaced
      by a re-armable 0-ms `layoutSettleTimer` (`onLayoutSettled()`), which also
      made `populate()` atomic with respect to the event loop — previously a
      queued handler could run against half-destroyed thumbnail widgets.
      Guarded by "The grid re-enables painting after every populate so it never
      stays blank"; verified by mutation, and confirmed that the three closest
      pre-existing tests do *not* catch the regression.
- [x] `src/main.cpp` ×1, `src/core.cpp` ×1 — done 2026-07-24. Both ran *before*
      `QApplication::exec()`, so they hand-drained a queue that `exec()`
      processes anyway; `showGui()` already defers `setupFullUi()` behind a
      50 ms timer, so the pump bought nothing.
- [x] `src/gui/mainwindow.cpp` ×1 (`preShowResize`) — done 2026-07-24. Removed;
      its own comment already read "not needed anymore with patched qt?".
- [x] `src/gui/viewers/imageviewerv2.cpp` ×1 (`showEvent`) — done 2026-07-24.
      Now `QTimer::singleShot(0, this, &ImageViewerV2::applyFitMode)`, matching
      the deferral this class already uses for `centerOnPixmap()`.

**10 of 17 done. 7 remain**, all needing more care than a deletion — and all
sharing one blocker: their observable effect is *visual* (repaint latency,
animation smoothness, focus position, window-manager geometry timing), so the
"interactive verification" this item has always called for is genuinely
required. They were deliberately left rather than changed blind. Each entry
below records the fix shape so the next session does not have to re-derive it.

Note on `showFullScreen`/`showWindowed` specifically: `QWidget::isFullScreen()`
is set synchronously by `showFullScreen()`, so `adaptToWindowState()` reads the
correct state without the pump — the emit ordering is *not* the risk. The risk
is that the pump also lets the window manager's resize arrive before
`adaptToWindowState()` lays out, so removing it may cause a transient
mislayout that only corrects on the next resize event. That needs eyes on a
real window.

- [ ] `src/gui/mainwindow.cpp` ×2 (`showFullScreen`/`showWindowed`) — the pump
      applies the new geometry *before* `fullscreenStateChanged` is emitted, so
      subscribers may rely on it. Deferring the emit is the likely fix.
- [ ] `src/gui/folderview/folderviewproxy.cpp` ×2 and
      `src/gui/panels/mainpanel/thumbnailstripproxy.cpp` ×2 — coupled to the
      ThumbnailView change above. Both do `populate()` → `processEvents()` →
      `focusOnSelection()`, and that pump is currently what drives the new
      settle timer. Removing it requires moving `focusOnSelection()` into the
      settle path, not just deleting the call. The `init()` pumps additionally
      guard `stateBuf` against queued mutations before the mutex is taken.
- [ ] `src/gui/customwidgets/slidepanel.cpp` ×1 — inside an animation frame
      handler; pumping there re-enters the animation. Needs the frame driver
      examined, not a blind removal.

*Model:* **Opus 4.8** — behavioral risk, event-ordering reasoning.

### C2. `performance-enum-size` warnings (41 sites, surveyed 2026-07-24)

From `code-analysis-action-plan.md` §4 / `touch-ups-action-plan.md` T7,
carried via 007 B3. Originally "address only when a flagged header is being
edited anyway". A full survey has since been done, and it removes most of the
reason for that caution — the sweep is mechanically safe:

- **41 project enums**, all currently implicit `unsigned int`. 40 want
  `std::uint8_t`; exactly one, `DuplicateResultsModel::Roles`, wants
  `std::uint16_t` (its `SortRole = Qt::UserRole` is 256 — do not reflexively
  write `uint8_t` there).
- The codebase has **no `Q_ENUM`/`Q_ENUMS`/`Q_FLAG`/`Q_DECLARE_FLAGS`, no
  `QFlags`, no enum bitfields, no enum streamed through `QDataStream`, and no
  enum crossing a queued signal/slot boundary** — so none of the usual
  width-shrinking hazards applies.
- Settings persistence is safe by construction: enums reach disk via
  `QVariant(int)` integral promotion and return through `toInt()` plus a range
  clamp, so a fixed underlying type leaves the on-disk bytes identical.
  `PanelPosition` is stored as a *string*; `ViewMode` keys shortcut JSON by
  string token, never by number.
- `scriptmanager.cpp` has an unnamed function-local enum needing
  `enum : std::uint8_t { … }` rather than a header edit.
- No header currently includes `<cstdint>`, so the sweep must add it wherever
  it introduces `std::uint8_t`.
- **Skip** `src/3rdparty/QtOpenCV/cvmatandqimage.h` (vendored — merge friction,
  no benefit).

*Model:* **Haiku 4.5** — mechanical, now that the risk analysis is done.

## Parked — blocked on an external precondition

### C3. `windowswatcher.cpp` old-style connects

From `touch-ups-action-plan.md` T2, carried via 007 B4. Converting
SIGNAL/SLOT strings safely requires a Windows build; parked until Windows
CI exists. *Model:* **Haiku 4.5** once buildable.

### C4. `directorypresenter.cpp` connects through `dynamic_cast<QObject*>`

From `touch-ups-action-plan.md` T2 (optional sub-item), carried via 007 B5.
Six connects go through the cast because `IDirectoryView` is a non-QObject
interface; a clean fix changes the interface shape. Only worth it if
`IDirectoryView` grows; signatures are covered by the review cross-check.
*Model:* **Sonnet 5** if ever picked up.

## Deferred by decision — revisit only if the premise changes

### C5. Frameless conversion of the remaining native-framed dialogs

From `dialog-theming-action-plan.md` D5, carried via 007 B6. Decision
(2026-07-17): only FileReplaceDialog was converted; SettingsDialog,
ResizeDialog, ScriptEditorDialog, ShortcutCreatorDialog, PrintDialog,
ChangelogWindow and FileInfoDialog keep native frames (content is already
themed). Full conversion = `FramelessWindowHint` + translucency +
`PE_Widget` paint + draggable title strip per dialog — cosmetic-only and
risky on Settings. Revisit only if a fully bespoke window look is wanted
app-wide. *Model:* **Opus 4.8**.

Related recorded decision, not open work: file pickers stay **native**
(D3, 2026-07-17) — native Recent/Places integration outweighs theming.

### C6. Small mixed-platform conditionals

From `os-specific-implementation-cleanup-plan.md`, carried via 007 B7 (all
primary splits done). Left as-is on purpose; revisit a file only if its
platform branching grows: `main.cpp` (macOS app object),
`videoplayerinitproxy.cpp` (plugin search paths), `actionmanager.cpp`
(Apple defaults data selection), `shortcutbuilder.cpp` (scan-code vs text
branch — tied to InputMap). Note: `utils/stuff.{h,cpp}` from that inventory
no longer exist (retired by touch-ups T4 into `utils/pathstring.h`).
*Model:* **Sonnet 5** per file.

### C7. ContextMenu / GridContextMenu shared base class

From `refactoring-plan.md` item 6, carried via 007 B8. Evaluated and
declined (they differ in 262 of 372 lines); revisit only if the two menus
start growing parallel features again. *Model:* **Opus 4.8** (API design
across both menus).

### C8. Grid-context `I` keybinding for File info — DONE (2026-07-24)

Decided: implemented, via the bespoke-migration route.

The feared risk (a version bump resurrecting deliberately-removed bindings)
does not apply to a *targeted* migration, and the premise checked out: `I` is
free in the grid context of **all five** presets and none of them binds
`toggleImageInfo` there, so no user can have deliberately removed a binding
that never existed.

- `grid.toggleImageInfo = ["I"]` added to `qimgv.json` and `xnviewmp.json`
  only — the other three presets (`gwenview`, `irfanview`, `leftie`) do not
  bind File info in any context, so forcing it on them would override preset
  intent.
- A state-conditional block in `adjustFromVersion()`, following the existing
  `MiddleButton=exit` idiom, fires only when `I` is still free in grid **and**
  the user's document context actually maps `I` to `toggleImageInfo`. Users who
  rebound `I`, and users on a preset that never binds File info, are untouched.
- The generic backfill could not do this: it only seeds actions whose
  introduction version is newer than the user's last version, and
  `toggleImageInfo` dates to 0.7.84.

---

## Retired source files

`code-analysis-action-plan.md`, `touch-ups-action-plan.md`,
`context-menu-action-plan.md`, `dialog-theming-action-plan.md`,
`os-specific-implementation-cleanup-plan.md`, `popup-polish-action-plan.md`,
`refactoring-plan.md`, `scripts-shortcuts-dropdown-polish-action-plan.md`,
`settings-ui-consistency-action-plan.md`,
`settings-theming-shortcut-binding-action-plan.md`,
`001-shortcuts-page-audit-fixes-action-plan.md`,
`003-duplicate-finder-design-plan.md`,
`004-stored-data-cleanup-action-plan.md`, `005-aur-package-action-plan.md`,
`006-aur-release-workflow-fixes-action-plan.md`,
`007-open-items-backlog-action-plan.md`,
`008-copy-path-shortcut-popup-theme-readme-order-action-plan.md`,
`009-file-info-popup-redesign-action-plan.md` — all completed items are in
git history.
