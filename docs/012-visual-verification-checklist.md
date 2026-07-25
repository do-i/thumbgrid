# 012 — Visual verification checklist

Hands-on checks for the 2026-07-24 changes. Automated tests cover the logic
(47/47 pass); these are the things only eyes on a running app can confirm.

Build and run: `./run.sh`

Anything marked **STOP** means revert that commit and tell me.

---

## 1. Grid painting — highest risk (commit `7cebd7cc`)

Painting is now re-enabled one event-loop turn *after* the grid rebuilds.
If that ever fails, the grid stays blank forever.

- [ ] Open a folder with images. Thumbnails appear. **STOP if the grid is blank.**
- [ ] Go into a subfolder, then back up. Thumbnails appear each time.
- [ ] Resize the window while the grid is open. No flicker or stuck scrollbar.
- [ ] Open a folder with several hundred images. Watch as it loads —
      the scene must not visibly jump when the scrollbar appears.

## 2. File info `I` key (commit `2557e129`)

- [ ] In **grid** view, select a file and press `I` → File info popup opens.
      (This is the new binding.)
- [ ] In **image** view, press `I` → still works as before.
- [ ] Settings → Shortcuts: `I` appears once under grid, once under document.
      **STOP if any shortcut you customised was overwritten.**

## 3. Rename (commits `35fc0ff0`, `4d006aae`)

- [ ] Rename a file normally → works.
- [ ] Rename a folder normally → works.
- [ ] Type `../escaped.txt` as a new name → refused with an error, and the
      rename box **stays open** with your text so you can fix it.
- [ ] Confirm nothing appeared in the parent folder.
- [ ] Rename onto an existing filename → overwrite confirmation still appears.

## 4. Delete a symlink — the data-loss fix (commit `79a71862`)

Set up a safe test first:

```sh
mkdir -p ~/tg-test/precious ~/tg-test/browsed
echo keep > ~/tg-test/precious/treasure.txt
ln -s ~/tg-test/precious ~/tg-test/browsed/looks_like_a_folder
```

- [ ] Open `~/tg-test/browsed` in thumbgrid, delete `looks_like_a_folder`.
- [ ] Run `ls ~/tg-test/precious` → `treasure.txt` **must still be there**.
      **STOP if it is gone** — that is the bug this fixed.
- [ ] Delete an ordinary folder with files in it → still deletes fully.

Clean up: `rm -rf ~/tg-test`

## 5. Copy / move into itself (commit `7ce277e1`)

- [ ] Select a folder → Move to… → pick that same folder, or a folder inside
      it → refused with "Cannot copy or move a folder into itself."
- [ ] Move a folder to an unrelated destination → still works normally.
- [ ] Same two checks via drag-and-drop.

## 6. Video thumbnails (commit `4d4a497e`)

- [ ] Open a folder with videos → thumbnails still generate.
- [ ] Put two videos with the **same filename** in different folders, browse
      both → each shows its own correct thumbnail.
- [ ] A broken/corrupt video file → shows a placeholder, no hang, no crash.

## 7. Startup and window behaviour (commit `25913b80`)

- [ ] Launch the app → window appears normally, no white flash, no delay.
- [ ] Launch with a file argument (`./run.sh some.jpg`) → opens that image.
- [ ] Turn on auto-resize window in settings, open images of different sizes →
      window resizes correctly.
- [ ] Open an image at 1:1 / original fit mode → it is positioned correctly
      immediately, not offset until you interact.

## 8. General smoke test (commit `e0130da2`)

41 enums changed width. The tests pass, but a quick sweep is cheap:

- [ ] Change sorting mode, panel position, scaling filter, theme → all apply.
- [ ] Restart the app → every one of those settings persisted.
- [ ] Open Settings, Duplicate finder, Resize, Print → all dialogs open, and
      the duplicate finder's columns sort correctly.

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
