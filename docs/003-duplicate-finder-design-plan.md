# 003 — Image Duplicate Finder: Design Plan (Algorithm + UI)

Status: **implemented** (v1, 2026-07-19) — phases 1–5 landed with tests; §7 remains v2.
Implementation deviations from this plan:
- Deletion uses the static `FileOperations` helpers (trash) with a dialog-local
  count+bytes confirmation instead of `FileOperationsController` — the
  controller is bound to the currently open directory, while finder results
  live anywhere. Same trash backend, same safety.
- The per-row hover ✕ became a row context menu (Open / Move to trash);
  checkbox + Delete-selected covers batch flow. Hover ✕ left as polish.
- The finder dialog is hand-built layout code rather than a .ui file (the
  mode-switching setup zone is dynamic); other dialogs' conventions kept.

## 1. Goals and use cases

Functionality comparable to geeqie's "Find duplicates", with a modern, intuitive UI.

- **UC1 — Single image search.** User picks one image and a scope folder
  (recursive optional). App finds all images in scope whose similarity ≥ a
  user-chosen % threshold.
- **UC2 — Folder-set comparison.** User picks a set of *source* ("truth")
  folders and a set of *target* folders. App looks for every source image in
  the targets and reports matches.
- **UC3 (proposed, nearly free)** — *Self-dedupe*: find duplicates **within**
  one folder set (no separate source). Same engine, one extra mode button;
  this is the most common real-world dedupe task (dupeGuru/czkawka's default
  mode). Recommend including.

Out of scope for v1: video similarity, cross-format visual diffing, cloud
storage. (Video could reuse the same pipeline later by hashing a few
extracted frames.)

## 2. Algorithm

### 2.1 Core: perceptual hashing (pHash / DCT, 64-bit)

Each image is reduced to a 64-bit *perceptual hash*; two images are compared
by Hamming distance between hashes (a single `popcount`, effectively free).

Pipeline per image:

1. Decode → grayscale → `cv::resize` to 32×32 (`INTER_AREA`).
2. `cv::dct` (imgproc), take the top-left 8×8 low-frequency block, drop the
   DC coefficient.
3. Threshold each coefficient against the median → 64-bit hash.
4. Similarity % = `(64 − hamming) / 64 × 100`. The UI threshold slider maps
   linearly to a max Hamming distance (**default 87 % ⇒ distance ≤ 8**).

Why pHash (DCT) rather than aHash/dHash: it is the standard robust choice —
stable under rescaling, re-encoding (JPEG quality changes), mild color/gamma
shifts, small crops/watermarks — while aHash/dHash trade accuracy for speed
we don't need (hashing cost is dominated by image *decode*, not the hash
math).

**Exact-duplicate fast path:** before perceptual comparison, group files by
(size, xxHash-of-content). Byte-identical files are reported as "Exact
(100 %)" without any tolerance ambiguity.

**Rotation/mirror matching (v1, optional, default OFF):** pHash is not
rotation invariant. When enabled, compute the 8 dihedral hash variants
(rotations 90/180/270 + mirrors and their combinations) for each *source*
image only.

*Cost note (why granular per-transform toggles are unnecessary):* the
variants multiply only the popcount *compare* stage, which is effectively
free (§2.2) — they do **not** cause extra image decodes. Enabling all 8
variants versus 2 has no measurable impact on search time; the expensive
work (decoding) is identical. So the UI exposes just two checkboxes —
**"Match rotated copies"** (adds 90/180/270) and **"Match mirrored copies"**
(adds flips; both together = full 8-variant dihedral group) — rather than
per-angle controls that would complicate the UI for no speed benefit.

### 2.2 Complexity — where the time actually goes

The "O(N²)" concern in UC2 is only in the *compare* stage, and comparing two
64-bit hashes is one CPU instruction. 10 000 × 10 000 images = 10⁸ popcounts
≈ well under a second, single-threaded. Brute force is fine for v1; no
BK-tree / multi-index hashing needed (noted as a v2 option if someone runs
100k × 100k libraries).

The real cost is **hashing = decoding every image once**: O(M+N), I/O and
JPEG-decode bound. That drives two design decisions:

