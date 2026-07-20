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
#   1. Looks up the chosen release on GitHub (aborting - or, in --dry-run,
#      just warning - on drafts/prereleases unless confirmed).
#   2. Clones thumbgrid-bin from AUR anonymously over HTTPS (no SSH key
#      needed) to read the currently-published pkgver/pkgrel: pkgrel is
#      always derived from what's actually on AUR, never from this repo's
#      working tree, since the two can be out of sync (publish from another
#      machine, a --dry-run edit, a lost commit).
#   3. Picks the newest matching x86_64 asset (name + sha256) on that
#      release - a release can carry more than one srcrel.
#   4. Renders packaging/arch-bin/PKGBUILD.template into the AUR clone and
#      regenerates .SRCINFO there. This repo's worktree is never modified.
#   5. Shows the exact AUR diff, commits it, switches that clone's remote to
#      SSH, and pushes - retrying, since aur.archlinux.org's
#      git/SSH service is known to reset connections intermittently
#      (confirmed: has taken up to ~8 attempts in practice).
#
# Usage: publish-aur.sh [version] [--dry-run]
#   version    e.g. 2026.7.12 or v2026.7.12; defaults to the latest git tag.
#   --dry-run  Do everything up through regenerating .SRCINFO (including the
#              anonymous AUR clone), print a diff against both this repo's
#              last commit and the current AUR package, then stop before any
#              commit or push.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKG_TEMPLATE="$ROOT_DIR/packaging/arch-bin/PKGBUILD.template"
REPO="do-i/thumbgrid"
# Clone anonymously (no auth needed, works in --dry-run without an SSH key);
# only the final push switches to the SSH remote.
AUR_HTTPS_REMOTE="https://aur.archlinux.org/thumbgrid-bin.git"
AUR_SSH_REMOTE="ssh://aur@aur.archlinux.org/thumbgrid-bin.git"
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

# Status-aware curl: a 4xx (bad tag, missing asset, ...) is a permanent
# answer from the server and retrying it 10x just wastes ~50s, so it fails
# immediately. Only transport errors (curl itself failing, reported as
# status "000") and 5xx responses are treated as transient and retried,
# same bound/delay as retry() above. $5 (hint) is optional extra text
# printed only on the permanent-failure path.
curl_fetch() {
    local desc="$1" url="$2" outfile="$3" max_time="$4" hint="${5:-}"
    local attempt=1 status
    while true; do
        status="$(curl -sSL --max-time "$max_time" -w '%{http_code}' -o "$outfile" "$url")" || status="000"
        case "$status" in
            2??) return 0 ;;
            4??)
                printf '%s failed: HTTP %s (permanent, not retrying).\n' "$desc" "$status" >&2
                [[ -n "$hint" ]] && printf '%s\n' "$hint" >&2
                return 1
                ;;
        esac
        if (( attempt >= RETRY_ATTEMPTS )); then
            printf '%s failed after %d attempts (last: HTTP %s).\n' "$desc" "$RETRY_ATTEMPTS" "$status" >&2
            return 1
        fi
        printf '%s failed (HTTP %s, attempt %d/%d), retrying in %ds...\n' "$desc" "$status" "$attempt" "$RETRY_ATTEMPTS" "$RETRY_DELAY" >&2
        sleep "$RETRY_DELAY"
        attempt=$((attempt + 1))
    done
}

# Escapes ERE metacharacters so a literal pkgver (which contains dots, e.g.
# "2026.7.12") can't be mis-parsed as part of the jq test() pattern below.
escape_regex() {
    printf '%s' "$1" | sed 's/[.[\*^$+?{}()|]/\\&/g'
}

