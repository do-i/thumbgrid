# Thumbnail Scheduling Follow-ups

## Context

Review of the uncommitted fix for the root-folder thumbnail-loading bug
(`docs/root-folder-thumbnail-loading-plan.md`). Verdict: **the original bug is
fixed** — verified with the new committed behavior test and with a temporary
test that reproduced the exact reported flow (load `/`, activate the `/tmp`
tile, assert cells leave the loading-icon state; passed 6 of 6 runs). The full
behavior suite shows no regressions caused by the diff.

Two side-effect risks were identified during verification. Neither is a
correctness regression in the folder-open flow, but both are worth follow-up
work before this pattern hardens.

## Issue 1: Visible-tile prioritization is gone (performance)

`generateThumbnails()` no longer cancels queued jobs, and nothing else prunes
the queue between directory loads. `ThumbnailView::loadVisibleThumbnails()`
fires on every scroll/resize event, so in a large *uncached* folder a fast
scroll enqueues a job for every tile that was ever visible. The pool (4
threads per presenter by default) drains FIFO, so the tiles the user is
currently looking at load only after every scrolled-past tile is generated.
The old per-request `clearTasks()` was what gave visible tiles priority; it
was removed wholesale because it also caused the bug. The same applies to the
thumbnail strip while flipping quickly through a document-view folder (each
skipped image still gets a full decode).

Action items:

- [ ] Reproduce and size the effect: folder with ~2000 uncached images,
      scroll straight to the bottom, measure time until the visible tiles
      render (before vs after the fix).
- [x] Restore prioritization without reintroducing cancellation-of-everything.
      Two options, in order of preference:
      - Track queued runnables in `Thumbnailer` (path/size -> runnable) and,
        on each `generateThumbnails()` batch, `QThreadPool::tryTake()` the
        queued-but-not-started jobs that are not in the incoming batch,
        removing them from `runningTasks` as well. This is now safe precisely
        because tracking is enqueue-time and cleared consistently — a
        cancelled job can be re-requested later.
      - Or keep the queue but start runnables with a monotonically increasing
        `QThreadPool::start(runnable, priority)` so the newest requests run
        first (LIFO). Stale jobs still burn CPU, but visible tiles stay snappy.
- [x] Regression-guard the fixed bug when doing the above: the existing
      "Opening a folder loads its thumbnails" test must keep passing.

## Issue 2: A blocked worker can starve the pool and hang app exit

During verification, one run hung for 300+ s with a worker stuck in a raw
`read()` under `QImageReader::canRead()` inside
`ThumbnailerRunnable::dirPreviewImages()` (probing a transient file in the
shared `/tmp`; not reproducible afterwards). Consequences observed/derived:

- `Thumbnailer::~Thumbnailer()` does `pool->clear(); pool->waitForDone();`
  with no timeout — a stuck worker blocks application exit indefinitely.
- Before this follow-up, `clearTasks()` (called on every `reloadModel()`) wiped
  the tracking entry of the stuck job, so revisiting the same directory
  re-queued it. Each revisit parked another pool thread on the same blocking
  file; after 4 revisits the presenter's thumbnailer was fully starved and no
  thumbnails loaded anywhere. (The old code failed differently: the stale
  tracking entry suppressed re-requests, losing one tile but only one thread.)

Action items:

- [x] In `dirPreviewImages()`, only probe files whose suffix is a supported
      image format (the app already knows the supported extensions) instead of
      running `QImageReader::canRead()` on every file in the child directory.
      This skips logs/sockets-adjacent/oddball files entirely and is also a
      straight speedup for mixed-content folders.
- [x] Cap the destructor wait: `pool->waitForDone(<a few seconds>)` and leak
      the stragglers on timeout rather than hanging exit (workers only touch
      their own runnable state plus the cache, which outlives them — verify
      that before landing).
- [ ] Optional hardening: remember paths whose job never completed within a
      generous deadline and refuse to re-queue them until the directory
      changes, so a pathological file cannot eat one thread per visit.

## Notes (no action required by this change)

- Duplicate in-flight jobs: this follow-up keeps still-running jobs tracked
  across `clearTasks()`, so an immediate re-request for the same path no longer
  queues a second job.
- Pre-existing test failure, unrelated to this diff: "Returning from picture
  view shows the same folder selection" fails identically on a clean `main`
  checkout (times out waiting for `MODE_FOLDERVIEW` after returning). Needs a
  separate investigation; do not attribute it to the thumbnail work.

## Implementation Notes

- Visible-tile priority is restored by tracking queued `ThumbnailerRunnable`
  instances and using `QThreadPool::tryTake()` to remove queued-but-not-started
  jobs that are no longer in the current visible batch.
- `clearTasks()` now removes only queued work; already-running jobs remain
  tracked until completion. This avoids re-queuing the same blocked path on
  repeated directory reloads.
- Verified with `ctest --test-dir build -R "Opening a folder (loads its thumbnails|shows what is inside it)" --output-on-failure`.
