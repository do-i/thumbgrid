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

- [ ] `src/components/directorymodel.cpp` ×3 (watcher-ordering guards →
      explicit event sequencing in DirectoryManager)
- [ ] `src/gui/customwidgets/thumbnailview.cpp` ×3 (layout waits →
      `QTimer::singleShot(0)` or polish-time geometry passes)
- [ ] The rest per the FIXME grep.

*Model:* **Opus 4.8** — behavioral risk, event-ordering reasoning.

### C2. `performance-enum-size` warnings (~30 sites)

From `code-analysis-action-plan.md` §4 / `touch-ups-action-plan.md` T7,
carried via 007 B3. Deliberately skipped as low-value; address only when a
flagged header is being edited anyway. *Model:* **Haiku 4.5**, folded into
whatever session touches the header.

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

### C8. Grid-context `I` keybinding for File info

From plan 009's opt-in follow-up (recorded 2026-07-24). Presets bind
`toggleImageInfo` to `I` in the `document` context only, so in grid view
the File info popup opens from the menu but not the keyboard. `I` is free
in the grid context of the qimgv preset, but the `adjustFromVersion()`
backfill only seeds bindings for *new* actions — extending an existing
action into a new context needs either a version bump (risk: resurrects
deliberately-removed bindings) or a bespoke migration. Decide deliberately
before touching it. *Model:* **Opus 4.8** (migration semantics).

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
