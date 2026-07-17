# Code Analysis — Action Plan

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

- [ ] **`src/gui/overlays/mapoverlay.{h,cpp}` (316 lines) is completely unreferenced.**
      No include, no instantiation anywhere outside its own files, yet it is compiled and linked.
      Remove the files and their `src/CMakeLists.txt` entry.
- [ ] **`src/utils/sleep.cpp` is dead.** It defines a `Sleeper` class referenced by nothing
      (no header, no includes of it anywhere). Remove file + CMake entry.
- [ ] **Strip `QT_VERSION < 5.14` legacy branches.** The project requires Qt6
      (`find_package(Qt6 REQUIRED ...)`), so these can never compile:
      - `src/gui/mainwindow.h:13-15` (`#include <QDesktopWidget>`), `:120-122` (`QDesktopWidget desktopWidget` member)
      - `src/gui/mainwindow.cpp:427-432`, `:441-445`, `:657-662` (guarded `desktopWidget.screenNumber()` branches)
- [ ] **Remove the commented-out `QProcess`/`QRegExp` block in `src/sourcecontainers/video.cpp:16-44`**
      (includes a leftover `qDebug() << "zzzz"` line). `QRegExp` no longer exists in Qt6; the block
      can never be revived as-is. Also drop the now-unused `#include <time.h>` if nothing else uses it.
- [ ] **Sweep commented-out code blocks** (comment lines containing statements). Highest density:
      `src/core.cpp` (~43), `src/gui/dialogs/settingsdialog.cpp` (~38), `src/settings.cpp` (~34),
      `src/gui/viewers/viewerwidget.cpp` (~22), `src/gui/viewers/imageviewerv2.cpp` (~22),
      `src/components/actionmanager/actionmanager.cpp` (~21). Delete obviously dead ones
      (e.g. `core.cpp:284-291` welcome-screen stubs, `core.cpp:730-733`, `imagelib.h:31`);
      keep comments that explain behavior.

## 2. Code duplication — **Sonnet 5**

Requires extracting shared helpers and re-testing the touched paths.

- [ ] **`Core::saveImageToDisk` clipboard-save block, `src/core.cpp:648-700`** — self-labeled
      `// ------- temporarily copypasted from ImageStatic (needs refactoring)`. Extract the
      backup-tmpfile-save-restore sequence into a shared helper (e.g. in `ImageLib` or a new
      `utils/safesave`) used by both `Core` and `ImageStatic::save`.
- [ ] **`DirectoryManager::setDirectory` vs `setDirectoryRecursive`, `src/components/directorymanager/directorymanager.cpp:106-152`**
      — identical path-validation prologue; `addEntriesFromDirectory` vs `addEntriesFromDirectoryRecursive`
      (`:329-415`) duplicate the FSEntry-construction body; `forceInsertFileEntry` (`:450`) and
      `renameFileEntry` (`:493`) duplicate the stat + `insert_sorted` sequence. Extract
      `validateDirectory(path)`, `makeFileEntry(entry)`, and a single insert helper.
- [ ] **`FileOperations::copyFileTo` vs `moveFileTo`, `src/utils/fileoperations.cpp`** — large shared
      sections (and both contain the ignored `QFile::open` result at `:138` / `:211`, see §6).
- [ ] **Theme id/name/token triple-switch, `src/settings.cpp:36-73`** — `themeName()`, `themeToken()`,
      `themeTidFromToken()` re-encode the same mapping three times. Replace with one static table
      (tid → {name, token}) and derive all three lookups from it.
- [ ] **`Cache::remove` / `clear` / `trimTo`, `src/components/cache/cache.cpp`** — repeated
      lock-take-delete sequence; extract one erase helper (combine with the §3 `trimTo` fix).

## 3. Inefficient algorithms (CPU) — **Sonnet 5**

- [ ] **`Cache::trimTo(QStringList)`, `src/components/cache/cache.cpp:64-71`** — O(n·m):
      for every cached item it linear-scans the QStringList, and `items.keys()` allocates a list
      first. Convert the argument to a `QSet<QString>` (O(1) membership) and iterate the hash
      directly with iterators. Same `for(auto path : items.keys())` pattern in `clear()` (`:32`).
- [ ] **`ActionManager::processWheelEvent`/key handling, `src/components/actionmanager/actionmanager.cpp:233`**
      — `inputMap->keys().contains(scanCode)` allocates the whole key list and linear-scans it on
      every key press. Use `inputMap->contains(scanCode)` (and audit the other `.keys().contains()` uses).
