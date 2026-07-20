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

### Ownership

Make ownership explicit in the type; a reader should never have to guess who
frees an object.

- **QObjects / QWidgets** → rely on **Qt parent ownership**. Construct with a
  parent (`new Foo(this)`) and let the parent destroy the child. Don't wrap a
  parented QObject in a smart pointer and don't `delete` it manually.
- **Everything else that is owned** → express ownership in the type:
  `std::unique_ptr<T>` for a single owner (use it for factory returns, and where
  a null return signals "none"), `std::shared_ptr<T>` for genuinely shared
  ownership, or a plain **value** where the type is cheap to copy or move
  (`QImage`/`QPixmap` are implicitly shared — returning them by value is cheap
  and leak-proof). Prefer value/`unique_ptr` returns over `new T` returning a
  raw pointer.
- **Raw pointers (`T*`) are non-owning observers only.** A raw pointer may
  borrow an object for the duration of a call (e.g. a `const QImage*` argument
  a function only reads), but it must never be the thing responsible for
  `delete`. If you find a `new`/`delete` pair spanning a raw pointer, convert it
  to one of the owning forms above.
- **Crossing a thread boundary via a queued signal** is the one place a raw
  owning pointer is still tolerated (Qt queued connections can't move a
  `unique_ptr`): `release()` into the signal and have the receiving slot adopt
  it immediately. Keep that hand-off obvious and localised.

### Naming

- **Member variables** get an `m` prefix: `mFoo`. No trailing underscores.
- **Parameters and locals** are plain camelCase: `foo`. No leading underscores —
  the historical `_foo` constructor-parameter style existed only to dodge
  shadowing an unprefixed member, which `mFoo` already solves
  (`Foo(int foo) : mFoo(foo)`).
- The codebase predates this convention and is mixed (`mFoo`/`foo`, `_foo`/`foo`).
  **Rename only opportunistically** — when already editing the class for another
  reason — never in bulk renaming sweeps.

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

### What a tag push builds

Pushing `v*` fans out to two workflows, both attaching their artifact to the
same GitHub Release:

- [`arch-package.yml`](.github/workflows/arch-package.yml) — builds the
  full-featured Arch package (Exiv2 / OpenCV / mpv) and, beyond uploading the
  `.pkg.tar.zst`, publishes the **pacman repo** on the `gh-pages` branch
  (`arch/x86_64/`, via `repo-add --prevent-downgrade`). That repo is what
  users' `pacman -Syu` reads, so this step — not AUR — is what ships an update
  to existing installs.
- [`mac-package.yml`](.github/workflows/mac-package.yml) — builds the macOS
  app bundle and packages it as a DMG.

### ABI-only rebuilds

When Arch updates Qt, mpv, Exiv2, or OpenCV, the shipped package can break
against the new libraries with no thumbgrid code change. Republish the latest
tag instead of cutting a version: run `arch-package.yml` via
**`workflow_dispatch`** with `pkgrel` bumped (2, 3, …). It rebuilds the
existing tag — pinning the worktree to it, so a moved `develop` can't leak
newer code under an old version — attaches the new package to that tag's
existing release alongside the old one, and refreshes the pacman repo.

### Publishing to AUR

The AUR package (`thumbgrid-bin`) is **deliberately not** updated by CI.
Releases reach GitHub and the pacman repo far more often, including for
testing, than they should reach AUR; promotion is a manual "I trust this
version now" step:

```
./scripts/publish-aur.sh --dry-run     # latest tag, stop before any commit/push
./scripts/publish-aur.sh 2026.7.12     # publish a specific version
```

It is also item `a` in `./run.sh`'s menu. What it does:

1. Looks up the release on GitHub, prompting for confirmation if it is marked
   draft or prerelease.
2. Clones `thumbgrid-bin` from AUR anonymously over HTTPS and reads the
   currently published `pkgver`/`pkgrel`. **`pkgrel` is always derived from
   what is live on AUR, never from this repo's working tree** — the two drift
   (publishing from another machine, an abandoned `--dry-run` edit, a lost
   commit), and AUR is the only authority on what was actually shipped.
3. Picks the **highest** `_srcrel` x86_64 asset on the release — an ABI-only
   rebuild leaves several, and GitHub lists assets in upload order, not
   version order — and hashes it.
4. Renders the version-neutral
   [`packaging/arch-bin/PKGBUILD.template`](packaging/arch-bin/PKGBUILD.template)
   directly into the temporary AUR clone and generates `.SRCINFO` there. The
   thumbgrid worktree is never modified and contains no stale release version.
5. Shows the exact staged AUR diff, then commits and pushes those two concrete
   files to AUR over SSH. `aur.archlinux.org` resets
   git/SSH connections intermittently under normal load, so the push retries
   up to 10 times — in practice it has taken ~8 attempts. Repeated failures
   are normal; only exhausting the retries is a real error.

`--dry-run` runs everything through `.SRCINFO` regeneration and shows the exact
diff against the live AUR package, then stops. Use it before every publish.

Note that a tag with a dash (`v2026.7.12-rc1`) normalises to a different
pacman version (`2026.7.12_rc1`) — dashes are not legal in pacman versions.
The script tracks both forms; pass it the git tag.

### Packaging consistency

[`scripts/check-packaging.sh`](scripts/check-packaging.sh), run in CI by
[`packaging-checks.yml`](.github/workflows/packaging-checks.yml), catches
`depends` drift between `packaging/arch/PKGBUILD` and the rendered
`packaging/arch-bin/PKGBUILD.template`, and verifies that the template renders
valid package metadata without stale release values. Run it locally after
touching either packaging recipe.

## Attribution

Please retain credit to easymodo and the qimgv contributors in any
user-facing About text and in `NOTICE`.
