#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# Performance gate: compares the current tree with a FIXED reference commit
# (default 25b03553, the last commit before the optimisation/refactor work),
# measuring both builds interleaved (R, C, R, C, ...) so machine drift hits both
# sides equally. Absolute timings on a dev machine drift ~12% within an hour;
# the reference-relative delta does not. Because the reference is fixed at the
# pre-programme commit, the gate bounds the CUMULATIVE regression of the whole
# programme, not just of the latest commit.
#
# Usage:
#   scripts/perf-baseline.sh               # measure both builds, print medians only
#   scripts/perf-baseline.sh --compare     # table vs reference, exit 1 on FAIL
#
# Options:
#   --ref COMMIT  reference commit (default 25b03553); its Release build lives in
#                 temp/perf/ref/ (git worktree + build dir) and is rebuilt only
#                 when the commit changes (stamp file temp/perf/ref/built-commit)
#   --runs N      pairs per measurement; overrides the defaults (app 15, scroll 15).
#                 One warm-up run of each side is made first and discarded
#   --no-build    skip building (current and reference)
#   --help        this text
#
# Environment:
#   PERF_BUILD_DIR   Release build dir of the current tree (default: <repo>/build-perf)
#
# Metrics (startup, folder_open and rss come from the same app launch):
#   startup_ms      "startup_paint": process start to first paint, app on the fixture
#   folder_open_ms  dir_load + populate, both for the fixture folder (n >= 2000)
#   rss_kb          peak RSS of the app (getrusage max RSS of timeout's child, the app)
#   scroll_ms       driver (thumbgrid_perf_scroll_driver): scrollbar stepped through
#                   its full range in 120 px steps, events processed after each
# Statistic per side: startup and folder_open = mean of the fastest 80% of runs (drops
# rare scheduling-stall outliers); rss and scroll = median.
# delta% = (current - reference) / reference.
#
# Limits (current worse than reference by more than): startup 5%, rss 3%,
# folder_open 5%, scroll 5%. Faster or smaller always passes.
#
# Fixture: temp/perf/fixture, 2000 small JPEGs + 200 subfolders, generated once
# (ImageMagick, 8 distinct images copied under many names) and then reused.
# Every run uses a fresh empty HOME, so every run has a cold thumbnail cache.
# App stderr goes to a file (a pipe loses lines when timeout kills the app).
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PERF_BUILD_DIR:-"$ROOT_DIR/build-perf"}"
PERF_DIR="$ROOT_DIR/temp/perf"
FIXTURE="$PERF_DIR/fixture"
SCRATCH="$PERF_DIR/scratch"
REF_DIR="$PERF_DIR/ref"
REF_SRC="$REF_DIR/src"
REF_BUILD="$REF_DIR/build"
REF_STAMP="$REF_DIR/built-commit"
REF_COMMIT=25b03553
APP_TIMEOUT=6      # seconds the app runs; the fixture loads in well under 2 s
DRIVER_TIMEOUT=60
METRICS=(startup_ms folder_open_ms rss_kb scroll_ms)
declare -A LIMIT=([startup_ms]=5 [folder_open_ms]=5 [rss_kb]=3 [scroll_ms]=5)

RUNS=""
DO_BUILD=1
MODE=measure

die() { echo "perf-baseline: $*" >&2; exit 2; }

usage() { sed -n '2,/^set -uo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//' | sed '1,3d'; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --compare) MODE=compare ;;
        --no-build) DO_BUILD=0 ;;
        --ref) shift; REF_COMMIT="${1:-}"; [[ -n "$REF_COMMIT" ]] || die "--ref needs a commit" ;;
        --runs) shift; RUNS="${1:-}"; [[ "$RUNS" =~ ^[1-9][0-9]*$ ]] || die "--runs needs a positive integer" ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1 (try --help)" ;;
    esac
    shift
done
APP_RUNS="${RUNS:-15}"
SCROLL_RUNS="${RUNS:-15}"

