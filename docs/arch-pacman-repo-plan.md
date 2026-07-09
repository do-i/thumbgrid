# Arch Pacman Repo Plan

## Goal

Make `pacman -Syu` automatically pick up rebuilt thumbgrid packages, so an
Arch ABI bump in Qt, Exiv2, OpenCV, or mpv doesn't leave the installed
thumbgrid binary linked against a library that no longer exists.

The current GitHub Release asset workflow produces a `thumbgrid.pkg.tar.zst`
installed manually with `pacman -U`, often from `/tmp`. Pacman then knows
thumbgrid is installed, but has no sync repository to check for rebuilt
packages, so it never gets refreshed by `pacman -Syu`.

## Decision

Distribute thumbgrid through a real (unsigned, for now) pacman binary
repository hosted on GitHub Pages. This is the only mechanism that makes
`pacman -Syu` discover rebuilds.

Explicitly not doing, for now:
- AUR packaging (no AUR account, not needed for this goal).
- Package/DB signing (`Optional TrustAll` is fine for personal use).
- Dependency-strictness tightening (Qt/OpenCV `.so` provides investigation).

These can be revisited later if the repo is opened up to other users.

## User-Facing Target

```ini
[thumbgrid]
SigLevel = Optional TrustAll
Server = https://do-i.github.io/thumbgrid/arch/$arch
```

```bash
sudo pacman -Sy thumbgrid   # one-time
sudo pacman -Syu            # picks up rebuilds from now on
```

## Implementation Steps

- [ ] Build the package: `makepkg -f --noconfirm` from `packaging/arch`.
- [ ] Create/update the repo DB:
      ```bash
      mkdir -p repo/arch/x86_64
      cp packaging/arch/*.pkg.tar.zst repo/arch/x86_64/
      repo-add repo/arch/x86_64/thumbgrid.db.tar.zst repo/arch/x86_64/*.pkg.tar.zst
      ```
- [ ] Publish `repo/` to GitHub Pages (`arch/x86_64/` path), including:
      - `thumbgrid.db`, `thumbgrid.db.tar.zst`
      - `thumbgrid.files`, `thumbgrid.files.tar.zst`
      - `thumbgrid-<pkgver>-<pkgrel>-x86_64.pkg.tar.zst`
- [ ] Add a GitHub Actions workflow (manual `workflow_dispatch`, and/or on
      `v*` tags) that runs the build + repo-add + publish steps above.
- [ ] Add the `[thumbgrid]` repo block to `/etc/pacman.conf` locally.
- [ ] Document the pacman repo install method in `README.md`, and note that
      `pacman -U thumbgrid.pkg.tar.zst` is a manual fallback with no
      auto-updates.

## Rebuild Policy

- App release: bump `pkgver`, reset `pkgrel=1`, rebuild, republish.
- Arch dependency ABI bump with no app code change: keep `pkgver`, bump
  `pkgrel`, rebuild, republish.
- Don't delete the previous `.pkg.tar.zst` until the new DB + Pages deploy
  are confirmed good — a failed deploy shouldn't leave the DB pointing at a
  missing package.

## Validation Checklist

- [ ] `makepkg -f` succeeds from `packaging/arch`.
- [ ] `repo-add` creates a valid `thumbgrid.db.tar.zst` and
      `thumbgrid.files.tar.zst`.
- [ ] A clean Arch environment can install with `pacman -S thumbgrid` from
      the hosted repo.
- [ ] A rebuild with the same `pkgver` and higher `pkgrel` is detected by
      `pacman -Syu`.
- [ ] `ldd /usr/bin/thumbgrid` and `ldd /usr/lib/thumbgrid/player_mpv.so`
      show no missing libraries after install.

## Deferred (not needed for the minimal goal)

- AUR package (`thumbgrid` / `thumbgrid-bin` / `thumbgrid-git`) — needs an
  AUR account, revisit later.
- Package/repo DB signing and maintainer key distribution.
- Tightening runtime deps to strict `.so` versions (Qt, OpenCV
  `provides` gap).
- Deciding whether repo hosting should move to a dedicated repository
  instead of GitHub Pages on this repo.