- **Persistent hash cache** (§2.3) so repeat searches are near-instant.
- **Parallel hashing** across a thread pool (§2.4).

### 2.3 Persistent hash cache

Small SQLite (or flat binary) cache alongside the existing thumbnail cache
(`components/cache/`): key = canonical path + mtime + file size → 64-bit
hash. Invalidation is automatic (mtime/size mismatch ⇒ rehash). A 100k-image
cache is ~few MB. Recommended for v1 — it turns the second run of any search
from minutes into seconds and is ~a day of work.

### 2.4 Threading model

- **Decision: separate pool** — a `QThreadPool` dedicated to the finder;
  pool size from a new **General settings** field "Duplicate search threads",
  mirroring the existing `thumbnailerThreadCount` pattern in `settings.h`.
- **Shared core budget (combined upper bound):** the two settings draw from
  one budget of `totalCores = QThread::idealThreadCount()`:

  ```
  max(thumbnailerThreads)     = totalCores − duplicateSearchThreads
  max(duplicateSearchThreads) = totalCores − thumbnailerThreads
  ```

  Each spinbox's maximum is recomputed live whenever the other changes
  (e.g. 8 cores, thumbnailer set to 6 ⇒ duplicate finder caps at 2).
  **Defaults: 2 threads each** — so on an 8-core machine both spinboxes
  initially allow up to 6. Guarantees no oversubscription by construction.

  Edge cases:
  - Low-core machines: defaults clamp to keep the invariant —
    `default = max(1, min(2, totalCores / 2))` (2-core machine ⇒ 1 + 1);
    minimum per setting is always 1.
  - Migration: an existing `thumbnailerThreadCount` from an old config that
    would break the invariant is clamped on load (`totalCores − 1` at most,
    leaving ≥1 for the finder).
  - Enforced in `Settings` setters (single source of truth), not only in
    the UI, so programmatic/config-file values can't violate it either.

  *Separate vs shared pool tradeoff:* a single shared pool with task
  priorities cannot deliver the behavior we want, because `QThreadPool`
  priorities only order **queued** tasks — an already-running dedupe hash
  job cannot be preempted, so a burst of thumbnail requests would still wait
  behind up-to-`poolSize` in-flight decodes (each tens to hundreds of ms).
  True preemption would require chunked cooperative yielding — complexity
  with no payoff. Two pools give hard isolation: browsing/thumbnailing speed
  is untouched by a running dedupe, and the shared core budget above rules
  out oversubscription entirely. Settings labels stay specific
  ("Thumbnailer threads", "Duplicate search threads") since the pools are
  independent.
- Producer: directory enumeration (reuse `DirectoryManager`'s supported-
  extension filtering) feeds a work queue; workers decode+hash; results
  funnel to the engine via queued signals.
- Compare stage runs on one worker (cheap), streaming `matchFound` signals so
  results appear live in the table, not only at the end.
- Cancellation: atomic flag checked between files; Cancel returns within one
  in-flight decode.
- Progress signal: `(filesTotal, filesHashed, matchesFound, currentPath)`.

### 2.5 Library candidates surveyed (open-source C/C++)

| Library | What it offers | License | Maintenance | Verdict |
|---|---|---|---|---|
| **OpenCV `img_hash`** (opencv_contrib) | pHash, aHash, blockMeanHash, radialVariance, marrHildreth, colorMoment | Apache-2.0 | Actively maintained (ships with every OpenCV release) | Best external option, **but** it lives in *contrib* — packaging varies by distro/vcpkg and thumbgrid currently only requires `core imgproc` |
| **pHash** (phash.org / aetilius) | The original DCT pHash + audio/video hashing | GPL-3.0 | Sporadic releases, heavy deps (CImg, ffmpeg) | Not recommended — heavier and less maintained than what we can do in-house |
| **libpuzzle** | Grid-feature similarity (crop-tolerant) | BSD | Unmaintained (~2015) | No |
| **Blockhash** (commonsmachinery) | blockhash-c, tiny | MIT | Unmaintained (~2016) | No |
| dupeGuru / czkawka / findimagedupes | Apps, not C/C++ libs (Python/Rust/Perl) | — | — | Prior art for UX & smart-select only |