# TAG is the raw git tag, used verbatim for the GitHub API lookup. PKGVER is
# CI's normalized form of it (see .github workflows: s/^v//; s/-/_/g -
# dashes aren't legal in pacman versions), used for asset names, PKGBUILD
# fields, and the AUR pkgver comparison. The two diverge for pre-release-style
# tags (e.g. tag v2026.7.12-rc1 -> pkgver 2026.7.12_rc1), so deriving TAG from
# PKGVER (as the old `TAG="v$PKGVER"` did) breaks; TAG must come from the raw
# input instead.
if [[ -n "$VERSION_ARG" ]]; then
    case "$VERSION_ARG" in
        v*) TAG="$VERSION_ARG" ;;
        *)  TAG="v$VERSION_ARG" ;;
    esac
else
    TAG="$(git -C "$ROOT_DIR" describe --tags --abbrev=0)"
fi
PKGVER="$(printf '%s' "$TAG" | sed -e 's/^v//' -e 's/-/_/g')"

printf 'Publishing thumbgrid %s to AUR (thumbgrid-bin)...\n' "$TAG"

RELEASE_JSON="$(mktemp)"
trap 'rm -f "$RELEASE_JSON"' EXIT
curl_fetch "GitHub release lookup" "https://api.github.com/repos/$REPO/releases/tags/$TAG" "$RELEASE_JSON" 20 \
    'Has the "Arch package" workflow finished for this tag?' || exit 1

# Guard against promoting something not meant for general use yet.
IS_DRAFT="$(jq -r '.draft' "$RELEASE_JSON")"
IS_PRERELEASE="$(jq -r '.prerelease' "$RELEASE_JSON")"
RELEASE_FLAGS=""
if [[ "$IS_DRAFT" == "true" ]]; then
    RELEASE_FLAGS="draft"
fi
if [[ "$IS_PRERELEASE" == "true" ]]; then
    if [[ -n "$RELEASE_FLAGS" ]]; then
        RELEASE_FLAGS="$RELEASE_FLAGS, prerelease"
    else
        RELEASE_FLAGS="prerelease"
    fi
fi
if [[ -n "$RELEASE_FLAGS" ]]; then
    printf 'Warning: release %s is marked %s on GitHub.\n' "$TAG" "$RELEASE_FLAGS" >&2
    if (( DRY_RUN )); then
        printf 'Dry run: a real run would prompt for confirmation before publishing a %s release.\n' "$RELEASE_FLAGS" >&2
    else
        read -r -p "Publish this $RELEASE_FLAGS release to AUR anyway? [y/N] " answer
        case "$answer" in
            [yY]|[yY][eE][sS]) ;;
            *) printf 'Aborted.\n' >&2; exit 1 ;;
        esac
    fi
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -f "$RELEASE_JSON"; rm -rf "$WORK_DIR"' EXIT

# rm -rf the destination first so a retry after a partial clone failure
# doesn't fail merely because the directory already exists / is non-empty.
clone_aur() {
    rm -rf "$WORK_DIR/thumbgrid-bin"
    git clone -q "$AUR_HTTPS_REMOTE" "$WORK_DIR/thumbgrid-bin"
}
retry "AUR clone" clone_aur

AUR_PKGBUILD="$WORK_DIR/thumbgrid-bin/PKGBUILD"
if [[ -f "$AUR_PKGBUILD" ]]; then
    AUR_PKGVER="$(grep -E '^pkgver=' "$AUR_PKGBUILD" | cut -d= -f2)"
    AUR_PKGREL="$(grep -E '^pkgrel=' "$AUR_PKGBUILD" | cut -d= -f2)"
else
    # Fresh AUR package: empty clone, no PKGBUILD yet.
    AUR_PKGVER=""
    AUR_PKGREL=0
fi

if [[ "$AUR_PKGVER" == "$PKGVER" ]]; then
    NEW_PKGREL=$((AUR_PKGREL + 1))
    printf 'thumbgrid-bin on AUR is already at pkgver %s; bumping pkgrel to %s.\n' "$PKGVER" "$NEW_PKGREL"
else
    NEW_PKGREL=1
fi

