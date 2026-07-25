# 012 — Visual verification checklist

Hands-on checks for the 2026-07-24 changes. Automated tests cover the logic
(50/50 pass); these are the things only eyes on a running app can confirm.

> **Rebuilding via `./run.sh b`** reconfigures the CMake cache with
> `BUILD_TESTING=OFF`. The app still rebuilds, but the test binaries stop
> rebuilding and `ctest` then silently re-runs stale ones. Use `./run.sh f`
> (full build) or reconfigure with `-DBUILD_TESTING=ON` before trusting a test
> result.

**Round 2 (2026-07-25).** Your five findings from round 1 are all fixed; the
items below are re-tests, not first passes. Every fix now has a regression test
that was confirmed to fail without it, so the automated suite will catch these
if they come back.

Build and run: `./run.sh`

Anything marked **STOP** means revert that commit and tell me.

---

## 1. Grid painting — highest risk (commit `7cebd7cc`)

Painting is now re-enabled one event-loop turn *after* the grid rebuilds.
If that ever fails, the grid stays blank forever.

- [x] Open a folder with images. Thumbnails appear. **STOP if the grid is blank.**
- [x] Go into a subfolder, then back up. Thumbnails appear each time.
- [x] Resize the window while the grid is open. No flicker or stuck scrollbar.
- [?] Open a folder with several hundred images. Watch as it loads —
      the scene must not visibly jump when the scrollbar appears.

Feedback: I do not have enough volume of image files just yet. I'll test in another machine later.

## 2. File info key — **fixed, re-test**

Round 1: "`I` is taken by zoom in. So, we should not hardcode it. Make this
configurable. `Alt+I` is fine if not already taken."

You were right, and the cause was narrower than it looked: you run the
**leftie** preset, which spends `I` on zoomIn and never bound File info at all.
The `I` in the original checklist came from the qimgv/xnviewmp presets. Two
changes:

- leftie now binds File info to `$Alt+I` in both document and grid.
- The migration no longer names a key. It backfills whatever key *your preset*
  assigns, and skips any context where you already bound the action — so
  nothing is hardcoded and nothing you customised gets taken.

`Alt+I` was free in every preset and in your live config.

- [ ] In **grid** view, select a file and press `Alt+I` → File info popup opens.
- [ ] In **image** view, press `Alt+I` → File info popup opens.
- [ ] `I` still zooms in, `O` still zooms out. **STOP if either changed.**
- [ ] Settings → Shortcuts: File info shows `Alt+I` under grid and document.

## 3. Rename (commits `35fc0ff0`, `4d006aae`)

- [x] Rename a file normally → works.
- [x] Rename a folder normally → works.
- [x] Type `../escaped.txt` as a new name → refused with an error, and the
      rename box **stays open** with your text so you can fix it.
- [x] Confirm nothing appeared in the parent folder.
- [x] Rename onto an existing filename → overwrite confirmation still appears.

## 4. Delete a symlink — the data-loss fix (commit `79a71862`)

Set up a safe test first:

```sh
mkdir -p ~/tg-test/precious ~/tg-test/browsed
echo keep > ~/tg-test/precious/treasure.txt
ln -s ~/tg-test/precious ~/tg-test/browsed/looks_like_a_folder
```

- [x] Open `~/tg-test/browsed` in thumbgrid, delete `looks_like_a_folder`.
- [x] Run `ls ~/tg-test/precious` → `treasure.txt` **must still be there**.
      **STOP if it is gone** — that is the bug this fixed.
- [x] Delete an ordinary folder with files in it → still deletes fully.

Clean up: `rm -rf ~/tg-test`

## 5. Copy / move into itself (commit `7ce277e1`)

Round 1: "I do not see error message."

The guard was firing and the message was being raised — it was going to a hidden
widget. The floating message overlay was parented to the image viewer, which is
hidden in grid view, so **every** floating message was invisible there (the
source even carried a `todo: use additional one for folderview?` note). It is
now parented to the window, so it shows in both views.

- [ ] Select a folder → Move to… → pick that same folder, or a folder inside
      it → refused with "Cannot copy or move a folder into itself."
- [ ] While in **grid** view, confirm other transient messages show up too
      (e.g. copy/paste feedback) — this fix covers all of them, not just this one.
- [x] Move a folder to an unrelated destination → still works normally.
- [x] Same two checks via drag-and-drop.

## 6. Video thumbnails (commit `4d4a497e`)

- [x] Open a folder with videos → thumbnails still generate.
- [x] Put two videos with the **same filename** in different folders, browse
      both → each shows its own correct thumbnail.
- [?] A broken/corrupt video file → shows a placeholder, no hang, no crash.

