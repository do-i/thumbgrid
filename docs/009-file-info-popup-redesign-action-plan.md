# 009 — Redesign "Image info" as a movable "File info" popup

Replace the docked, document-view-only ImageInfoOverlay with a movable,
non-modal, tabbed "File info" window available from the context menu in
both the document and grid views (2026-07-24).

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work.

## Decisions (agreed with maintainer, 2026-07-24)

- **Replace, don't duplicate**: the popup fully replaces ImageInfoOverlay;
  both the `I` shortcut and the menu entries open the popup. The overlay
  and its proxy are deleted.
- **Live-follow**: one non-modal singleton window that refreshes as the
  selection / current image changes (inspector pattern, precedent:
  DuplicateFinderDialog, `src/gui/dialogs/duplicatefinderdialog.cpp:89`).
- **Native frame**: OS titlebar like SettingsDialog/ResizeDialog —
  movable and closable for free, consistent with the recorded
  keep-native-frames decision (007 §B6).
- **Folder counts**: direct children only ("N files, M folders"), no
  recursive scan.
- **Action id stays `toggleImageInfo`**: preset files
  (`src/res/presets/qimgv.json:178`, `src/res/presets/xnviewmp.json:34`)
  and user `shortcuts.json` configs reference it; only user-visible text
  changes to "File info". (The Shortcuts settings page displays raw action
  ids via `shortcutActionLabel`, `src/gui/dialogs/settingsdialog.cpp:1131`
  — changing that is out of scope.)

## Current state (researched)

- Menu entry: `ui->imageInfo` in `src/gui/contextmenu.cpp:97-99`, text
  "Image info", icon `:/res/icons/common/overlay/info16.png`, action
  `toggleImageInfo`. No equivalent in GridContextMenu.
- Action wiring: `toggleImageInfo` (`src/utils/actions.cpp:75`) →
  `MW::toggleImageInfoOverlay` (`src/core.cpp:221`,
  `src/gui/mainwindow.cpp:341`) — early-returns in `MODE_FOLDERVIEW`.
- Overlay: `src/gui/overlays/imageinfooverlay.{h,cpp}` + proxy, rows of
  `EntryInfoItem` (`src/gui/customwidgets/entryinfoitem.*`), fed by
  `DocumentInfo::getExifTags()` (`src/sourcecontainers/documentinfo.h:47`);
  a full Exif/IPTC/XMP dump also exists there (lazy-loaded).
- Grid selection paths: `folderViewPresenter.selectedPaths()` via
  `Core::currentSelection()` (`src/core.cpp:1033`).
- The theme QSS has **no QTabWidget/QTabBar rules** — tab styling is a
  new pattern this plan introduces.

---

## B1. FileInfoDialog shell, live-follow plumbing, General tab

New `src/gui/dialogs/fileinfodialog.{h,cpp}`: non-modal, native-framed
QDialog (class name `FileInfoDialog` for QSS scoping), owned by Core as a
lazily-created singleton (mirror the `duplicateFinderDialog` unique_ptr
pattern, `src/core.cpp` `showDuplicateFinder`). Content: a QTabWidget with
General and EXIF tabs (EXIF filled in B2).

General tab rows (reuse `EntryInfoItem` for visual consistency with the
old overlay): absolute path, size (`QLocale().formattedDataSize`),
permissions (rwx string from `QFile::permissions`), owner
(`QFileInfo::owner`, may be empty on some platforms — hide the row then),
created (`QFileInfo::birthTime`, invalid on some filesystems — show a
dash), last modified. For a folder selection: same rows where applicable
plus "Contains: N files, M folders" from a non-recursive
`QDir::entryList` count; the EXIF tab is disabled.

Live-follow: a `setTarget(const QString &path)` slot; Core retargets it on
document-view image change and on grid selection change (grid: first item
of the selection when multiple are selected). File deleted/renamed under
the popup → clear to a "no selection" state rather than stale rows.
Closing hides the singleton; reopening retargets and raises it.

*Model:* **Opus 4.8** — the dialog itself is routine, but the live-follow
signal topology (document-view current-file changes vs. grid selection
changes vs. deletions, across Core/MW/presenter boundaries) is the
judgment-heavy core of the feature; wrong wiring here shows up as stale
or flickering data.