- [ ] **Full index-cache rebuild per watcher event, `src/components/directorymanager/directorymanager.cpp:169-181, 464, 477, 510, 526, 542`**
      — every single insert/remove/rename triggers `rebuildFileIndexCache()` which re-inserts all N
      entries; a burst of watcher events is O(N²). Update the hash incrementally (shift indices ≥
      insertion point) or coalesce rebuilds with a dirty flag drained once per event batch.
- [ ] **Double hash lookups throughout `Cache` and `DirectoryManager`** — `contains(path)` followed
      by `items[path]`/`.value(path)` does the lookup twice; use `find()`/iterator once per call
      (`cache.cpp` nearly every method, `directorymanager.cpp` entry management).
- [ ] **Double `stat()` in `DirectoryManager::isFile`/`isDir`, `directorymanager.cpp:285-299`** —
      `fs::exists()` + `fs::is_regular_file()` each hit the filesystem; a single
      `fs::status()`/`std::error_code` overload answers both.
- [ ] **`std::bind(compareFunction(), this, _1, _2)` in sorting, `directorymanager.cpp:417-425, 463, 525, 541, 571`**
      — member-function-pointer + bind blocks inlining in `std::sort`'s hot comparator; replace the
      comparator plumbing with lambdas (also removes the `CompareFunction` typedef machinery).

## 4. Inefficient memory — **Sonnet 5** (bulk sweep parts: **Haiku 4.5**)

- [ ] **~264 pass-by-value `QString`/`QList<QString>` parameters** flagged by clang-tidy
      (`performance-unnecessary-value-param`); heaviest offenders in `components/` headers
      (`fileoperationscontroller.h`, `directorymodel.h`, `directorymanager.h`, `cache.h`) plus
      `std::shared_ptr<const QImage>` copies in `utils/imagelib.h`. Change to `const &`.
      Mechanical → Haiku 4.5 with the clang-tidy log as the worklist
      (`scratchpad/clang-tidy.txt` from this session, or re-run the command in the header above).
- [ ] **14 loop variables copied per iteration** (`performance-for-range-copy`), e.g.
      `for(auto path : items.keys())` — combine with the §3 keys() fixes.
- [ ] **Raw owning `QImage*`/`new QImage` returns in the thumbnail/image pipeline** —
      `ThumbnailCache::readThumbnail`/`saveThumbnail` (`src/components/cache/thumbnailcache.cpp`),
      `ImageLib::*Raw` and `scaled*` (`src/utils/imagelib.{h,cpp}`), `CacheItem*` raw storage in
      `Cache`. QImage is implicitly shared — returning by value is cheap and leak-proof; prefer
      value returns (or `std::unique_ptr` where null-signaling is needed). This is judgment work → Sonnet 5.
- [ ] **~30 `performance-enum-size` warnings** (enums defaulting to 4-byte base) — genuinely low
      value; do only if touching those headers anyway. Lowest priority in this plan.

## 5. Inconsistent C++ / Qt design patterns — **Haiku 4.5** (sweeps), **Opus 4.8** (ownership decision)

- [ ] **`override` usage is inconsistent: 282 unique clang-tidy findings** — 192 virtual overrides
      missing `override`, 55 `virtual ... override` redundancies, 35 `virtual` where `override`
      belongs. One mechanical sweep with `clang-tidy -fix -checks=modernize-use-override`
      then hand-verify the diff. → Haiku 4.5
- [ ] **30 old-style `SIGNAL()/SLOT()` connects vs 454 new-style** in:
      `components/directorypresenter.cpp`, `gui/viewers/videoplayerinitproxy.cpp`,
      `gui/panels/mainpanel/mainpanel.cpp`, `gui/panels/croppanel/croppanel.cpp`,
      `components/directorymanager/watchers/windows/windowswatcher.cpp` (Windows-only — cannot be
      compile-verified on this machine; convert but flag for a Windows build). New-style gives
      compile-time checking. → Haiku 4.5
- [ ] **7 `foreach`/`Q_FOREACH` macro uses** — replace with range-for (mind detach semantics:
      add `std::as_const` where the container is non-const). → Haiku 4.5
- [ ] **`QList<QString>` (59 uses) vs `QStringList` (135 uses)** — pick `QStringList` and sweep. → Haiku 4.5
- [ ] **Ownership convention is mixed: 230 raw `new`, 76 manual `delete`, 259 smart-pointer uses.**
      Most raw `new` is fine (Qt parent-owned widgets), but non-QObject owning raw pointers
      (`Cache`/`CacheItem`, `ImageLib` returns, `Thumbnail` plumbing) need an explicit rule:
      Qt parent ownership for QObjects, `std::unique_ptr`/values elsewhere. Deciding and applying
      the boundary without breaking the loader/scaler threading → **Opus 4.8**.
