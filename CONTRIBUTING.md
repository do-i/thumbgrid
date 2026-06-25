# Contributing to thumbgrid

thumbgrid is a **hard fork** of [easymodo/qimgv](https://github.com/easymodo/qimgv).
It is GPLv3 software; the original qimgv copyright and attribution are retained
(see [`NOTICE`](NOTICE) and [`LICENSE`](LICENSE)).

## Building

See the *Installation → Build from source* section in the [README](README.md).
The interactive helper `./run.sh` can install the minimal Qt6 build dependencies
and configure / build / run for you.

## Relationship to upstream

This is a hard fork, not a tracking branch. We do **not** merge upstream
wholesale. Instead, useful upstream changes are **cherry-picked**:

1. Add upstream as a remote (once):
   `git remote add upstream https://github.com/easymodo/qimgv.git`
2. Fetch and review: `git fetch upstream && git log upstream/master`
3. Cherry-pick the commits you want:
   `git cherry-pick <sha>` (resolve conflicts; brand strings differ).

When cherry-picking, watch for the rename from `qimgv` to `thumbgrid`
(binary/target name, app id, desktop/appdata/icon names, install paths under
`/usr/lib/thumbgrid` and `…/share/thumbgrid`). The source directory was also
renamed from `qimgv/` to `src/`, so upstream paths need remapping.

## Coding notes

- Qt6-only. Qt5 support was dropped upstream of this fork.
- Versioning is calendar-based (`YYYY.M.N`); the **git tag is the single source
  of truth**. CMake derives the version from the latest tag and generates
  `src/appversion.cpp` from [`src/appversion.cpp.in`](src/appversion.cpp.in).
  Cut releases with `scripts/release.sh` — don't hand-edit the version. It must
  stay monotonically increasing (drives the changelog / shortcut migration).
- Add an SPDX + copyright header to files you substantially change or add,
  alongside the existing upstream attribution.

## Attribution

Please retain credit to easymodo and the qimgv contributors in any
user-facing About text and in `NOTICE`.
