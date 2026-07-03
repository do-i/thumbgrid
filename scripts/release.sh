#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# Interactive release manager.
#
# Branching model:
#   - `develop` is the integration branch. Feature branches merge into it with
#     --ff-only (rebase onto develop first), so develop is always a linear
#     history sitting directly on top of main.
#   - `main` never receives commits directly. It only ever advances by a
#     fast-forward of `develop` (done by this script), followed by a tag.
#     A direct push to main breaks that invariant; this script detects it
#     and refuses to release rather than guessing what to do.
#   - `release-<version>` branches are created manually, ad hoc, only to
#     patch an already-shipped version. This script never creates, reads, or
#     touches them; fixes there are cherry-picked into develop/main by hand.
#
# What "Cut release" does:
#   1. Fetches origin, verifies origin/main is an ancestor of origin/develop
#      (no direct pushes to main since the last release).
#   2. Verifies origin/develop's tip has a green tests.yml run (via gh).
#   3. Tags that commit and pushes main + the tag together, atomically.
#   4. Bumps develop's fallback dev version and pushes develop.
#
# "Bump develop version" does step 4 on its own, with no merge or tag - for
# syncing the CalVer fallback to the current month when releases are
# infrequent.
#
# Usage:
#   scripts/release.sh                     # interactive menu
#   scripts/release.sh cut                 # cut a release non-interactively
#   scripts/release.sh cut 2026.7.2        # cut an explicit version
#   scripts/release.sh bump                # bump develop's dev version only
#   scripts/release.sh bump 2026.8.1       # bump to an explicit version
#   scripts/release.sh status              # show state, make no changes
#   scripts/release.sh --dry-run ...       # show what would happen, change nothing
#   scripts/release.sh --skip-ci-check ... # bypass the gh CI status gate
#   scripts/release.sh --yes ...           # skip the confirmation prompt

set -euo pipefail

DEFAULT_BRANCH="main"
DEV_BRANCH="develop"

DRY_RUN=0
SKIP_CI_CHECK=0
ASSUME_YES=0
EXPLICIT_VERSION=""
COMMAND=""

for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=1 ;;
        --skip-ci-check) SKIP_CI_CHECK=1 ;;
        --yes|-y) ASSUME_YES=1 ;;
        -h|--help) sed -n '2,40p' "$0"; exit 0 ;;
        cut|bump|status) COMMAND="$arg" ;;
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

# Restores whatever ref was checked out before a detached-HEAD version bump,
# even if the script errors out mid-way (set -e triggers this via EXIT).
ORIGINAL_REF=""
restore_original_ref() {
    if [[ -n "$ORIGINAL_REF" ]]; then
        git checkout --quiet "$ORIGINAL_REF" 2>/dev/null || true
    fi
}
trap restore_original_ref EXIT

current_ref() {
    if git symbolic-ref -q HEAD >/dev/null; then
        git rev-parse --abbrev-ref HEAD
    else
        git rev-parse HEAD
    fi
}

