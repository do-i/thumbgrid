# Touch-ups Action Plan — post-refactor review

Follow-up items found while reviewing the six implementation sessions of
`docs/code-analysis-action-plan.md` (33 commits, `041b242b..f6602a81`, 2026-07-17).
The review verified: clean zero-warning build, 24/24 tests green, app smoke run clean,
string-based signal connects still matching their signatures, SafeSave/index-cache/ownership
changes read line-by-line. Everything below is residue or polish, not regressions —
nothing here blocks using the current `develop`.

Model key as in the main plan: Haiku 4.5 = mechanical sweep, Sonnet 5 = localized refactor,
Opus 4.8 = behavioral risk.

## T1. Finish the commented-out-code sweep — **Haiku 4.5**

Session 1 swept core.cpp, settingsdialog.cpp, viewerwidget.cpp, imageviewerv2.cpp, imagelib.h
but did not touch:

- [x] `src/settings.cpp` (~34 comment-code lines) and `src/components/actionmanager/actionmanager.cpp` (~21)
      — re-checked 2026-07-17: both already clean, remaining `//` lines are all explanatory comments
- [x] Commented-out `qDebug()` lines that session 5 deliberately left (they are dead code, not live
      logging): main.cpp, shortcutbuilder.cpp, imagelib.cpp, loaderrunnable.cpp, scalerrunnable.cpp,
      flowlayout.cpp, documentwidget.cpp, thumbnailview.cpp, scaler.cpp, directorywatcher.cpp,
      windowsworker.cpp (also the two videoplayermpv.cpp copies in plugins/)
- [x] `src/gui/panels/mainpanel/mainpanel.cpp:41` — one commented-out old-style connect

Same rule as before: delete disabled code, keep comments that explain behavior.

## T2. Modernize the remaining string-based connects — **Sonnet 5**

- [x] **`src/gui/viewers/videoplayerinitproxy.cpp:86-90`** — five signal→signal forwards still use
      SIGNAL/SIGNAL. Session 2's skip reason ("requires lambdas") was incorrect: signal-to-signal
      pointer-to-member connects are legal — `connect(player.get(), &VideoPlayer::durationChanged,
      this, &VideoPlayerInitProxy::durationChanged)`. The `VideoPlayer` base-class header is already
      included; verify against the mpv plugin build (`plugins/player_mpv`) and the video smoke path
      (this machine needs `-DPLAYER_MPV_USE_WID=ON`, already set in build cache).
      Done: converted to base-class pointer-to-member; also removed the proxy's dead
      `playbackFinished()` re-declaration (no Q_OBJECT in that class, so it had no moc backing).
- [x] **Six `QTimer::singleShot(..., SLOT(...))` string forms** — `src/core.cpp:71,1493`,
      `src/gui/viewers/imageviewerv2.cpp:836,862`, `src/gui/panels/croppanel/croppanel.cpp:222`,
      `src/gui/overlays/renameoverlay.cpp:33` → pointer-to-member overload.
- [ ] **`src/components/directorypresenter.cpp:69-80`** (optional, larger): six connects go through
      `dynamic_cast<QObject*>(view.get())` because `IDirectoryView` is a non-QObject interface.
      A clean fix changes the interface shape (QObject-derived abstract base, or connect in the
      concrete setView overloads). Only worth it if IDirectoryView grows; otherwise leave —
      the signatures are now covered by the review's cross-check.
- [ ] **`watchers/windows/windowswatcher.cpp`** — old-style connects remain; requires a Windows
      build to convert safely. Flag for whenever Windows CI exists.

## T3. Finish ownership in the thumbnailer chain — **Sonnet 5**

- [x] `ThumbnailerRunnable::createThumbnail` still returns `std::pair<QImage*, QSize>` (raw owning
      pointer), and session 6's ImageLib conversion bridges into it with `.release()`
      (`src/components/thumbnailer/thumbnailerrunnable.cpp:213-218`). Convert the pair to hold
      `std::unique_ptr<QImage>` (or return a small struct), and follow through to where the
      Thumbnail object adopts the pixmap. Same drill as session 6's ImageLib work; the chain is
      threaded but the data flow stays identical.
