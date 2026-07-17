# Code Analysis — Action Plan

> **Status (2026-07-17): implemented.** All six sessions executed and reviewed; 33 commits,
> `041b242b..f6602a81`. Every item below is checked off with its outcome; partial items and
> follow-up polish live in `docs/touch-ups-action-plan.md`. Build is zero-warning, 24/24 tests
> pass, app smoke-verified.

Analysis of `src/` (127 compiled TUs, ~27.5k lines excluding tests/3rdparty) on 2026-07-16.
Evidence sources: clean rebuild with GCC warnings, `run-clang-tidy` (performance-*, bugprone-*,
modernize-use-override, modernize-loop-convert), and manual review of the largest files.

Each category names the Claude model best suited to implement it in a separate session:

- **Haiku 4.5** (`claude-haiku-4-5-20251001`) — mechanical, pattern-driven sweeps with a build to verify.
- **Sonnet 5** (`claude-sonnet-5`) — localized refactors that need judgment but stay within a file or two.
- **Opus 4.8** (`claude-opus-4-8`) — cross-cutting changes with behavioral risk that need careful
  reasoning about event ordering, ownership, or re-entrancy.

Verify every item with a full rebuild (`make -j$(nproc)` in `build/`) and the behavior tests under
`src/tests/behavior/` (note: test HOME must be redirected — see project memory / test scripts).

---

## 1. Dead code — **Haiku 4.5**

Low-risk deletions; each item is verifiable by grep + rebuild.

- [x] **`src/gui/overlays/mapoverlay.{h,cpp}` (316 lines) is completely unreferenced.** — done (8c2d13fb)
- [x] **`src/utils/sleep.cpp` is dead.** — done (5ff870ff)
- [x] **Strip `QT_VERSION < 5.14` legacy branches** (QDesktopWidget in mainwindow) — done (2c59bb43)
- [x] **Remove the commented-out `QProcess`/`QRegExp` block in video.cpp** incl. `zzzz` debug line and
      unused `<time.h>` — done (140ebd36)
- [x] **Sweep commented-out code blocks** — *partial* (a2cc7fd1): core.cpp, settingsdialog.cpp,
      viewerwidget.cpp, imageviewerv2.cpp, imagelib.h swept; settings.cpp, actionmanager.cpp and
      commented-out qDebug lines remain → touch-ups T1.

## 2. Code duplication — **Sonnet 5**

- [x] **Core clipboard-save "copypasted from ImageStatic" block** → shared `SafeSave::withBackup`
      helper used by both — done (751ba3a5)
- [x] **DirectoryManager setDirectory/scan-loop/insert duplication** → `validateDirectory`,
      `tryMakeFileEntry`, `statFileEntry`/`insertFileEntrySorted` helpers — done (68c46610, 2514508c)
- [x] **FileOperations copyFileTo/moveFileTo shared sections** → `backupExistingDestination`,
      `restoreFileTimestamps` helpers — done (54d58783)
- [x] **Theme id/name/token triple-switch** → single static table — done (845e19ff)
- [x] **Cache lock-take-delete repetition** → `eraseEntry` helper — done (17975538)

## 3. Inefficient algorithms (CPU) — **Sonnet 5**

- [x] **`Cache::trimTo` O(n·m) + keys() allocations** → QSet membership + iterator erase — done (17975538)
- [x] **ActionManager `keys().contains(scanCode)`** → single lookup — done (a631dafa); audit found it
      was the only occurrence repo-wide
- [x] **Full index-cache rebuild per watcher event** → dirty-flag + lazy `ensureFileIndexCache()`/
      `ensureDirIndexCache()`; every emit site hand-traced for index freshness — done (2a00d67d)
- [x] **Double hash lookups (contains + [])** in Cache/DirectoryManager → `find()` once — done
      (17975538, 68c46610)
- [x] **isFile/isDir double stat** → single `fs::status()` — done (68c46610)
- [x] **`std::bind` comparator plumbing** → lambdas, `CompareFunction` machinery removed — done (2514508c)

## 4. Inefficient memory — **Sonnet 5** (bulk sweep parts: **Haiku 4.5**)

- [x] **~264 pass-by-value `QString`/`QList` parameters** → const-ref via clang-tidy sweep + manual
      reconciliation; 17 signal/template signatures deliberately kept by-value (Qt-conventional) —
      done (42c40f0f, 5fa1c19a, f0a36643)
- [x] **Loop-variable copies** → fixed in the same sweep
- [x] **Raw owning `QImage*` returns in the thumbnail/image pipeline** → `std::unique_ptr` returns
      from ImageLib + ThumbnailCache::readThumbnail; Cache owns CacheItems via shared_ptr;
      CacheItem semaphore is a value member — done (516e1fd8, 70bbed1a, fe43e33a). Residue: the
      thumbnailer's internal `pair<QImage*,QSize>` hand-off → touch-ups T3.