**Recommendation: implement pHash in-house (~60–100 lines) on top of the
OpenCV `core`+`imgproc` the project already mandatorily links, using the
existing `3rdparty/QtOpenCV` QImage↔cv::Mat bridge.** Zero new dependencies,
no contrib-packaging risk, the algorithm is standard and easily unit-tested.
If we ever want fancier hashes (radial variance for rotation robustness),
adding the `img_hash` module is a drop-in later.

## 3. UI design

### 3.1 Entry points

1. **Document view** — right-click a supported image → **"Find duplicates…"**
   (new item in `gui/contextmenu`). Opens the finder window in UC1 mode,
   pre-seeded with that image; scope defaults to the image's folder,
   "include subfolders" on.
2. **Folder view** — right-click folder/selection → "Find duplicates…" opens
   in UC2/UC3 mode with the selection pre-filled as the source set.
3. Registered action `findDuplicates` in `actionmanager` → appears in the
   shortcuts page and is user-bindable, consistent with the rest of the app.

### 3.2 The finder window

Non-modal window styled like `settingsdialog` (user can keep browsing while a
search runs). Single pane, three vertical zones — **not** a wizard:

```
┌─ Find Duplicates ────────────────────────────────────────────────┐
│ Mode:  (•) One image   ( ) Compare folders   ( ) Within folders  │
│                                                                  │
│ Source   [ /photos/2024/IMG_0123.jpg          ] [Browse…]        │
│ Search in  ┌────────────────────────────────┐  [+ Add] [−]      │
│            │ /backup/photos      ☑ subfolders│                   │
│            │ /nas/dump           ☑ subfolders│                   │
│            └────────────────────────────────┘   (drag&drop OK)   │
│ Similarity  [Exact ── Near-identical ──●── Similar]  [ 87 % ▲▼]  │
│ Options    ☐ Match rotated copies   ☐ Match mirrored copies      │
│                                                                  │
│ [▶ Start]   ████████████░░░░  8 102 / 12 340 hashed · 37 matches │
├─ Results ────────────────────────────────────────────────────────┤
│ Filter: [____________]  Min %: [85 ▲▼]                           │
│ ┌──┬─────┬──────┬───────────┬─────────┬──────────┬────────────┐  │
│ │☑ │thumb│  %   │  W × H    │  size   │ name     │ path     ✕ │  │
│ ├──┼─────┼──────┼───────────┼─────────┼──────────┼────────────┤  │
│ │… │     │ 100  │ 4000×3000 │ 3.2 MB  │ a.jpg    │ /backup/… │  │
│ └──┴─────┴──────┴───────────┴─────────┴──────────┴────────────┘  │
│ ┌ Preview: selected row ────────────────────────────────────┐    │
│ │  [ source thumb ]   vs   [ match thumb ]  + metadata diff │    │
│ └───────────────────────────────────────────────────────────┘    │
│ Smart select: [Keep largest ▾]  3 sel  [Move to… ▾] [Delete…]    │
└──────────────────────────────────────────────────────────────────┘
```

- **Setup zone** collapses (animated splitter/toggle) once a search starts,
  giving the results table the space.
- **Start** morphs into **Cancel** while running. Live counters: target file
  total, hashed count, match count, elapsed; results stream into the table
  as found.
- Similarity control is a slider with *labeled zones* (Exact ≈100 %,
  Near-identical ≥95 %, Similar ≥85 %) plus an exact spinbox — raw
  percentages alone are opaque to users.

### 3.3 Results table

`QTableView` over `DuplicateResultsModel` + `QSortFilterProxyModel`.

- Columns: ☑ checkbox · thumbnail (via existing thumbnailer) · similarity % ·
  dimensions (W×H) · file size · filename · absolute path · hover **✕**
  delete button (last column).
- Sortable on every column via header click; filter row above the table
  (substring on name/path + min-similarity spinbox).
- **UC2/UC3 grouping:** matches come in *groups* (a source image and its
  duplicates). A flat table loses that structure, so the model is a
  two-level tree (`QTreeView`): parent row = the source/reference image,
  children = its matches. In UC1 there is one implicit group, so it renders
  as the flat table the user described. One model, both shapes.
- Row interactions: double-click → open in main view; context menu → Open,
  Reveal in folder view, Compare side-by-side; Space toggles checkbox.