- [x] While there: `Scaler::finished(QImage*, ...)` queued-signal raw hand-off is the documented
      tolerated exception (see CONTRIBUTING "Ownership") — leave it, but confirm the receiving
      `Scaler::onTaskFinish` adopts into a smart pointer immediately.
      Done: it didn't adopt (manual delete/forward); now adopts into unique_ptr on entry and
      releases only across the queued acceptScalingResult hop; slotForwardScaledResult adopts too.

## T4. Retire `utils/stuff.{h,cpp}` — **Haiku 4.5**

Deferred by session 5 (was optional there):

- [x] Replace `clamp()` callers with `std::clamp` and delete the function.
- [x] Move `toStdString`/`fromStdString` (and the `StdString` typedef) into a purpose-named header
      (e.g. `utils/pathstring.h`); update includes; delete `stuff.{h,cpp}` + CMake entry.
      Done: header-only inline helpers; `#define StdString` became a `using` alias; includes moved
      into the four .cpp files that actually use the helpers (the old header includes were unused).

## T5. Misc small polish — **Haiku 4.5** (fold into any of the above sessions)

- [x] `Cache` uses `QMap` (ordered, O(log n)) where `QHash` suffices — swap container; `keys()`
      callers don't rely on ordering (verify `Cache::keys()`'s one caller).
- [x] `const QStringList Cache::keys() const` — drop the useless top-level `const` on the return type.
      Done both: swapped to QHash; `keys()` turned out to have zero callers, so it was deleted
      outright instead of fixing its return type.
- [x] `logCache` / `logThumbnailer` categories are declared but unused — either add the missing
      qCDebug coverage in cache.cpp/thumbnailer*.cpp (preferred; both subsystems are currently
      silent) or drop the two categories.
      Done: qCDebug in Cache insert/remove/clear/trimTo and Thumbnailer queue/mem-cache-drop;
      the shutdown-timeout qWarning became qCWarning(logThumbnailer).

## T6. The 18 kept processEvents sites — **Opus 4.8** (opportunistic, one site at a time)

Session 6 removed 2 of 20 sites and marked the rest with the greppable
`// FIXME: re-entrancy hazard (processEvents)`. Each kept site synchronizes with real async state
(startup settle, fs-watcher ordering, cross-thread stateBuf, resize/layout propagation, animation
frames) and needs a per-site restructure plus interactive verification — this machine has a real X
display and an XTest-based click-driver approach for visual checks. Do these one at a time, never
in bulk, ideally when already working in the affected file. Highest-value candidates first:

- [ ] `src/components/directorymodel.cpp` ×3 (watcher-ordering guards — replace with explicit
      event sequencing in DirectoryManager rather than draining the queue)
- [ ] `src/gui/customwidgets/thumbnailview.cpp` ×3 (layout waits — `QTimer::singleShot(0)` or
      polish-time geometry passes)
- [ ] The rest per the FIXME grep.

## T7. Open decisions (not code yet)

- [ ] **Naming convention** (§5 residue): members are mixed `mFoo`/`foo`, params mixed `_foo`/`foo`.
      Decide (suggest: `m` prefix for members, no leading underscores) and document in
      CONTRIBUTING.md next to the Ownership section; rename only opportunistically.
- [ ] **Enum-size warnings** (§4 residue, ~30 sites): deliberately skipped as low-value; keep
      skipping unless a header is being edited anyway.

## Suggested batching

| Batch | Scope | Model |
|---|---|---|
| A | T1 + T4 + T5 | Haiku 4.5 |
| B | T2 (videoplayerinitproxy + singleShot forms) + T3 | Sonnet 5 |
| C | T6, one site at a time, as touched | Opus 4.8 |

T7 needs a human decision first; T2's directorypresenter/windowswatcher sub-items stay parked
until IDirectoryView changes or Windows CI exists.