- [ ] B1 done

## B2. EXIF tab

Populate from `DocumentInfo::getExifTags()` (same source the overlay
used); when the map is empty or the target is not an image, disable the
tab (`QTabWidget::setTabEnabled(false)`) — never an empty enabled tab.
Preserve access to the existing full-metadata mode if the overlay exposed
it (check `imageinfooverlay.cpp` for the toggle before deleting in B3;
if present, a "show all metadata" checkbox in the tab keeps parity).

*Model:* **Sonnet 5** — data plumbing with an existing source and a clear
disabled-state rule.

- [ ] B2 done

## B3. Menu entries, action rewiring, overlay removal

- Rename the document menu entry: `tr("Image info")` → `tr("File info")`
  in `src/gui/contextmenu.cpp:98`; icon stays
  `:/res/icons/common/overlay/info16.png`.
- Add a "File info" `ContextMenuItem` to GridContextMenu (same icon,
  action `toggleImageInfo`), gated in `setSelectionInfo()` with
  `info.total() == 1` (like Rename — the popup shows one file).
- Rewire the action: `MW::toggleImageInfoOverlay` becomes a
  show/raise/hide toggle of the popup and must work in both view modes
  (drop the `MODE_FOLDERVIEW` early-return, `src/gui/mainwindow.cpp:342`).
- Delete `src/gui/overlays/imageinfooverlay.{h,cpp}` (+ its .ui if any)
  and `imageinfooverlayproxy.*`; remove their construction/wiring from MW
  and their CMake entries. `EntryInfoItem` stays (reused by B1).
- Translations: lupdate refresh (`lupdate6 src -ts
  src/res/translations/*.ts` — never with `-no-obsolete`).

*Model:* **Sonnet 5** — coordinated but mechanical once B1 exists;
removal has clear grep-able edges.

- [ ] B3 done

**Follow-up recorded, not done (opt-in):** presets bind `toggleImageInfo`
to `I` in the `document` context only, so in grid view the popup opens
from the menu but not the keyboard. `I` is free in the grid context of the
qimgv preset, but the `adjustFromVersion()` backfill only seeds bindings
for *new* actions — extending an existing action into a new context needs
either a version bump (risk: resurrects deliberately-removed bindings) or
a bespoke migration. Decide deliberately before touching it.
*Model if picked up:* **Opus 4.8** (migration semantics).

## B4. Theming

New `FileInfoDialog` QSS in `style-template.qss` following the
established class-scoped pattern (background, QLabel text, EntryInfoItem
rows if they need it), **plus first-time QTabWidget/QTabBar rules**
(`FileInfoDialog QTabWidget::pane`, `FileInfoDialog QTabBar::tab` with
selected/hover/disabled states) using existing tokens (%widget%,
%background%, %accent%, %text%, %text_secondary_rgba%). Scope the tab
rules to FileInfoDialog so no other dialog silently changes.

Verify offscreen on light + dark presets (the established snapshot
harness approach — throwaway QTest file, QT_QPA_PLATFORM=offscreen,
grab + PNG; never the live X session), including the disabled-EXIF-tab
state.

*Model:* **Sonnet 5** — new QTabBar rules need real QSS care (tab
overlap, selected elevation, disabled contrast), more than mechanical.

- [ ] B4 done

## B5. Behavior tests

New test binaries (one `Core` per process — see behavior-test-isolation
memory):

- Popup opens from the action in document view and in grid view (no
  folder-view early-return regression).
- Grid menu gating: File info enabled for exactly one selected item,
  disabled for none/multi (extend `test_grid_context_menu_gating.cpp` —
  gating assertions only, no second Core there).
- Folder target: General tab shows the direct-children counts; EXIF tab
  disabled.
- Non-EXIF file (e.g. a .txt or headerless png): EXIF tab disabled;
  image with EXIF: tab enabled (fixture: write a small jpeg with exiv2
  tags, or reuse an existing test asset if one has tags).
- Live-follow: change grid selection while open → path row updates.

*Model:* **Sonnet 5** — follows established test patterns; the fixtures
are the only fiddly part.

- [ ] B5 done

## Suggested order

B1 → B2 → B3 (each buildable alone), then B4 and B5 in either order.
Commit per item.
