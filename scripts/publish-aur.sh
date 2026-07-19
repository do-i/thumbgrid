#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# Publish a chosen GitHub release to the thumbgrid-bin AUR package.
#
# Deliberately NOT run by CI on every tag push: releases go to GitHub and the
# custom pacman repo (gh-pages) far more often, for testing, than they should
# reach AUR. This script is the manual, on-demand "promote this version to
# AUR once I trust it" step - run via `./run.sh` or directly.
#
# What it does:
#   1. Looks up the chosen release's exact asset (name + sha256) on GitHub.
#   2. Updates packaging/arch-bin/PKGBUILD + regenerates .SRCINFO.
#   3. Commits that change locally in this repo (not pushed - review/push it
#      yourself with your normal git workflow).
#   4. Clones thumbgrid-bin from AUR, copies the two files in, commits, and
#      pushes - retrying, since aur.archlinux.org's git/SSH service is known
#      to reset connections intermittently (confirmed: has taken up to ~8
#      attempts in practice).
#
# Usage: publish-aur.sh [version] [--dry-run]
#   version    e.g. 2026.7.12 or v2026.7.12; defaults to the latest git tag.
#   --dry-run  Do everything up through regenerating .SRCINFO, print what
#              would be committed/pushed, then stop.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKG_DIR="$ROOT_DIR/packaging/arch-bin"
REPO="do-i/thumbgrid"
AUR_REMOTE="ssh://aur@aur.archlinux.org/thumbgrid-bin.git"
RETRY_ATTEMPTS=10
RETRY_DELAY=5

DRY_RUN=0
VERSION_ARG=""
for arg in "$@"; do
    case "$arg" in
        -n|--dry-run) DRY_RUN=1 ;;
        -h|--help)
            printf 'Usage: %s [version] [--dry-run]\n' "$(basename "$0")"
            printf 'Publish a GitHub release of thumbgrid to the thumbgrid-bin AUR package.\n'
            exit 0
            ;;
        -*)
            printf 'Unknown option: %s\n' "$arg" >&2
            exit 1
            ;;
        *) VERSION_ARG="$arg" ;;
    esac
done

for tool in curl jq sha256sum makepkg bsdtar git; do
    command -v "$tool" >/dev/null 2>&1 || { printf '%s is required but not found.\n' "$tool" >&2; exit 1; }
done

# Retries a command a bounded number of times with a fixed delay. AUR's
# git/SSH service resets connections intermittently under normal load; a
# single failure is not a real error here, only exhausting retries is.
retry() {
    local attempt=1
    local desc="$1"
    shift
    while true; do
        if "$@"; then
            return 0
        fi
        if (( attempt >= RETRY_ATTEMPTS )); then
            printf '%s failed after %d attempts.\n' "$desc" "$RETRY_ATTEMPTS" >&2
            return 1
        fi
        printf '%s failed (attempt %d/%d), retrying in %ds...\n' "$desc" "$attempt" "$RETRY_ATTEMPTS" "$RETRY_DELAY" >&2
        sleep "$RETRY_DELAY"
        attempt=$((attempt + 1))
    done
}

if [[ -n "$VERSION_ARG" ]]; then
    PKGVER="${VERSION_ARG#v}"
else
    PKGVER="$(git -C "$ROOT_DIR" describe --tags --abbrev=0)"
    PKGVER="${PKGVER#v}"
fi
TAG="v$PKGVER"

printf 'Publishing thumbgrid %s to AUR (thumbgrid-bin)...\n' "$TAG"

RELEASE_JSON="$(mktemp)"
trap 'rm -f "$RELEASE_JSON"' EXIT
retry "GitHub release lookup" curl -sSfL --max-time 20 \
    "https://api.github.com/repos/$REPO/releases/tags/$TAG" -o "$RELEASE_JSON"

ASSET_NAME="$(jq -r --arg pkgver "$PKGVER" \
    '.assets[] | select(.name | test("^thumbgrid-" + $pkgver + "-[0-9]+-x86_64\\.pkg\\.tar\\.zst$")) | .name' \
    "$RELEASE_JSON" | head -1)"
ASSET_URL="$(jq -r --arg name "$ASSET_NAME" '.assets[] | select(.name == $name) | .browser_download_url' "$RELEASE_JSON")"