PKGVER_RE="$(escape_regex "$PKGVER")"
mapfile -t ASSET_NAMES < <(jq -r --arg pkgver "$PKGVER_RE" \
    '.assets[] | select(.name | test("^thumbgrid-" + $pkgver + "-[0-9]+-x86_64\\.pkg\\.tar\\.zst$")) | .name' \
    "$RELEASE_JSON")

# A release can carry more than one srcrel (e.g. a re-run of the "Arch
# package" workflow); GitHub lists assets in upload order, not version
# order, so pick the highest srcrel rather than the first one.
ASSET_NAME=""
BEST_SRCREL=-1
for name in "${ASSET_NAMES[@]}"; do
    srcrel="${name#thumbgrid-"$PKGVER"-}"
    srcrel="${srcrel%-x86_64.pkg.tar.zst}"
    srcrel=$((10#$srcrel))
    if (( srcrel > BEST_SRCREL )); then
        BEST_SRCREL="$srcrel"
        ASSET_NAME="$name"
    fi
done
ASSET_URL="$(jq -r --arg name "$ASSET_NAME" '.assets[] | select(.name == $name) | .browser_download_url' "$RELEASE_JSON")"

if [[ -z "$ASSET_NAME" || -z "$ASSET_URL" ]]; then
    printf 'No x86_64 package asset found on release %s.\n' "$TAG" >&2
    printf 'Has the "Arch package" workflow finished for this tag?\n' >&2
    exit 1
fi
SRCREL="$BEST_SRCREL"

printf 'Found asset: %s (srcrel=%s)\n' "$ASSET_NAME" "$SRCREL"

ASSET_FILE="$(mktemp)"
trap 'rm -f "$RELEASE_JSON" "$ASSET_FILE"; rm -rf "$WORK_DIR"' EXIT
curl_fetch "Asset download" "$ASSET_URL" "$ASSET_FILE" 60 || exit 1
SHA256="$(sha256sum "$ASSET_FILE" | cut -d' ' -f1)"
printf 'sha256: %s\n' "$SHA256"

cp "$PKG_TEMPLATE" "$WORK_DIR/thumbgrid-bin/PKGBUILD"
sed -i \
    -e "s/@PKGVER@/$PKGVER/g" \
    -e "s/@PKGREL@/$NEW_PKGREL/g" \
    -e "s/@SRCREL@/$SRCREL/g" \
    -e "s/@SHA256@/$SHA256/g" \
    "$WORK_DIR/thumbgrid-bin/PKGBUILD"

(cd "$WORK_DIR/thumbgrid-bin" && makepkg --printsrcinfo > .SRCINFO)

pushd "$WORK_DIR/thumbgrid-bin" >/dev/null
# Stage in the temporary clone so the same review output also works for a
# brand-new AUR package whose PKGBUILD and .SRCINFO are not tracked yet.
git add PKGBUILD .SRCINFO
printf '\n--- PKGBUILD/.SRCINFO diff (against the current AUR package) ---\n'
git --no-pager diff --cached -- PKGBUILD .SRCINFO || true
printf -- '---\n\n'

if (( DRY_RUN )); then
    printf 'Dry run: stopping before commit/push.\n'
    popd >/dev/null
    exit 0
fi

if git diff --cached --quiet; then
    printf 'thumbgrid-bin on AUR is already at %s-%s; nothing to push.\n' "$PKGVER" "$NEW_PKGREL"
    popd >/dev/null
    exit 0
fi
git commit -q -m "thumbgrid-bin $PKGVER-$NEW_PKGREL"
# Switch this clone's remote to SSH only now, for the push - everything
# before this point (including --dry-run) only ever needed anonymous HTTPS.
git remote set-url origin "$AUR_SSH_REMOTE"
retry "AUR push" git push -q origin HEAD:master
popd >/dev/null

printf '\nPublished thumbgrid-bin %s-%s to AUR.\n' "$PKGVER" "$NEW_PKGREL"
printf 'https://aur.archlinux.org/packages/thumbgrid-bin\n'