Feedback: I do not have broken file to test.

## 7. Startup and window behaviour (commit `25913b80`)

- [x] Launch the app → window appears normally, no white flash, no delay.
- [x] Launch with a file argument (`./run.sh some.jpg`) → opens that image.
      Accepted as-is: `./run.sh` shows its menu first and `r` opens the image;
      `thumbgrid img.jpg` works directly, which is what matters.
Round 1: "when footer status bar is displayed, left and right sides have extra
areas."

Auto-resize sized the *window* to the image, but the status footer sits below
the viewer and eats into it — so the viewport came out shorter than the image,
fit-window scaled it down, and the slack showed up as bars either side. It now
sizes the *viewport* to the image and adds the chrome (footer, pinned panel)
back on top.

- [ ] Turn on auto-resize window, open images of different sizes → window
      resizes correctly, with **and** without the status footer shown.
- [ ] Same again with a panel pinned.

Round 1: "most images are correctly displayed. but thumbgrid-icon.webp &
thumbgrid-icon.png only shows top left corner of img."

Right diagnosis — those two are 1254×1254, larger than the viewport on *both*
axes, which is the case nothing centered. `reset()` parks the view on the
image's top-left; at 1:1 the anchored zoom is a no-op (the scale is already
1.0), and neither of the two follow-up steps repositions an image that overflows
both axes. Images overflowing only one axis got centered on the other, which is
why everything else looked fine. The 1:1 path now centers explicitly.

- [ ] Open `thumbgrid-icon.webp` and `thumbgrid-icon.png` at 1:1 → centered
      immediately, not offset until you interact.
- [ ] Open a small image and a wide banner at 1:1 → still positioned as before.

## 8. General smoke test (commit `e0130da2`)

41 enums changed width. The tests pass, but a quick sweep is cheap:

- [x] Change sorting mode, panel position, scaling filter, theme → all apply.
- [x] Restart the app → every one of those settings persisted.
- [-] Open Settings, Duplicate finder, Resize, Print → all dialogs open, and
      the duplicate finder's columns sort correctly.

Round 1: "bug: Right click image, stretch wide -> does not stretch."
Round 2: "stretch wide still does not work even after rebuilt."

Two separate bugs sat on top of each other here.

**The one you were hitting.** "Stretch wide" is the `◀▶` **fit width** button.
It did nothing because "Expand images" is off in your settings: `fitWidth()`
clamped to 1.0 for any image already smaller than the window. Same for fit
window. Three buttons, all inert, on any small image.

"Expand images, up to: Nx" now governs only the **automatic** fit — the one
applied when an image opens or the window resizes. An explicit click on a fit
button is a direct instruction and scales to the window whatever the setting
says. The choice sticks across a resize, and resets when you open the next
image (or persists, if you have "keep fit mode" on).

**The one I fixed in round 1.** The `▲▼` height-stretch button scaled on height
alone, which is *identical* to fit-window's `min(scaleX, scaleY)` for any image
narrower than the window — so it computed the same scale and did nothing. It now
takes the larger ratio and fills the window.

Also fixed while in there: `ViewerWidget::setFitMode()` handled only three of
the four modes, so picking "Fit in window (stretch)" as the **default** fit mode
in Settings was silently dropped on some paths.

- [ ] Open an image **smaller** than the window. Click fit width → it now
      stretches to the window width. Click fit window → it fits. Click the
      height-stretch button → it fills.
- [ ] Open an image **larger** than the window → all three still behave sanely.
- [ ] Resize the window after clicking fit width → it stays fitted, does not
      snap back to 1:1.
- [ ] Open the next image → it opens at 1:1 again (the "Expand images" default),
      confirming the override was per-click and not sticky across images.
- [ ] Settings → default fit mode → "Fit in window (stretch)" → reopen an image
      and confirm it applies.

---

## Still needs doing (not verification — actual work)

Seven `processEvents()` calls are left, all needing someone watching the
screen. Details and fix shapes are in `010-open-items-backlog-action-plan.md`:
fullscreen enter/exit (×2), the folder-view and thumbnail-strip proxies (×4),
and the slide panel animation (×1).

## Needs your credentials — I could not do these

From `011-security-audit-action-plan.md`:

- **XL3** pacman repo signing — needs a signing key + published fingerprint.
- **M3** macOS notarization / Windows Authenticode — needs certificates.
- **L2** workflow permission changes — needs repo settings.
- **L1** Windows dependency pinning — large, needs a Windows runner.
- **XL2** the macOS Finder fix is committed but was only syntax-checked here.
  Test "Open containing directory" on a real Mac before release.