command -v python3 >/dev/null || die "python3 is needed (peak RSS via getrusage)"
REF_FULL="$(git -C "$ROOT_DIR" rev-parse --verify --quiet "$REF_COMMIT^{commit}")" || die "unknown reference commit $REF_COMMIT"
mkdir -p "$PERF_DIR" "$SCRATCH"
TMP_ROOT="$(mktemp -d -p "$SCRATCH")"
trap 'rm -rf "$TMP_ROOT"' EXIT

# --- build ---------------------------------------------------------------
# PLAYER_MPV_USE_WID mirrors the dev build (software-GL machine). Both builds use
# identical flags. Always --parallel 4, never unbounded.
build_tree() { # src build_dir
    if [[ ! -f "$2/CMakeCache.txt" ]]; then
        cmake -S "$1" -B "$2" -DCMAKE_BUILD_TYPE=Release \
            -DPLAYER_MPV_USE_WID=ON -DBUILD_TESTING=ON >/dev/null || die "configure failed ($1)"
    fi
    cmake --build "$2" --parallel 4 --target thumbgrid thumbgrid_perf_scroll_driver \
        >"$TMP_ROOT/build.log" 2>&1 || { tail -30 "$TMP_ROOT/build.log" >&2; die "build failed ($1)"; }
}

# The reference is a detached git worktree (works from a linked worktree too, and
# git-lfs smudges the PNGs/JPEGs, which `git archive` would not).
ensure_reference() {
    local stamp=""
    [[ -f "$REF_STAMP" ]] && stamp="$(cat "$REF_STAMP")"
    [[ "$stamp" == "$REF_FULL" && -x "$REF_BUILD/src/thumbgrid" && -x "$REF_BUILD/src/tests/thumbgrid_perf_scroll_driver" ]] && return 0
    echo "building reference $REF_COMMIT in $REF_DIR (one-off) ..." >&2
    mkdir -p "$REF_DIR"
    if [[ -e "$REF_SRC/.git" ]]; then
        git -C "$REF_SRC" checkout --quiet --detach "$REF_FULL" || die "cannot check out $REF_FULL"
    else
        git -C "$ROOT_DIR" worktree prune
        git -C "$ROOT_DIR" worktree add --quiet --detach "$REF_SRC" "$REF_FULL" || die "cannot create reference worktree"
    fi
    local png
    png="$(find "$REF_SRC" -name '*.png' -not -path '*/.git/*' | head -1)"
    if [[ -n "$png" ]] && ! file "$png" | grep -q 'PNG image data'; then
        git -C "$REF_SRC" lfs checkout >/dev/null 2>&1
        file "$png" | grep -q 'PNG image data' || die "reference has LFS pointer files ($png)"
    fi
    rm -f "$REF_STAMP"
    build_tree "$REF_SRC" "$REF_BUILD"
    echo "$REF_FULL" >"$REF_STAMP"
}

if [[ $DO_BUILD -eq 1 ]]; then
    ensure_reference
    build_tree "$ROOT_DIR" "$BUILD_DIR"
fi
CUR_APP="$BUILD_DIR/src/thumbgrid"
CUR_DRIVER="$BUILD_DIR/src/tests/thumbgrid_perf_scroll_driver"
REF_APP="$REF_BUILD/src/thumbgrid"
REF_DRIVER="$REF_BUILD/src/tests/thumbgrid_perf_scroll_driver"
for f in "$CUR_APP" "$CUR_DRIVER" "$REF_APP" "$REF_DRIVER"; do
    [[ -x "$f" ]] || die "missing $f (run without --no-build)"
done
[[ "$(cat "$REF_STAMP" 2>/dev/null)" == "$REF_FULL" ]] || die "reference build is not $REF_COMMIT (run without --no-build)"

