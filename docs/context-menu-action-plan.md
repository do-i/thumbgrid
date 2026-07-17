# Grid Context Menu Action Plan — context sensitivity + file operations

Goal: make the grid-view right-click menu context sensitive and surface the existing
file operations in it. Today `GridContextMenu` (`src/gui/folderview/gridcontextmenu.{h,cpp}`)
offers only "Convert to..." (JPEG/PNG/WebP), three view toggles, and Settings; the only
gating is `setImageEntriesEnabled(!selection().isEmpty())` in
`FolderGridView::contextMenuEvent` (`src/gui/folderview/foldergridview.cpp:461-466`) —
a selected video, folder, or text file still enables Convert. Meanwhile rename, delete
(trash + permanent), and move flows already exist end-to-end
(`FileOperationsController`, `Core::renameCurrentSelection` / `removePermanent` /
`moveToTrash`, `MW::triggerMoveOverlay`, `RenameOverlay`) but are keyboard-only —
none are in the menu.

Requirements this plan covers:
1. "Convert to..." enabled only when every selected file is a convertible image, or a
   selected folder contains at least one convertible image.
2. New menu items: Rename (single selection only), Delete (trash + permanent),
   Move to... — all operating on the current selection, files and folders alike.
3. Folder selections participate in conversion (a folder expands to its convertible
   images).

Scope guard: grid menu only. The separate viewer-mode `ContextMenu`
(`src/gui/contextmenu.{h,cpp}`) is untouched.

Model key as in prior plans: Haiku 4.5 = mechanical sweep, Sonnet 5 = localized
refactor with clear spec, Opus 4.8 = behavioral risk / design judgment needed.

## C1. Selection introspection: `SelectionInfo` — **Opus 4.8**

The view only knows indices; paths and types live behind the presenter/model. Add a
small value struct and a computation helper where both are visible:

- [ ] `SelectionInfo { int total; int fileCount; int dirCount; bool allConvertibleImages;
      bool anyConvertible; }` (name/shape at implementer's discretion), computed in
      `DirectoryPresenter` next to `selectedPaths()`
      (`src/components/directorypresenter.cpp:231-256`), which already handles the
      parent-dir offset and dir/file index split.
- [ ] "Convertible image" test = extension-based best effort:
      `QImageReader::supportedImageFormats()` + `jfif`, minus formats conversion always
      skips in practice (gif is effectively always ANIMATED; conversion only accepts
      `DocumentType::STATIC`, see `fileoperationscontroller.cpp:263-267`). Do **not**
      load images here — the authoritative STATIC check stays in `convertToFormat`;
      the menu gate is a fast predictor. Put the predicate in a shared spot
      (`DocumentInfo` or a small helper in `utils/`) so C2 reuses it.
- [ ] "Folder contains ≥1 convertible image" = shallow `QDirIterator` over the selected
      dir, extension predicate, stop at first hit, hard cap on entries examined
      (~1000) so opening the menu on a huge directory never stalls. Compute lazily —
      only when the menu is about to show, not on every selection change.
- [ ] Plumbing to the view: `FolderGridView::contextMenuEvent` needs the info but has
      no model access. Preferred shape: presenter injects a provider
      (`std::function<SelectionInfo()>`) into the view via
      `DirectoryPresenter::setView(...)`, mirroring how the remove handler is injected
      into `FileOperationsController` (`core.cpp:95-96`). Avoid new signal round-trips
      through the `FolderView`/`FolderViewProxy`/`MW` chain for a synchronous query.

Why Opus: the dir/file index math, parent-entry offset, and the lazy/capped folder scan
all have edge cases (recursive mode, `..` tile, "show other files" toggle), and the
predictor-vs-authority split is a design decision that must stay coherent with C2.

## C2. Folder-aware conversion — **Opus 4.8**

- [ ] `FileOperationsController::convertToFormat`
      (`src/components/fileoperationscontroller.cpp:237-298`): expand any directory in
      `paths` to its directly contained convertible images (non-recursive, same shared
      predicate as C1) before the existing per-file loop; keep per-file STATIC
      verification as the authority. Report skipped/expanded counts in the existing
      summary message.
- [ ] Risk to verify: the loop uses `model->getImage(path)`; images inside a selected
      subfolder are not entries of the current directory model. Confirm
      `DirectoryModel::getImage` can load an out-of-model path (it routes through
      `Loader`), or load via `DocumentInfo`/`ImageFactory` directly for expanded paths.
      This is the behavioral crux of the item — test it first.
- [ ] Overwrite prompting must keep working for expanded paths (converted copy lands
      next to the source, inside the subfolder).

## C3. New menu items wired to existing actions — **Sonnet 5**

All four operations already have registered actions, Core handlers, and overlays; this
item is menu surface only. `ContextMenuItem::setAction(name)`
(`src/gui/customwidgets/contextmenuitem.cpp:19-23`) auto-fills the shortcut label and
invokes through `actionManager` — use it, do not hand-roll signal chains.

- [ ] Verify exact action names in `src/utils/actions.cpp` (`Actions::init`) before
      wiring; expected: rename (`renameFile`-ish, the one routed to
      `Core::renameCurrentSelection` via `MW::renameRequested`), `moveFile`
      (→ move overlay), `moveToTrash`, `removeFile` (permanent).
- [ ] `GridContextMenu` main page gains, between Convert and the view toggles:
      "Rename..." · "Move to..." · "Move to trash" · separator · "Delete permanently".
      Keep permanent delete visually separated (it is destructive and not undoable);
      it already has its own confirmation dialog downstream — rely on that, no new
      dialog.
- [ ] Rename applies to a **single** selected file *or folder*. Check
      `Core::renameCurrentSelection` (`core.cpp:736`) + `RenameOverlay` against a
      folder selection — `DirectoryModel::renameEntry` (`directorymodel.cpp:139-151`)
      already branches dir/file, but the overlay's path handling needs a look. If
      folder rename turns out broken, fix is in scope here (small), and note it.
- [ ] Delete/move on mixed selections: `FileOperationsController::removePaths` already
      routes dirs→`removeDir`, files→remove handler. Verify `removeDir` semantics on
      non-empty directories (recursive or refuse?) and make the menu behavior match —
      refusing with a message is acceptable, silent no-op is not.

## C4. Context-sensitive enablement — **Sonnet 5**

- [ ] Replace `GridContextMenu::setImageEntriesEnabled(bool)` with
      `setSelectionInfo(const SelectionInfo &)` that enables/disables:
      Convert (per C1 rule), Rename (`total == 1`), Move/Trash/Delete (`total >= 1`).
      View toggles and Settings stay always-on.
- [ ] `FolderGridView::contextMenuEvent` calls the C1 provider and passes the result.
      Selection-empty case: file-op items disabled, menu still opens (today's behavior).
- [ ] Disabled `ContextMenuItem` styling/behavior: confirm `setEnabled(false)` already
      renders and blocks clicks correctly (Convert item is the precedent).

## C5. Shortcut/preset sweep — **Haiku 4.5**

- [ ] The wired actions already have global/document bindings; check each preset in
      `src/res/presets/*.json` for `grid`-section coverage so the menu's shortcut
      labels aren't blank in grid context. Follow preset authoring rules: context
      sections + `$Ctrl` tokens, never expanded modifiers, never flat/global dumps.
      Note: `leftie.json` has uncommitted local edits — don't clobber them blindly.
- [ ] No new action registrations expected; if C3 discovers a missing action name it
      gets registered in `Actions::init` with the current version number (per the
      established pattern) as part of C3, not here.

## C6. Behavior tests — **Sonnet 5**

Follow the `src/tests/behavior/` pattern (`test_other_file_types_toggle.cpp` is the
template: `QTemporaryDir` + `tgtest::writeImage` + `Core::loadPath/showGui` +
`findChild<FolderGridView*>("thumbnailGrid")`; one binary per behavior via
`thumbgrid_add_behavior_test`). These are the first tests exercising file-op menu
paths — keep each narrow:

- [ ] Gating: selections of {image}, {video}, {folder-with-image},
      {folder-without-image}, {image+video}, {two images} → assert menu item
      enabled/disabled states (query the `ContextMenuItem` children directly; no
      synthetic right-click needed).
- [ ] Delete: select file → invoke `removeFile` action path → file gone from disk,
      `grid->itemCount()` drops. Use **permanent** delete in tests (trash would
      pollute the test user's real trash; behavior-test HOME redirection exists but
      trash dirs are still messy — see behavior-test isolation notes).
- [ ] Rename: single file and single folder rename via `Core::renameCurrentSelection`,
      assert on-disk name + model entry.
- [ ] Folder-aware convert (C2): folder containing one png + one txt, convert selection
      to jpg → exactly one new jpg inside the folder.

## C7. Verification (end of implementation)

- [ ] Zero-warning build, full test suite green (24 existing + new).
- [ ] Interactive smoke on the real X display: right-click gating with mixed
      selections, rename/move/trash/delete/convert each once, folder cases included.
      Use the XTest click-driver approach for the click-driven checks; `import` for
      screenshots.
- [ ] Video-format selection check needs the usual `-DPLAYER_MPV_USE_WID=ON` build
      cache (already set on this machine).

## Suggested batching

| Batch | Scope | Model |
|---|---|---|
| 1 | C1 + C2 (introspection + folder-aware convert, shared predicate) | Opus 4.8 |
| 2 | C3 + C4 (menu items + gating plumbing) | Sonnet 5 |
| 3 | C5 (preset sweep) | Haiku 4.5 |
| 4 | C6 + C7 (tests + verification) | Sonnet 5 |

Batch 1 before 2: C4's `setSelectionInfo` consumes C1's struct. Batch 3 anytime after
C3 fixes the action-name list. Batch 4 last, but write the C2 out-of-model
`getImage` probe test early — it derisks the only real unknown in the plan.
