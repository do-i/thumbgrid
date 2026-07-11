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

## Releasing

The git tag is the single source of truth for the version.

**Branching model:**

- `develop` is the integration branch. Feature branches merge into it with
  `--ff-only` (rebase onto `develop` first), so `develop` stays a linear
  history sitting directly on top of `main`.
- `main` never receives commits directly — a GitHub ruleset blocks
  force-pushes and deletion, and `release.sh` itself refuses to release if
  `main` has diverged (e.g. from an accidental direct push). `main` only
  advances by a fast-forward of `develop`, done by the release script.
- `release-<version>` branches are created manually, ad hoc, only to patch an
  already-shipped version with a critical fix. `release.sh` never creates,
  reads, or touches these; fixes there are cherry-picked into `develop`/`main`
  by hand as needed.

**Cutting a release**, from anywhere with a clean working tree:

```
./scripts/release.sh             # interactive menu
./scripts/release.sh cut         # cut a release non-interactively
./scripts/release.sh cut 2026.7.2  # cut an explicit version instead of auto-CalVer
./scripts/release.sh --dry-run cut # preview the steps, change nothing
```

"Cut release" fetches `origin`, verifies `origin/main` is an ancestor of
`origin/develop` (aborting with the offending commits listed if not), checks
via `gh` that `develop`'s tip has a passing `tests.yml` run, then fast-forwards
`main` to that commit, tags it, and pushes `main` + the tag together
atomically. Pushing the tag triggers
[`arch-package.yml`](.github/workflows/arch-package.yml), which builds the
Arch package and publishes a GitHub Release with **auto-generated release
notes** (from merged PRs / commits since the previous tag). The script only
tags — there is no version constant in the tree to bump.

**Other menu items:**

```
./scripts/release.sh status  # show develop/main state, make no changes
```

Flags: `--dry-run` (preview only), `--skip-ci-check` (bypass the `gh` CI-status
gate), `--yes` (skip the confirmation prompt).

A few notes:

- Versions follow CalVer `YYYY.M.N`: major = year, minor = month, micro =
  sequential release within that month (from 1). The micro must stay
  monotonically increasing — it drives the in-app update/changelog logic.
- The in-app version is derived from the latest git tag at **build time**
  ([`cmake/GenerateAppVersion.cmake`](cmake/GenerateAppVersion.cmake)), so dev
  builds show the tag plus a `-<count>-g<hash>` suffix and refresh on every
  commit without re-running `cmake`. A build requires git history and a tag.
- To shape the auto-generated notes (categorise by label, exclude bots, …), add
  a [`.github/release.yml`](https://docs.github.com/en/repositories/releasing-projects-on-github/automatically-generated-release-notes).

## Attribution

Please retain credit to easymodo and the qimgv contributors in any
user-facing About text and in `NOTICE`.