if [[ -z "$ASSET_NAME" || -z "$ASSET_URL" ]]; then
    printf 'No x86_64 package asset found on release %s.\n' "$TAG" >&2
    printf 'Has the "Arch package" workflow finished for this tag?\n' >&2
    exit 1
fi

# thumbgrid-<pkgver>-<srcrel>-x86_64.pkg.tar.zst -> srcrel
SRCREL="$(printf '%s' "$ASSET_NAME" | sed -E "s/^thumbgrid-${PKGVER}-([0-9]+)-x86_64\.pkg\.tar\.zst\$/\1/")"

printf 'Found asset: %s (srcrel=%s)\n' "$ASSET_NAME" "$SRCREL"

ASSET_FILE="$(mktemp)"
trap 'rm -f "$RELEASE_JSON" "$ASSET_FILE"' EXIT
retry "Asset download" curl -sSfL --max-time 60 "$ASSET_URL" -o "$ASSET_FILE"
SHA256="$(sha256sum "$ASSET_FILE" | cut -d' ' -f1)"
printf 'sha256: %s\n' "$SHA256"

CURRENT_PKGVER="$(grep -E '^pkgver=' "$PKG_DIR/PKGBUILD" | cut -d= -f2)"
if [[ "$CURRENT_PKGVER" == "$PKGVER" ]]; then
    CURRENT_PKGREL="$(grep -E '^pkgrel=' "$PKG_DIR/PKGBUILD" | cut -d= -f2)"
    NEW_PKGREL=$((CURRENT_PKGREL + 1))
    printf 'Republishing the same pkgver; bumping pkgrel to %s.\n' "$NEW_PKGREL"
else
    NEW_PKGREL=1
fi

sed -i \
    -e "s/^pkgver=.*/pkgver=$PKGVER/" \
    -e "s/^pkgrel=.*/pkgrel=$NEW_PKGREL/" \
    -e "s/^_srcrel=.*/_srcrel=$SRCREL/" \
    -e "s/^sha256sums=.*/sha256sums=('$SHA256')/" \
    "$PKG_DIR/PKGBUILD"

(cd "$PKG_DIR" && makepkg --printsrcinfo > .SRCINFO)

printf '\n--- packaging/arch-bin/PKGBUILD diff ---\n'
git -C "$ROOT_DIR" --no-pager diff -- packaging/arch-bin/ || true
printf -- '---\n\n'

if (( DRY_RUN )); then
    printf 'Dry run: stopping before commit/push.\n'
    exit 0
fi

if ! git -C "$ROOT_DIR" diff --quiet -- packaging/arch-bin/; then
    git -C "$ROOT_DIR" add packaging/arch-bin/PKGBUILD packaging/arch-bin/.SRCINFO
    git -C "$ROOT_DIR" commit -m "Publish thumbgrid-bin $PKGVER-$NEW_PKGREL to AUR" -q
    printf 'Committed locally in %s. Push it yourself when ready.\n' "$ROOT_DIR"
else
    printf 'PKGBUILD already up to date; nothing to commit.\n'
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -f "$RELEASE_JSON" "$ASSET_FILE"; rm -rf "$WORK_DIR"' EXIT

retry "AUR clone" git clone -q "$AUR_REMOTE" "$WORK_DIR/thumbgrid-bin"
cp "$PKG_DIR/PKGBUILD" "$PKG_DIR/.SRCINFO" "$WORK_DIR/thumbgrid-bin/"

pushd "$WORK_DIR/thumbgrid-bin" >/dev/null
# --cached (not plain diff) so this also sees copied-but-untracked files
# correctly on a from-scratch clone, not just changes to already-tracked ones.
git add PKGBUILD .SRCINFO
if git diff --cached --quiet; then
    printf 'thumbgrid-bin on AUR is already at %s-%s; nothing to push.\n' "$PKGVER" "$NEW_PKGREL"
    popd >/dev/null
    exit 0
fi
git commit -q -m "thumbgrid-bin $PKGVER-$NEW_PKGREL"
retry "AUR push" git push -q origin HEAD:master
popd >/dev/null

printf '\nPublished thumbgrid-bin %s-%s to AUR.\n' "$PKGVER" "$NEW_PKGREL"
printf 'https://aur.archlinux.org/packages/thumbgrid-bin\n'