# --- fixture ---------------------------------------------------------------
make_fixture() {
    command -v magick >/dev/null || die "ImageMagick (magick) is needed to generate the fixture"
    local tmp="$FIXTURE.tmp" i
    rm -rf "$tmp"; mkdir -p "$tmp/.seed"
    for i in 0 1 2 3 4 5 6 7; do
        magick -seed "$((i + 1))" -size 160x120 plasma:fractal -quality 80 "$tmp/.seed/$i.jpg"
    done
    for i in $(seq 1 2000); do
        cp "$tmp/.seed/$((i % 8)).jpg" "$(printf '%s/img_%04d.jpg' "$tmp" "$i")"
    done
    for i in $(seq 1 200); do mkdir "$(printf '%s/dir_%03d' "$tmp" "$i")"; done
    rm -rf "$tmp/.seed"
    touch -d '2026-01-01 00:00:00' "$tmp"/img_*.jpg
    mv "$tmp" "$FIXTURE"
}
[[ -d "$FIXTURE" ]] || { echo "generating fixture in $FIXTURE ..." >&2; make_fixture; }
[[ "$(find "$FIXTURE" -maxdepth 1 -name '*.jpg' | wc -l)" -eq 2000 ]] || die "fixture is damaged; delete $FIXTURE"

# --- measuring -------------------------------------------------------------
# /usr/bin/time is not installed here: run the command and write the largest
# peak RSS (kB) among its waited-for descendants (the app) to file $1.
peak_rss() {
    python3 -I -c 'import resource,subprocess,sys
subprocess.run(sys.argv[2:])
open(sys.argv[1],"w").write(str(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss))' "$@"
}

# Runs the app once on the fixture; prints "startup folder_open rss".
# folder_open = dir_load + populate lines with n >= 2000: the fixture load
# produces one dir_load and (after the first empty populate) one full populate;
# small populates (n < 2000) are intermediate states and are ignored.
one_run() { # app binary
    local app="$1" home out tf
    home="$(mktemp -d -p "$TMP_ROOT")"; out="$home/out.txt"; tf="$home/time.txt"
    HOME="$home" QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 \
        QT_LOGGING_RULES="thumbgrid.perf.debug=true" \
        peak_rss "$tf" timeout "$APP_TIMEOUT" "$app" "$FIXTURE" >/dev/null 2>"$out"
    local startup load pop rss
    startup="$(sed -n 's/.*perf startup_paint=\([0-9.]*\).*/\1/p' "$out" | head -1)"
    load="$(sed -n 's/.*perf dir_load=\([0-9.]*\) n=\([0-9]*\).*/\1 \2/p' "$out" | awk '$2>=2000{print $1; exit}')"
    pop="$(sed -n 's/.*perf populate=\([0-9.]*\) n=\([0-9]*\).*/\1 \2/p' "$out" | awk '$2>=2000{print $1; exit}')"
    rss="$(cat "$tf")"
    if [[ -z "$startup" || -z "$load" || -z "$pop" || -z "$rss" ]]; then
        echo "perf-baseline: incomplete perf output (startup='$startup' dir_load='$load' populate='$pop' rss='$rss')" >&2
        sed 's/^/  | /' "$out" | head -20 >&2
        return 1
    fi
    if (( rss < 20000 || rss > 2000000 )); then
        echo "perf-baseline: implausible RSS $rss kB (measuring timeout instead of the app?)" >&2
        return 1
    fi
    awk -v s="$startup" -v l="$load" -v p="$pop" -v r="$rss" 'BEGIN{printf "%s %.3f %s\n", s, l+p, r}'
    rm -rf "$home"
}

scroll_run() { # driver binary
    local driver="$1" home out
    home="$(mktemp -d -p "$TMP_ROOT")"
    out="$(HOME="$home" QT_QPA_PLATFORM=offscreen THUMBGRID_PERF_FIXTURE="$FIXTURE" \
        timeout "$DRIVER_TIMEOUT" "$driver" 2>&1)"
    local v
    v="$(sed -n 's/.*scroll_pass_ms=\([0-9.]*\) steps=\([0-9]*\).*/\1/p' <<<"$out" | head -1)"
    if [[ -z "$v" ]]; then
        echo "perf-baseline: scroll driver printed no result" >&2
        echo "$out" | tail -15 >&2
        return 1
    fi
    echo "$v"
    rm -rf "$home"
}

