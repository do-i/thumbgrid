# 002 — Open Items Backlog

Every non-completed item gathered from the retired action plan files
(2026-07-18 consolidation). Provenance is noted per item; the source files
are deleted — this file is now the single home for open work. Newly found
bugs from the commit audit live separately in
`001-shortcuts-page-audit-fixes-action-plan.md`.

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work.

---

## Actionable now

### B1. ResizeDialog clipped group-box title

From `settings-theming-shortcut-binding-action-plan.md` (U7 regression-pass
finding, 2026-07-18). A group-box title in ResizeDialog renders clipped on
all presets; A/B-confirmed against commit 70fbbeaa as pre-existing, not
caused by the theming batch. Likely cause: a stale `<minimumSize>` in
`resizedialog.ui`.

*Model:* **Haiku 4.5** — pinned suspect; remove/adjust the minimumSize,
verify with an offscreen render of ResizeDialog on light + dark presets.
Escalate to Sonnet 5 if the minimumSize turns out not to be the cause.

- [ ] B1 done

## Opportunistic — do when already in the file, never in bulk

### B2. The 18 kept `processEvents()` sites

From `touch-ups-action-plan.md` T6 (residue of code-analysis §6). All carry
the greppable marker `// FIXME: re-entrancy hazard (processEvents)`. Each
synchronizes with real async state and needs a per-site restructure plus
interactive verification. One site at a time. Highest value first:

- [ ] `src/components/directorymodel.cpp` ×3 (watcher-ordering guards →
      explicit event sequencing in DirectoryManager)
- [ ] `src/gui/customwidgets/thumbnailview.cpp` ×3 (layout waits →
      `QTimer::singleShot(0)` or polish-time geometry passes)
- [ ] The rest per the FIXME grep.

*Model:* **Opus 4.8** — behavioral risk, event-ordering reasoning.

### B3. `performance-enum-size` warnings (~30 sites)

From `code-analysis-action-plan.md` §4 / `touch-ups-action-plan.md` T7.
Deliberately skipped as low-value; address only when a flagged header is
being edited anyway. *Model:* **Haiku 4.5**, folded into whatever session
touches the header.

## Parked — blocked on an external precondition

### B4. `windowswatcher.cpp` old-style connects

From `touch-ups-action-plan.md` T2. Converting SIGNAL/SLOT strings safely
requires a Windows build; parked until Windows CI exists. *Model:* **Haiku
4.5** once buildable.

### B5. `directorypresenter.cpp` connects through `dynamic_cast<QObject*>`

From `touch-ups-action-plan.md` T2 (optional sub-item). Six connects go
through the cast because `IDirectoryView` is a non-QObject interface; a clean
fix changes the interface shape. Only worth it if `IDirectoryView` grows;
signatures are covered by the review cross-check. *Model:* **Sonnet 5** if
ever picked up.

## Deferred by decision — revisit only if the premise changes

### B6. Frameless conversion of the remaining native-framed dialogs

From `dialog-theming-action-plan.md` D5. Decision (2026-07-17): only
FileReplaceDialog was converted; SettingsDialog, ResizeDialog,
ScriptEditorDialog, ShortcutCreatorDialog, PrintDialog and ChangelogWindow
keep native frames (content is already themed). Full conversion =
`FramelessWindowHint` + translucency + `PE_Widget` paint + draggable title
strip per dialog — cosmetic-only and risky on Settings. Revisit only if a
fully bespoke window look is wanted app-wide. *Model:* **Opus 4.8**.

Related recorded decision, not open work: file pickers stay **native**
(D3, 2026-07-17) — native Recent/Places integration outweighs theming.

### B7. Small mixed-platform conditionals

From `os-specific-implementation-cleanup-plan.md` (all primary splits done).
Left as-is on purpose; revisit a file only if its platform branching grows:
`main.cpp` (macOS app object), `videoplayerinitproxy.cpp` (plugin search
paths), `actionmanager.cpp` (Apple defaults data selection),
`shortcutbuilder.cpp` (scan-code vs text branch — tied to InputMap).
Note: `utils/stuff.{h,cpp}` from that inventory no longer exist (retired by
touch-ups T4 into `utils/pathstring.h`). *Model:* **Sonnet 5** per file.

### B8. ContextMenu / GridContextMenu shared base class

From `refactoring-plan.md` item 6. Evaluated and declined (they differ in
262 of 372 lines); revisit only if the two menus start growing parallel
features again. *Model:* **Opus 4.8** (API design across both menus).

---

## Retired source files

`code-analysis-action-plan.md`, `touch-ups-action-plan.md`,
`context-menu-action-plan.md`, `dialog-theming-action-plan.md`,
`os-specific-implementation-cleanup-plan.md`, `popup-polish-action-plan.md`,
`refactoring-plan.md`, `scripts-shortcuts-dropdown-polish-action-plan.md`,
`settings-ui-consistency-action-plan.md`,
`settings-theming-shortcut-binding-action-plan.md` — all completed items are
in git history (this file's creation commit removes them).