- **Deletion** goes through the existing `FileOperationsController` →
  **trash by default**, permanent only with the app's usual modifier/confirm
  conventions. Confirmation dialog lists count + total bytes reclaimed.
- **Move to folder (v1, the safe alternative to delete):** a second action
  button next to Delete. Destination is picked **in the dedupe window**
  (folder picker on first use, then remembered as last-used — no settings
  round-trip needed; a default can live in settings later if wanted).

  *Collision handling — decision:* each move operation creates a
  timestamped session subfolder, `<dest>/dedupe-20260719-142233/`, and
  files are placed inside it **preserving their relative path from the
  common root of the searched folders** (e.g. searching `/nas/dump` →
  `dedupe-…/photos/2024/img.jpg`). Rationale versus the alternatives:
  - *Timestamp subdir + relative paths (chosen):* zero collisions across
    sessions by construction; collisions within one session are impossible
    (source paths were unique); filenames stay human-readable; the origin
    of every moved file is reconstructible from its subpath — which makes
    the whole operation *reversible by hand*. That reversibility is the
    point of move-as-safety.
  - *Hash-as-filename in one flat dir:* collision-proof but destroys
    filenames and origin info — a moved file can't be identified or
    restored without a manifest. Rejected.
  - *Suffix `-1`, `-2` on collision in a flat dest:* loses origin info and
    silently renames; hard to audit. Rejected.

  Additionally the engine writes a small `manifest.txt` (original path →
  new path per line) into the session folder, so even a partially-moved or
  hand-edited session remains auditable/restorable.

### 3.4 Extra UX (recommended, this is what beats geeqie)

1. **Side-by-side preview pane** (in v1): selecting a row shows source vs
   match with a metadata diff (dimensions, file size, mtime highlighted
   green/red for better/worse). Deleting confidently requires *seeing* both;
   this single feature is most of the intuitiveness win.
