#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# Interactive release manager.
#
# Versioning:
#   - The version is derived entirely from the latest git tag (vYYYY.M.N) at
#     build time (see CMakeLists.txt / cmake/GenerateAppVersion.cmake). There is
#     no version constant in the tree to maintain, so this script only tags;
#     it never edits or commits source. Dev builds automatically show the tag
#     plus a "-<count>-g<hash>" suffix from `git describe`.
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
#
# Usage:
#   scripts/release.sh                     # interactive menu
#   scripts/release.sh cut                 # cut a release non-interactively
#   scripts/release.sh cut 2026.7.2        # cut an explicit version
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
        cut|status) COMMAND="$arg" ;;
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

# If local branch $1 exists and is a strict ancestor of $2 (i.e. it has no
# unpushed local-only commits), fast-forwards it to $2. Otherwise warns and
# leaves it alone - we never discard local work. Without this, a local main
# left behind by the push-only ff-merge silently diverges from origin the
# moment this script runs.
sync_local_branch() {
    local branch="$1" new_sha="$2"
    if [[ "$DRY_RUN" == "1" ]]; then
        return
    fi
    if git show-ref --verify --quiet "refs/heads/$branch"; then
        if git merge-base --is-ancestor "$branch" "$new_sha"; then
            git branch -f "$branch" "$new_sha"
            echo "Fast-forwarded local $branch to ${new_sha:0:12}."
        else
            echo "warning: local $branch has diverged; not updating it automatically." >&2
            echo "Run 'git fetch origin $branch && git rebase origin/$branch' to reconcile." >&2
        fi
    fi
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
    echo "Latest tag:                      $(git describe --tags --abbrev=0 2>/dev/null || echo '(none)')"
    echo "Auto-computed version for today: $(auto_next_version)"
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
    sync_local_branch "$DEFAULT_BRANCH" "$develop_sha"
    echo "Pushed $DEFAULT_BRANCH and $tag."
    echo "arch-package.yml will build the package and publish the GitHub Release."
    echo
    echo "Done. Released $tag."
}

menu() {
    while true; do
        echo
        echo "thumbgrid release manager"
        echo "  1) Cut release   ($DEV_BRANCH -> $DEFAULT_BRANCH, tag, push)"
        echo "  2) Show status"
        echo "  3) Quit"
        read -rp "Select an option [1-3]: " choice
        case "$choice" in
            1) cmd_cut ;;
            2) show_status ;;
            3) exit 0 ;;
            *) echo "invalid choice" ;;
        esac
    done
}

case "$COMMAND" in
    cut) cmd_cut ;;
    status) show_status ;;
    "") menu ;;
esac
