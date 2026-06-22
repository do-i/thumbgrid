# TODO

Action items for maintaining this hard fork of [easymodo/qimgv](https://github.com/easymodo/qimgv).

## Rebranding

- [x] **Decide on a new app name** (and binary/package name). Until decided, the
      following items are blocked on it.
      Name: `thumbgrid`
- [ ] **New app logo and icon.** Source assets live under
      `qimgv/res/icons/`. Replace the app icon and update
      use new icon -> `qimgv/res/icons/common/logo/thumbgrid/thumbgrid-icon.webp`
      `qimgv/distrib/qimgv.desktop` (`Icon=` / `Name=`) and any packaging refs.
- [ ] **Rename the project** once the name is final:
  - `project(...)` and `HOMEPAGE_URL` in `CMakeLists.txt` (still
    `https://github.com/easymodo/qimgv`).
  - `qimgv/distrib/qimgv.desktop` and the `.desktop` filename.
  - Window title / app id in source (grep `qimgv` across `qimgv/`).
  - `packaging/` and `scripts/` build artifacts.
  - use this new github repo: https://github.com/do-i/thumbgrid

## About page & in-app credits

File: `qimgv/gui/dialogs/settingsdialog.ui` (~lines 5279–5287).

- [ ] Retain original author credit (easymodo, easymodofrf@gmail.com) and the
      Contributors link — required for a GPL fork.
- [ ] Add fork maintainer info: name, GitHub URL, and contact email
      (`doijoji+git@gmail.com`).
- [ ] Point "Report issues / request features" at the fork's issue tracker.

## License

- [ ] Keep the existing `LICENSE` (GPLv3) intact.
- [ ] Add fork copyright line alongside the original (do **not** remove upstream
      copyright). Add per-file copyright headers only to files substantially
      changed/added.
- [ ] Add a short `NOTICE`/attribution paragraph to the README stating this is a
      fork of easymodo/qimgv and crediting the original author.

## Documentation

- [ ] **Fix version mismatch:** README header still says `Current version: 1.0.2`
      while `CMakeLists.txt` is now `2026.6.1` (calVer). Update README and decide
      how version is surfaced going forward.
- [ ] Rewrite README install section — the upstream channels (AUR `qimgv-git`,
      `apt`/`dnf`/`zypper`/`pkg`, Chocolatey, WinGet, releases page) point to
      easymodo and do not apply to this fork yet. Replace with build-from-source
      instructions or fork-specific packages.
- [ ] Document the fork's added features in the README:
  - exiv2 EXIF/IPTC/XMP metadata: view tags, full-metadata mode, preserve on
    save, strip-all for privacy (partially done — verify it's accurate).
  - Folder view: copy/cut/paste of files & folders (symlink-aware), create dir
    (F7), rename (F2), on-disk async folder thumbnail cache.
  - Qt6-only build (Qt5 support dropped).
- [ ] Refresh screenshots if the UI has diverged from the upstream 0.9 shots.
- [ ] Decide whether to keep the upstream "Donate to Ukraine" section as-is
      (recommended to retain) and the war banner at the top.

## Repo hygiene

- [ ] Add a `CONTRIBUTING`/fork note or update `CLAUDE.md` if contributors are
      expected.
- [ ] Sync useful upstream changes periodically; document the merge strategy
      (this is a hard fork, so likely cherry-pick rather than full merge).
- [ ] Audit remaining `easymodo`/`qimgv` string references before a release:
      `grep -rn "easymodo" .` and review each hit.

## Feature ideas / backlog

_(Add as they come up — keep separate from the rebranding checklist above.)_