# mean of the fastest 80% (slowest fifth dropped): startup and folder_open have rare
# slow outliers (scheduling stalls, 200-600 ms); this is steadier run-to-run than the
# median or the median of the fastest half (measured over three 15-pair runs).
trimmed() { sort -g | awk '{a[NR]=$1} END{n=int(NR*0.8+0.5); if(n<1)n=1; for(i=1;i<=n;i++)s+=a[i]; printf "%.3f\n", s/n}'; }
median() { sort -g | awk '{a[NR]=$1} END{ if(NR%2) print a[(NR+1)/2]; else printf "%.3f\n", (a[NR/2]+a[NR/2+1])/2 }'; }

# median of the fastest half
best() { sort -g | awk '{a[NR]=$1} END{n=int((NR+1)/2); if(n%2) print a[(n+1)/2]; else printf "%.3f\n", (a[n/2]+a[n/2+1])/2}'; }
median() { sort -g | awk '{a[NR]=$1} END{ if(NR%2) print a[(NR+1)/2]; else printf "%.3f\n", (a[NR/2]+a[NR/2+1])/2 }'; }

measure() {
    local i line s f r v side
    declare -g -A SERIES=()   # "<side>:<metric>" -> newline-separated values
    echo "warm-up (reference, current) ..." >&2
    for side in R C; do
        if [[ $side == R ]]; then one_run "$REF_APP" >/dev/null || exit 2; else one_run "$CUR_APP" >/dev/null || exit 2; fi
    done
    for ((i = 1; i <= APP_RUNS; i++)); do
        for side in R C; do
            if [[ $side == R ]]; then line="$(one_run "$REF_APP")" || exit 2; else line="$(one_run "$CUR_APP")" || exit 2; fi
            read -r s f r <<<"$line"
            SERIES[$side:startup_ms]+="$s"$'\n'; SERIES[$side:folder_open_ms]+="$f"$'\n'; SERIES[$side:rss_kb]+="$r"$'\n'
            echo "app $i/$APP_RUNS $side: startup=$s folder_open=$f rss=$r" >&2
        done
    done
    for side in R C; do
        if [[ $side == R ]]; then scroll_run "$REF_DRIVER" >/dev/null || exit 2; else scroll_run "$CUR_DRIVER" >/dev/null || exit 2; fi
    done
    for ((i = 1; i <= SCROLL_RUNS; i++)); do
        for side in R C; do
            if [[ $side == R ]]; then v="$(scroll_run "$REF_DRIVER")" || exit 2; else v="$(scroll_run "$CUR_DRIVER")" || exit 2; fi
            SERIES[$side:scroll_ms]+="$v"$'\n'
            echo "scroll $i/$SCROLL_RUNS $side: scroll=$v" >&2
        done
    done
    declare -g -A VAL
    local m stat
    for side in R C; do
        for m in "${METRICS[@]}"; do
            case $m in startup_ms|folder_open_ms) stat=trimmed ;; *) stat=median ;; esac
            VAL[$side:$m]="$(printf '%s' "${SERIES[$side:$m]}" | $stat)"
        done
    done
    { for k in "${!SERIES[@]}"; do echo "$k: $(echo ${SERIES[$k]})"; done | sort; } >"$PERF_DIR/last-run.txt"
}

measure

echo
echo "reference: $REF_COMMIT  (raw samples: temp/perf/last-run.txt)"
if [[ $MODE == compare ]]; then
    printf '%-16s %12s %12s %9s %7s  %s\n' metric reference current delta% limit result
else
    printf '%-16s %12s %12s %9s\n' metric reference current delta%
fi
status=0
for m in "${METRICS[@]}"; do
    read -r delta res <<<"$(awk -v b="${VAL[R:$m]}" -v c="${VAL[C:$m]}" -v l="${LIMIT[$m]}" \
        'BEGIN{d=(c-b)/b*100; printf "%.1f %s\n", d, (d>l)?"FAIL":"PASS"}')"
    if [[ $MODE == compare ]]; then
        [[ $res == FAIL ]] && status=1
        printf '%-16s %12s %12s %+8.1f%% %6s%%  %s\n' "$m" "${VAL[R:$m]}" "${VAL[C:$m]}" "$delta" "${LIMIT[$m]}" "$res"
    else
        printf '%-16s %12s %12s %+8.1f%%\n' "$m" "${VAL[R:$m]}" "${VAL[C:$m]}" "$delta"
    fi
done
exit $status
