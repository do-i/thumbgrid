# 004 — Stored Data: Readable State + "Stored data" Cleanup Page

Two goals (2026-07-19; implemented same day, all items below checked off):

1. Make the `[State]` section of `thumbgrid.conf` human-readable — today
   `State/duplicateFinder` is a single `@Variant(...)` hex blob because the
   whole `QVariantMap` (two binary QByteArrays + scalars + a folder list) is
   serialized as one value.
2. Give the user granular control over everything the app persists about
   their files: a new **Stored data** settings page listing each data store
   in a table with a per-row delete ('x'), a select-all, and a
   "delete selected on exit" option.

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work · Fable 5 (`claude-fable-5`) cross-cutting
multi-file features.

---

## Data store inventory (ground truth, verified 2026-07-19)

| Store | Location | Contains paths? | Format |
|---|---|---|---|
| Saved paths (session resume) | `thumbgrid.conf` `State/savedPaths` | yes | readable list |
| Bookmarks | `thumbgrid.conf` `State/bookmarks` | yes | readable list |
| Duplicate-finder history | `thumbgrid.conf` `State/duplicateFinder` | yes (`targets` folder list) | **opaque blob** |
| Duplicate hash index | `<cache>/duplicatehashes.dat` (`HashCache`, `duplicatefinder.cpp:44`) | yes (full path → hash map) | binary |
| Thumbnail cache | `<cache>/thumbnails/` (`ThumbnailCache`) | no (id-named PNGs, but derived from user files) | binary |
| Window/panel layout | `State/windowGeometry`, `maximizedWindow`, `lastDisplay`, `placesPanel*` | no | readable |
| Print preferences | `State/print*`, `State/lastPrinter` | no | readable |

The privacy-relevant stores are the first five. Window/print state is
included in the table as optional rows only if it falls out for free; it is
not path-revealing.

---

## S1. Flatten `State/duplicateFinder` into readable keys

Answering "does it need its own file": **no**. The blob is unreadable
because the map is one value, not because INI can't hold it. Flatten the
map written in `duplicatefinderdialog.cpp` (`closeEvent`, ~line 521) into
individual keys:

- `State/duplicateFinderSimilarity` (int), `...Recursive` / `...Rotated` /
  `...Mirrored` (bool), `...Targets` (QStringList) — all plain INI.
- `State/duplicateFinderGeometry`, `...Header` — remain QByteArray
  (`saveGeometry()`/`saveState()` are inherently binary); isolated as two
  clearly-named keys instead of poisoning the whole record.

Replace `Settings::duplicateFinderState()`/`setDuplicateFinderState()`
(settings.cpp:1595) with typed accessors, or keep the map API and
(de)flatten inside Settings — prefer typed accessors, matching the rest of
the file. Add the new keys to `settingGroupFor()`; drop `duplicateFinder`.
Migration: in `runConfigRecoveryMigrations()`, if the old
`State/duplicateFinder` key exists, explode it into the new keys and remove
it (same existing-key-wins rule as the savedState import). Update the
`state-recovery` behavior test or add a sibling scenario.

*Model:* **Sonnet 5** — well-bounded refactor with a mechanical migration;
the pattern (recovery migration + test scenario) already exists to copy.

- [x] S1 done (2026-07-19)

## S2. `StoredDataStore` registry (backend, no UI)

Small value-type registry, e.g. `src/components/storeddata/`:

```
struct StoredDataStore {
    QString id;            // stable: "savedPaths", "bookmarks", "dupeHistory",
                           //         "dupeIndex", "thumbnails"
    QString title;         // translated display name
    QString detail();      // "3 folders", "1,204 files · 48 MB" — computed lazily
    void clear();          // deletes the store
};
QList<StoredDataStore> storedDataStores();
```

Clear semantics per store:

- `savedPaths` / `bookmarks` / dupe-history keys → remove from
  `settingsConf` via Settings methods (add `Settings::clearSavedPaths()`
  etc. — do not hand the registry a raw QSettings).
- `dupeIndex` → delete `<cache>/duplicatehashes.dat`. Note: an open
  DuplicateFinderDialog holds the cache in memory and re-saves on search
  finish; acceptable (file regenerates), but document it and clear the
  in-memory `HashCache` too if the dialog instance is reachable.
- `thumbnails` → remove only `*.png` files inside
  `settings->thumbnailCacheDir()`, never the directory tree blindly; do not
  follow symlinks. Recreate the empty dir after.

Size/count computation must be cheap or deferred (thumbnail dir can hold
tens of thousands of files) — compute on page show, in a worker or with a
capped walk, never on app start.

*Model:* **Sonnet 5** — plain backend code with clear specs; the only
judgment call (safe deletion rules) is spelled out above.

- [x] S2 done (2026-07-19)

## S3. "Stored data" settings page (UI)

New sidebar entry + stacked page in `settingsdialog.cpp` (pages are
assembled around line 370; sidebar is `ui->sideBar2`). Layout:

- Table (QTreeWidget or QTableWidget, consistent with the shortcuts page):
  columns **[✓] | Store | Size/Count | [x]**. The 'x' is a flat icon button
  per row that clears that store immediately after a
  `CustomMessageBox::confirm()`.
- Header or first row provides **Select all** (tri-state checkbox).
- Below the table: checkbox **"Delete selected on exit"** (see S4) and a
  **"Delete selected now"** button acting on the checked rows (one confirm
  for the batch).
- Row checkboxes drive both the exit option and the batch button; the 'x'
  is independent of check state.
- Sizes refresh when the page becomes visible and after any delete.
- Theme: verify against all color presets offscreen (same pixel-diff
  approach as the settings QSS work; the duplicate-finder window theming
  commit 0f22a965 is the reference for what "all presets" means).

*Model:* **Fable 5** — cross-cutting UI feature touching the .ui file,
dialog wiring, theming across presets, and the S2 backend; highest-risk
item of the plan. (Opus 4.8 acceptable fallback.)

- [x] S3 done (2026-07-19)

## S4. Delete-on-exit

- Persist the selection as `State/clearOnExit` (QStringList of store ids —
  readable, survives S1's conventions) plus implicit "enabled if
  non-empty"; the checkbox writes it, page restores it.
- Execution point: `main.cpp` — `saveSettings()` is already registered via
  `atexit` (main.cpp:99). Run the clears in a sibling step **after** Core
  has written its final session state (savedPaths, geometry) and before
  `Settings` teardown, so a cleared store is not immediately re-written by
  shutdown code. Verify the ordering empirically: `Core::close()`
  (core.cpp:384) writes savedPaths — the clear must run after it.
- Thumbnailer/dedupe threads must be stopped before deleting their files —
  reuse the existing shutdown order, do not add new races.

*Model:* **Opus 4.8** — small amount of code but the correctness is all
shutdown-ordering judgment.

- [x] S4 done (2026-07-19)

## S5. Behavior tests

- Registry: each store's `clear()` removes exactly its data (fixture files
  in redirected XDG dirs — see the existing test-home isolation pattern).
- Exit path: seed `State/clearOnExit`, run the exit hook, assert the store
  is gone *and* keys written by shutdown (e.g. windowGeometry) survive.
- S1 migration scenario (covered under S1).

*Model:* **Sonnet 5**, mirroring `test_settings_migration_gates.cpp`
structure. Remember `-DBUILD_TESTING=ON` is not sticky in the cache.

- [x] S5 done (2026-07-19)

---

Suggested order: S1 → S2 → S3 → S4 (S5 woven into each). S1 is independent
and shippable alone; S2–S4 stack.
