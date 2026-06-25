# TODO

Action items for maintaining this hard fork of [easymodo/qimgv](https://github.com/easymodo/qimgv).

## Rebranding

- [x] **Decide on a new app name** (and binary/package name). Until decided, the
      following items are blocked on it.
      Name: `thumbgrid`
- [x] **New app logo and icon.** Source: `src/res/icons/common/logo/thumbgrid/thumbgrid-icon.webp`.
      Regenerated in-app About icon, hicolor PNGs (`thumbgrid.png`), scalable SVG,
      `thumbgrid.ico` (Windows) and `thumbgrid.icns` (macOS); updated `.desktop`
      `Icon=`/`Name=` and packaging refs.
- [x] **Rename the project** (binary/package/app id → `thumbgrid`):
  - `project(...)` / `HOMEPAGE_URL` in `CMakeLists.txt` → `do-i/thumbgrid`.
  - `.desktop`, `.appdata.xml` renamed and rebranded; org/app id in `main.cpp`.
  - CMake target, install paths (`/usr/lib/thumbgrid`, `…/share/thumbgrid`),
    `packaging/` and `scripts/` artifacts, CI workflows.
  - new repo: https://github.com/do-i/thumbgrid
  - [x] Renamed the internal **source directory** `qimgv/` → `src/` and updated
    all build/script/doc path references (upstream attribution URLs kept).

## About page & in-app credits

File: `src/gui/dialogs/settingsdialog.ui`.

- [x] Retain original author credit (easymodo, easymodofrf@gmail.com) and the
      Contributors link — required for a GPL fork.
- [x] Add fork maintainer info: name, GitHub URL, and contact email
      (`doijoji+git@gmail.com`).
- [x] Point "Report issues / request features" at the fork's issue tracker.

## License

- [x] Keep the existing `LICENSE` (GPLv3) intact.
- [x] Add fork copyright line alongside the original (upstream copyright kept).
      Added `NOTICE` and SPDX/copyright headers to fork-authored files
      (`appversion.cpp`, `run.sh`, `PKGBUILD`, `scripts/build-thumbgrid.sh`).
- [x] Add a short `NOTICE`/attribution paragraph to the README crediting the
      original author.

## Documentation

- [x] **Fix version mismatch:** README now reflects calVer (`2026.6.1`) and
      documents that the version lives in `src/appversion.cpp` / `CMakeLists.txt`.
- [x] Rewrite README install section — replaced upstream channels with
      build-from-source (`run.sh` / CMake) and the Arch `packaging/arch` package.
- [x] Document the fork's added features in the README (exiv2 metadata, folder
      view file management, Qt6-only build).
- [ ] Refresh screenshots if the UI has diverged from the upstream 0.9 shots.
      (Still pending — requires capturing the running UI; README notes the shots
      are inherited from upstream 0.9.)
- [x] Decide on the "Donate to Ukraine" section and war banner: **retained**.

## Repo hygiene

- [x] Added `CONTRIBUTING.md` with a fork note and build pointers.
- [x] Documented the upstream merge strategy (cherry-pick) in `CONTRIBUTING.md`.
- [x] Audited remaining `easymodo`/`qimgv` references. Remaining hits are
      intentional: upstream attribution (README/NOTICE/About/CMake/PKGBUILD),
      historical upstream-issue links in comments, and the upstream
      `qimgv-deps-bin`/`qt-builds` prebuilt build-dependency URLs in the Windows
      script. Brand strings in CLI output and code comments were updated.

## Feature ideas / backlog

_(Add as they come up — keep separate from the rebranding checklist above.)_