- [ ] **Member/param naming is mixed** (`mSortingMode` vs `watcher`, `_path`/`_info` leading
      underscores). Agree a convention in CONTRIBUTING.md; only rename opportunistically —
      a whole-repo rename is churn with little payoff.

## 6. Anti-pattern usage — **Opus 4.8**

These have real behavioral risk; they need reasoning about event-loop re-entrancy and error paths.

- [ ] **21 `qApp->processEvents()` calls** (`core.cpp:66`, `components/directorymodel.cpp:141,178,192`,
      `gui/mainwindow.cpp:256,670,680`, `gui/viewers/imageviewerv2.cpp:706`, `main.cpp:155`, ...) —
      re-entrancy hazards: a nested event loop mid-function can delete objects under you or re-enter
      the same slot. `mainwindow.cpp:256` even carries the comment "not needed anymore with patched qt?".
      Replace each with signals/queued invocation, or delete where it was a repaint hack; test each
      site individually (this is where regressions will come from). → **Opus 4.8**
- [ ] **Ignored `QFile::open()` results, `src/utils/fileoperations.cpp:138, 211`** (GCC
      `-Wunused-result`): `dstF.open(QIODevice::ReadWrite)` return unchecked before writing — a
      failed open silently corrupts the copy/move result reporting. Handle the failure and set
      `FileOpResult` accordingly. Small but a genuine bug fix; include a test. → Sonnet 5
- [ ] **137 unconditional `qDebug()` calls**, many in hot paths (`directorymanager.cpp` logs every
      watcher insert/remove/rename with `"fileIns"`/`"fileRem"`/`"zzzz"`-style tags). Adopt
      `QLoggingCategory` (e.g. `thumbgrid.dirmanager`, `thumbgrid.cache`) so logging is filterable
      and free when disabled. → Haiku 4.5 once the category scheme is chosen
- [ ] **`Cache::insert(nullptr)` returns `true`** with the comment `// TODO: what state returns here ??`
      (`cache.cpp:10-21`) — should return false; audit callers. → Sonnet 5
- [ ] **27 `dynamic_cast`s**, several in per-event paths (`foldergridview.cpp:57,72` on drag/drop,
      `thumbnailview.cpp` itemAt casts). For QGraphicsItem hierarchies prefer `qgraphicsitem_cast`
      with `type()`; for QObject use `qobject_cast`. → Sonnet 5
- [ ] **`utils/stuff.{h,cpp}` grab-bag** — `clamp()` reimplements `std::clamp`; fold `toStdString`/
      `fromStdString` into a named `utils/pathstring` (or similar) and delete `stuff.*`. → Haiku 4.5

## 7. Deprecated API usage — **Haiku 4.5**

All confirmed by the clean-build warning log; each is a one-line change, verify with rebuild
(the build must end with zero `-Wdeprecated-declarations` warnings).

- [ ] `QEnterEvent::pos()` → `position().toPoint()` — `src/gui/viewers/viewerwidget.cpp:596`
- [ ] `QDropEvent::pos()` → `position().toPoint()` — `src/gui/folderview/foldergridview.cpp:57, 72`
      and `src/gui/folderview/treeviewcustom.cpp:20`
- [ ] `QImage::mirrored(bool,bool)` → `flipped(Qt::Orientations)` — `src/utils/imagelib.cpp:58, 69`
- [ ] Qt5-only APIs behind dead version guards (`QDesktopWidget`, `QRegExp`) — removed by §1.

---

## Suggested execution order

| Session | Scope | Model | Risk |
|---|---|---|---|
| 1 | §7 deprecated APIs + §1 dead code | Haiku 4.5 | Low |
| 2 | §5 mechanical sweeps (override, connect, foreach, QStringList) | Haiku 4.5 | Low |
| 3 | §4 pass-by-value sweep (clang-tidy worklist) | Haiku 4.5 | Low |
| 4 | §3 CPU fixes + §2 duplication extraction | Sonnet 5 | Medium |
| 5 | §6 QFile::open, Cache::insert, dynamic_cast, logging categories | Sonnet 5 | Medium |
| 6 | §6 processEvents removal + §5 ownership convention | Opus 4.8 | High |

Sessions 1–3 are independent of each other; run 4–6 after them to avoid rebase churn.
Re-run the clang-tidy command (header of this file) after sessions 1–3 to regenerate the worklist
for session 4+.