2. **Smart select** menu: "Keep largest resolution", "Keep biggest file",
   "Keep newest / oldest", "Keep the one in source folders" — checks all
   *other* rows per group in one click (dupeGuru's killer feature).
3. Drag & drop folders from the OS or from thumbgrid's folder view into the
   source/target lists.
4. Persistent window state: last folders, threshold, column widths.
5. v2 ideas: A/B flicker compare (overlay two images, toggle on hover),
   export results to CSV, "move to folder" as an alternative to delete.

## 4. Architecture / file layout

```
src/components/duplicatefinder/
    duplicatefinder.h/.cpp     — engine: QObject, owns pool, emits progress/matchFound/finished
    imagehasher.h/.cpp         — pHash (cv::dct) + xxHash exact path; pure functions, unit-testable
    hashcache.h/.cpp           — persistent path+mtime+size → hash store
    duplicatematch.h           — result structs (group id, paths, %, dims, sizes)
src/gui/dialogs/
    duplicatefinderdialog.h/.cpp/.ui
    duplicateresultsmodel.h/.cpp  (+ proxy)
settings.h/.cpp                — duplicateSearchThreadCount(), defaultSimilarityThreshold();
                                 combined-core-budget clamping for both thread settings (§2.4)
gui/contextmenu, actionmanager — new "Find duplicates…" entries
```

Signals engine→UI only via queued connections; the dialog never touches
worker threads directly. Engine is UI-free → fully coverable by the existing
test setup (see [behavior-test-isolation] note: redirect HOME).

## 5. Implementation phases (with model assignments)

| Phase | Work | Est. | Suggested model + rationale |
|---|---|---|---|
| 1 | `imagehasher` (pHash + exact path) + unit tests: re-encoded/rescaled copies match, distinct images don't, threshold mapping | S | **Claude Fable 5** — algorithmic core; correctness and numeric subtleties (DCT block, median, INTER_AREA) matter most |
| 2 | `duplicatefinder` engine: enumeration via DirectoryManager, thread pool, cancellation, progress signals; settings plumbing | M | **Claude Fable 5** — concurrency + lifetime issues are the risk here |
| 3 | Dialog UI (.ui, mode switching, folder lists, slider), results tree model, thumbnail delegate, sort/filter | M–L | **Claude Sonnet 5** for the .ui/model boilerplate; Fable 5 pass for UX polish and delegate painting |
| 4 | Delete integration (FileOperationsController), **move-to-folder with session subdir + manifest**, smart select, preview pane | M | **Claude Sonnet 5** — mostly wiring to existing components |
| 5 | Hash cache; **rotation/mirror variants (2 checkboxes, default off)**; persistence of window state | S–M | **Claude Sonnet 5** — well-scoped, patterned code |
| 6 | v2: screenshot→video source finder (§7), CSV export, A/B compare, BK-tree for huge sets | — | defer |

## 6. Decisions log (questions resolved 2026-07-19)

1. **Modes:** UC3 "within folders" self-dedupe **approved** — three modes in v1.
2. **Threads:** **separate pool + separate setting** ("Duplicate search
   threads"). A shared pool with priorities was considered and rejected:
   `QThreadPool` priorities only order *queued* tasks — running dedupe
   decodes can't be preempted by thumbnail requests, so thumbnailing would
   still stall. Full tradeoff analysis in §2.4.
   **Addendum (2026-07-19):** the two settings share a combined budget
   capped at total cores — each spinbox's max = cores − the other's current
   value (8 cores, thumbnailer at 6 ⇒ finder max 2). Defaults 2 + 2, so on
   8 cores each initially allows up to 6. Enforced in Settings setters, with
   low-core clamping and config migration. Details in §2.4.
3. **Delete / move:** trash-by-default confirmed; **move-to-folder added to
   v1** as the safe alternative. Destination picked in the dedupe window
   (last-used remembered). Collisions solved by timestamped session subdir +
   relative-path preservation + manifest; alternatives (hash filenames, `-1`
   suffixes) rejected as non-auditable. Full design in §3.3.
4. **Default threshold: 87 %** (Hamming ≤ 8).
5. **Rotation/mirror:** in v1 as **option, default off**. Exposed as two
   checkboxes (rotated / mirrored) rather than per-transform toggles —
   variants only multiply the free compare stage, not decodes, so granular
   control would add UI complexity with no speed benefit (see §2.1 cost note).
6. **Videos:** out of v1. The "which video is this screenshot from?" idea is
   very feasible and is specced as the headline v2 feature — see §7.
7. **Grouped tree view for UC2/UC3: approved.**

## 7. v2 concept — "Find the source video of a screenshot"

Answering the question from review: yes, this exact feature is achievable
with the same architecture, and it is how commercial "video fingerprinting"
works at small scale.

**How it works:**

1. **Index videos once.** For every video in scope, decode frames at a
   sampling interval (e.g. every 0.5 s, or every keyframe — mpv/ffmpeg,
   already a project dependency, does the decoding) and compute the same
   64-bit pHash per sampled frame. Store `(video, timestamp) → hash` in the
   hash cache. A 2-hour video at 0.5 s sampling is ~14 400 hashes ≈ 115 KB —
   a whole library indexes to a few MB.
2. **Query.** pHash the screenshot, compare against all frame hashes
   (popcount — millions of compares per second, §2.2). Best matches give not
   just *which video* but *which timestamp*, so the result row can show
   "movie.mkv @ 00:41:37" and even deep-link playback there.
3. **Sampling vs "random frame":** a screenshot is rarely exactly on a
   sampled frame, but adjacent frames within ~0.5 s are visually near-
   identical in most content, so the neighboring sampled hash still lands
   within threshold. Fast cuts/action can be covered by denser sampling or
   scene-change-triggered sampling.

**Caveats to design around (why it's v2):**

- Screenshots often include player chrome, subtitles, or letterbox bars.
  Black-bar borders shift the DCT significantly → need an auto-crop
  (trim uniform borders) preprocessing step on the query image.
- Indexing is decode-heavy (minutes for a large library, first run only) →
  needs its own progress UI and an incremental/cancellable indexer.
- Threshold behavior differs from stills (motion blur, encoder differences)
  → likely a slightly looser default for this mode.

None of this changes v1 architecture: it reuses `imagehasher`, the hash
cache (schema already keyed by path — extend key with a frame timestamp),
the thread pool, and the results UI. That reuse is deliberate in the v1
component layout.