- [ ] **~30 `performance-enum-size` warnings** — deliberately skipped (low value); revisit only when
      editing those headers anyway (touch-ups T7).

## 5. Inconsistent C++ / Qt design patterns — **Haiku 4.5** (sweeps), **Opus 4.8** (ownership decision)

- [x] **`override` inconsistency (282 findings)** → clang-tidy modernize-use-override sweep across
      78 files — done (6d634908)
- [x] **Old-style SIGNAL/SLOT connects** — *partial* (c61964eb): croppanel converted (21 connects);
      directorypresenter (non-QObject interface), videoplayerinitproxy (skip reason was wrong),
      windowswatcher (needs Windows build) remain → touch-ups T2.
- [x] **7 `foreach`/`Q_FOREACH` uses** → range-for with `std::as_const` — done (044c2641)
- [x] **`QList<QString>` vs `QStringList`** → QStringList everywhere (59 sites) — done (20f3fe9a)
- [x] **Ownership convention** → decided and documented in CONTRIBUTING.md ("Ownership" section:
      Qt parent ownership for QObjects, unique_ptr/values elsewhere, raw = non-owning observer,
      queued-signal release() as the tolerated exception); applied to Cache/CacheItem, ImageLib,
      ThumbnailCache — done (f6602a81 + session-6 commits)
- [ ] **Member/param naming convention** — open decision → touch-ups T7.

## 6. Anti-pattern usage — **Opus 4.8**

- [x] **21 `qApp->processEvents()` calls** — *partial by design* (6bf27b98, b56e2622): 2 redundant
      force-paints removed (imageviewerv2 scroll callbacks); 18 kept — each synchronizes with real
      async state — and all carry the greppable `// FIXME: re-entrancy hazard (processEvents)`
      marker for one-at-a-time follow-up → touch-ups T6.
- [x] **Ignored `QFile::open()` results in fileoperations.cpp** → `restoreFileTimestamps` returns
      bool, both callers report `FileOpResult::OTHER_ERROR` on failure; build now zero-warning —
      done (35b04b53)
- [x] **137 unconditional `qDebug()` calls** → `QLoggingCategory` (8 categories in
      `src/utils/logging.{h,cpp}`), error-ish messages upgraded to qCWarning, ~95 live calls
      converted — done (10071d95, 96581fd0, 42b9db6b)
- [x] **`Cache::insert(nullptr)` returned true** → false; all 4 callers audited — done (5381b560)
- [x] **`dynamic_cast` in per-event paths** → `qgraphicsitem_cast` for the 6 ThumbnailWidget
      itemAt() casts (drag/drop + mouse handlers); remaining dynamic_casts are QEvent-hierarchy or
      cold-path and were deliberately left — done (a1b5e200)
- [ ] **`utils/stuff.{h,cpp}` grab-bag** — deferred → touch-ups T4.

## 7. Deprecated API usage — **Haiku 4.5**

- [x] `QEnterEvent::pos()` / `QDropEvent::pos()` → `position().toPoint()` (4 sites) — done (156fae76)
- [x] `QImage::mirrored()` → `flipped()` (2 sites) — done (156fae76)
- [x] Qt5-only APIs behind dead version guards — removed with §1.

---

## Suggested execution order

| Session | Scope | Model | Risk | Outcome |
|---|---|---|---|---|
| 1 | §7 deprecated APIs + §1 dead code | Haiku 4.5 | Low | 6 commits (156fae76..a2cc7fd1) |
| 2 | §5 mechanical sweeps (override, connect, foreach, QStringList) | Haiku 4.5 | Low | 4 commits (6d634908..20f3fe9a) |
| 3 | §4 pass-by-value sweep (clang-tidy worklist) | Haiku 4.5 | Low | 3 commits (42c40f0f..f0a36643) |
| 4 | §3 CPU fixes + §2 duplication extraction | Sonnet 5 | Medium | 8 commits (751ba3a5..845e19ff) |
| 5 | §6 QFile::open, Cache::insert, dynamic_cast, logging categories | Sonnet 5 | Medium | 6 commits (35b04b53..42b9db6b) |
| 6 | §6 processEvents removal + §5 ownership convention | Opus 4.8 | High | 6 commits (6bf27b98..f6602a81) |

All sessions verified individually (zero-warning build + 24/24 ctest) and reviewed together
afterwards; the review's findings are `docs/touch-ups-action-plan.md`.