read_fallback_version() {
    # $1: committish to read from (defaults to the working tree file).
    if [[ $# -ge 1 ]]; then
        git show "$1:CMakeLists.txt" 2>/dev/null \
            | sed -n 's/^set(THUMBGRID_VERSION_FALLBACK \([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)).*/\1/p'
    else
        sed -n 's/^set(THUMBGRID_VERSION_FALLBACK \([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)).*/\1/p' CMakeLists.txt
    fi
}

# Auto CalVer: major=year, minor=month, micro=sequential within the month,
# based on today's date and existing tags.
auto_next_version() {
    local year month prefix last_micro micro
    year="$(date +%Y)"
    month="$(date +%-m)"
    prefix="v${year}.${month}."
    last_micro="$(git tag --list "${prefix}*" \
        | sed "s|^${prefix}||" \
        | grep -E '^[0-9]+$' \
        | sort -n | tail -1 || true)"
    if [[ -z "$last_micro" ]]; then
        micro=1
    else
        micro=$((last_micro + 1))
    fi
    echo "${year}.${month}.${micro}"
}

fetch_all() {
    echo "Fetching origin..."
    if ! git fetch --quiet origin "$DEFAULT_BRANCH" "$DEV_BRANCH"; then
        echo "error: failed to fetch origin/$DEFAULT_BRANCH or origin/$DEV_BRANCH." >&2
        exit 1
    fi
    git fetch --quiet --tags origin \
        || echo "warning: 'git fetch --tags' reported issues; continuing with local tags." >&2
}

check_not_diverged() {
    if ! git merge-base --is-ancestor "origin/$DEFAULT_BRANCH" "origin/$DEV_BRANCH"; then
        echo "error: origin/$DEFAULT_BRANCH is not an ancestor of origin/$DEV_BRANCH." >&2
        echo "$DEFAULT_BRANCH has commits that aren't on $DEV_BRANCH - probably a direct push to $DEFAULT_BRANCH." >&2
        echo "Commits on $DEFAULT_BRANCH but missing from $DEV_BRANCH:" >&2
        git log --oneline "origin/$DEV_BRANCH..origin/$DEFAULT_BRANCH" >&2
        echo >&2
        echo "Rebase $DEV_BRANCH onto $DEFAULT_BRANCH and push it, then retry." >&2
        exit 1
    fi
}

check_ci_status() {
    local sha="$1"
    if [[ "$SKIP_CI_CHECK" == "1" ]]; then
        echo "warning: skipping CI status check (--skip-ci-check)." >&2
        return
    fi
    if ! command -v gh &>/dev/null; then
        echo "error: gh CLI not found; cannot verify CI status for $sha." >&2
        echo "Install gh, or pass --skip-ci-check to override." >&2
        exit 1
    fi
    local result
    if ! result="$(gh run list --branch "$DEV_BRANCH" --workflow tests.yml \
            --json headSha,status,conclusion --limit 30 \
            --jq "[.[] | select(.headSha == \"$sha\")][0] | \"\(.status) \(.conclusion)\"" 2>&1)"; then
        echo "error: 'gh run list' failed: $result" >&2
        exit 1
    fi
    local status conclusion
    read -r status conclusion <<<"$result"
    if [[ -z "$status" || "$status" == "null" ]]; then
        echo "error: no tests.yml run found for $DEV_BRANCH commit $sha." >&2
        echo "Push $DEV_BRANCH and wait for CI, or pass --skip-ci-check to override." >&2
        exit 1
    fi
    if [[ "$status" != "completed" || "$conclusion" != "success" ]]; then
        echo "error: tests.yml for $DEV_BRANCH commit $sha is '$status'/'$conclusion', not a passing run." >&2
        exit 1
    fi
    echo "CI check: tests.yml passed for $DEV_BRANCH @ ${sha:0:12}."
}

confirm() {
    local prompt="$1"
    if [[ "$ASSUME_YES" == "1" || "$DRY_RUN" == "1" ]]; then
        return 0
    fi
    local reply
    read -rp "$prompt [y/N] " reply
    [[ "$reply" =~ ^[Yy]$ ]]
}

# Checks out a detached HEAD at $1, sets THUMBGRID_VERSION_FALLBACK to $2,
# commits with message $3, pushes to $DEV_BRANCH, then restores the ref that
# was checked out before this ran (via the ORIGINAL_REF/EXIT-trap machinery
# above, so it's restored even on error).
bump_version_commit() {
    local base_sha="$1" new_version="$2" message="$3"
    ORIGINAL_REF="$(current_ref)"

    run git checkout --quiet --detach "$base_sha"

    if grep -q 'set(THUMBGRID_VERSION_FALLBACK ' CMakeLists.txt; then
        run sed -i "s|set(THUMBGRID_VERSION_FALLBACK .*)|set(THUMBGRID_VERSION_FALLBACK ${new_version})|" CMakeLists.txt
        if [[ "$DRY_RUN" == "1" ]] || ! git diff --quiet CMakeLists.txt; then
            run git add CMakeLists.txt
            run git commit --quiet -m "$message"
            run git push origin "HEAD:refs/heads/$DEV_BRANCH"
        else
            echo "CMakeLists.txt fallback version is already ${new_version}; nothing to bump."
        fi
    else
        echo "warning: THUMBGRID_VERSION_FALLBACK not found in CMakeLists.txt; skipping bump." >&2
    fi

    run git checkout --quiet "$ORIGINAL_REF"
    ORIGINAL_REF=""
}

show_status() {
    fetch_all
    local main_sha develop_sha
    main_sha="$(git rev-parse "origin/$DEFAULT_BRANCH")"
    develop_sha="$(git rev-parse "origin/$DEV_BRANCH")"
    echo "origin/$DEFAULT_BRANCH: ${main_sha:0:12}"
    echo "origin/$DEV_BRANCH:  ${develop_sha:0:12}"
    echo
    if git merge-base --is-ancestor "origin/$DEFAULT_BRANCH" "origin/$DEV_BRANCH"; then
        echo "Commits on $DEV_BRANCH not yet on $DEFAULT_BRANCH:"
        git log --oneline "origin/$DEFAULT_BRANCH..origin/$DEV_BRANCH"
    else
        echo "WARNING: $DEFAULT_BRANCH has diverged from $DEV_BRANCH (direct push to $DEFAULT_BRANCH?)."
        echo "Commits on $DEFAULT_BRANCH not on $DEV_BRANCH:"
        git log --oneline "origin/$DEV_BRANCH..origin/$DEFAULT_BRANCH"
    fi
    echo
    echo "Current $DEV_BRANCH fallback version: $(read_fallback_version "origin/$DEV_BRANCH")"
    echo "Auto-computed version for today:   $(auto_next_version)"
}

cmd_cut() {
    if [[ -n "$(git status --porcelain)" ]]; then
        echo "error: working tree is dirty. Commit or stash first." >&2
        exit 1
    fi
    fetch_all
    check_not_diverged

    local develop_sha version tag
    develop_sha="$(git rev-parse "origin/$DEV_BRANCH")"
    check_ci_status "$develop_sha"

    version="${EXPLICIT_VERSION:-$(auto_next_version)}"
    tag="v${version}"
    if ! [[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "error: version '$version' is not MAJOR.MINOR.PATCH." >&2
        exit 1
    fi
    if git rev-parse "$tag" >/dev/null 2>&1; then
        echo "error: tag $tag already exists." >&2
        exit 1
    fi

    echo "Will fast-forward $DEFAULT_BRANCH to $DEV_BRANCH @ ${develop_sha:0:12} and tag it $tag."
    echo "Commits landing on $DEFAULT_BRANCH:"
    git log --oneline "origin/$DEFAULT_BRANCH..origin/$DEV_BRANCH"
    if ! confirm "Proceed with release $tag?"; then
        echo "Aborted."
        exit 1
    fi

    run git tag -a "$tag" "$develop_sha" -m "thumbgrid ${version}"
    run git push --atomic origin "${develop_sha}:refs/heads/${DEFAULT_BRANCH}" "refs/tags/${tag}"
    echo "Pushed $DEFAULT_BRANCH and $tag."
    echo "arch-package.yml will build the package and publish the GitHub Release."

    local next_version
    next_version="$(auto_next_version)"
    bump_version_commit "$develop_sha" "$next_version" "Bump dev version to ${next_version}"

    echo
    echo "Done. Released $tag; $DEV_BRANCH now at dev version $next_version."
}

cmd_bump() {
    if [[ -n "$(git status --porcelain)" ]]; then
        echo "error: working tree is dirty. Commit or stash first." >&2
        exit 1
    fi
    fetch_all
    local develop_sha version current
    develop_sha="$(git rev-parse "origin/$DEV_BRANCH")"
    version="${EXPLICIT_VERSION:-$(auto_next_version)}"
    current="$(read_fallback_version "origin/$DEV_BRANCH")"
    if [[ "$current" == "$version" ]]; then
        echo "$DEV_BRANCH's fallback version is already $version; nothing to do."
        exit 0
    fi
    echo "Will bump $DEV_BRANCH's fallback version from $current to $version (no merge, no tag)."
    if ! confirm "Proceed?"; then
        echo "Aborted."
        exit 1
    fi
    bump_version_commit "$develop_sha" "$version" "Bump dev version to ${version}"
    echo "Done."
}

menu() {
    while true; do
        echo
        echo "thumbgrid release manager"
        echo "  1) Cut release   ($DEV_BRANCH -> $DEFAULT_BRANCH, tag, push, bump $DEV_BRANCH)"
        echo "  2) Bump $DEV_BRANCH version only (sync CalVer to current month, no release)"
        echo "  3) Show status"
        echo "  4) Quit"
        read -rp "Select an option [1-4]: " choice
        case "$choice" in
            1) cmd_cut ;;
            2) cmd_bump ;;
            3) show_status ;;
            4) exit 0 ;;
            *) echo "invalid choice" ;;
        esac
    done
}

case "$COMMAND" in
    cut) cmd_cut ;;
    bump) cmd_bump ;;
    status) show_status ;;
    "") menu ;;
esac
