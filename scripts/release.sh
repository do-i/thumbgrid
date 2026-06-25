#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# One-command release. The git tag is the single source of truth for the
# version; CMake derives everything else from it (see CMakeLists.txt).
#
# What this does:
#   1. Sanity-checks: on the default branch, clean tree, up to date with remote.
#   2. Picks the version: explicit arg, or auto-next CalVer (YYYY.M.N).
#   3. Updates the fallback version in CMakeLists.txt (for source tarballs).
#   4. Commits that, creates an annotated tag vX, and pushes both.
#   5. CI (arch-package.yml) then builds the package and publishes a GitHub
#      Release with auto-generated notes.
#
# Usage:
#   scripts/release.sh                # auto-pick next CalVer for this month
#   scripts/release.sh 2026.7.1       # explicit version (no leading 'v')
#   scripts/release.sh --dry-run      # show what would happen, change nothing

set -euo pipefail

DRY_RUN=0
EXPLICIT_VERSION=""
DEFAULT_BRANCH="main"

for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=1 ;;
        -h|--help) sed -n '2,21p' "$0"; exit 0 ;;
        v*) EXPLICIT_VERSION="${arg#v}" ;;
        [0-9]*) EXPLICIT_VERSION="$arg" ;;
        *) echo "error: unknown argument '$arg'" >&2; exit 2 ;;
    esac
done

run() {
    if [[ "$DRY_RUN" == "1" ]]; then
        echo "  [dry-run] $*"
    else
        "$@"
    fi
}

cd "$(git rev-parse --show-toplevel)"

# --- sanity checks --------------------------------------------------------
branch="$(git rev-parse --abbrev-ref HEAD)"
if [[ "$branch" != "$DEFAULT_BRANCH" ]]; then
    echo "error: not on $DEFAULT_BRANCH (on '$branch'). Release from $DEFAULT_BRANCH." >&2
    exit 1
fi
if [[ -n "$(git status --porcelain)" ]]; then
    echo "error: working tree is dirty. Commit or stash first." >&2
    exit 1
fi
# Refresh the remote branch for the behind-check below. Fail loudly, never silently.
if ! git fetch --quiet origin "$DEFAULT_BRANCH"; then
    echo "error: failed to fetch origin/$DEFAULT_BRANCH (network or remote problem)." >&2
    exit 1
fi
# Refresh tags too, but a tag mismatch (diverged tags) must not abort the release silently.
git fetch --quiet --tags origin \
    || echo "warning: 'git fetch --tags' reported issues (diverged tags?); continuing with local tags." >&2
if [[ -n "$(git rev-list "HEAD..origin/$DEFAULT_BRANCH" 2>/dev/null)" ]]; then
    echo "error: local $DEFAULT_BRANCH is behind origin/$DEFAULT_BRANCH. Pull first." >&2
    exit 1
fi

# --- determine version ----------------------------------------------------
if [[ -n "$EXPLICIT_VERSION" ]]; then
    VERSION="$EXPLICIT_VERSION"
else
    # Auto CalVer: major=year, minor=month, micro=sequential within the month.
    year="$(date +%Y)"
    month="$(date +%-m)"
    prefix="v${year}.${month}."
    # Highest existing micro for this YYYY.M, else start at 1.
    last_micro="$(git tag --list "${prefix}*" \
        | sed "s|^${prefix}||" \
        | grep -E '^[0-9]+$' \
        | sort -n | tail -1)"
    if [[ -z "$last_micro" ]]; then
        micro=1
    else
        micro=$((last_micro + 1))
    fi
    VERSION="${year}.${month}.${micro}"
fi

TAG="v${VERSION}"

if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "error: version '$VERSION' is not MAJOR.MINOR.PATCH." >&2
    exit 1
fi
if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "error: tag $TAG already exists." >&2
    exit 1
fi

echo "Releasing $TAG (from branch $branch)"

# --- update fallback version in CMakeLists.txt ----------------------------
# Keeps source-tarball builds (no .git) reporting the right version.
if grep -q 'set(THUMBGRID_VERSION_FALLBACK ' CMakeLists.txt; then
    run sed -i "s|set(THUMBGRID_VERSION_FALLBACK .*)|set(THUMBGRID_VERSION_FALLBACK ${VERSION})|" CMakeLists.txt
    if [[ "$DRY_RUN" == "1" ]] || ! git diff --quiet CMakeLists.txt; then
        run git add CMakeLists.txt
        run git commit -m "Release ${TAG}"
    fi
else
    echo "warning: THUMBGRID_VERSION_FALLBACK not found in CMakeLists.txt; skipping bump." >&2
fi

# --- tag and push ---------------------------------------------------------
run git tag -a "$TAG" -m "thumbgrid ${VERSION}"
run git push origin "$DEFAULT_BRANCH"
run git push origin "$TAG"

echo
echo "Done. Pushed $TAG."
echo "CI will build the package and publish the GitHub Release with"
echo "auto-generated notes (arch-package.yml, generate_release_notes: true)."
