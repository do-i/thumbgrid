#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# Performance gate: measures startup, folder open, peak memory and scroll cost
# on a fixed fixture and compares them with a recorded baseline.
#
# Usage:
#   scripts/perf-baseline.sh               # measure and print only
#   scripts/perf-baseline.sh --record      # measure, write temp/perf/baseline.json
#   scripts/perf-baseline.sh --compare     # measure, table vs baseline, exit 1 on FAIL
#
# Options:
#   --runs N      runs per metric, median is used (default 7); one extra
#                 warm-up run is made first and discarded
#   --no-build    skip the incremental Release build
#   --help        this text
#
# Environment:
#   PERF_BUILD_DIR   Release build dir (default: <repo>/build-perf, git-ignored)
#
# Metrics over N runs (median of the fastest half for startup and folder_open, which are
# bimodal under scheduling noise; median for rss and scroll):
#   startup_ms      "startup_paint": process start to first paint, app on the fixture
#   folder_open_ms  dir_load + populate, both for the fixture folder (n >= 2000)
#   rss_kb          peak RSS of the app (getrusage max RSS of timeout's child, the app)
#   scroll_ms       driver (thumbgrid_perf_scroll_driver): scrollbar stepped through
#                   its full range in 120 px steps, events processed after each
#
# Tolerances (worse than baseline by more than): startup 5%, rss 3%,
# folder_open 5%, scroll 5%. Faster or smaller always passes.
#
# Fixture: temp/perf/fixture, 2000 small JPEGs + 200 subfolders, generated once
# (ImageMagick, 8 distinct images copied under many names) and then reused.
# Every run uses a fresh empty HOME, so every run has a cold thumbnail cache.
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PERF_BUILD_DIR:-"$ROOT_DIR/build-perf"}"
PERF_DIR="$ROOT_DIR/temp/perf"
FIXTURE="$PERF_DIR/fixture"
BASELINE="$PERF_DIR/baseline.json"
SCRATCH="$PERF_DIR/scratch"
APP="$BUILD_DIR/src/thumbgrid"
DRIVER="$BUILD_DIR/src/tests/thumbgrid_perf_scroll_driver"
APP_TIMEOUT=6      # seconds the app runs; the fixture loads in well under 2 s
DRIVER_TIMEOUT=60
METRICS=(startup_ms folder_open_ms rss_kb scroll_ms)
declare -A LIMIT=([startup_ms]=5 [folder_open_ms]=5 [rss_kb]=3 [scroll_ms]=5)

RUNS=7
DO_BUILD=1
MODE=measure

die() { echo "perf-baseline: $*" >&2; exit 2; }

usage() { sed -n '2,/^set -uo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//' | sed '1,3d'; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --record) MODE=record ;;
        --compare) MODE=compare ;;
        --no-build) DO_BUILD=0 ;;
        --runs) shift; RUNS="${1:-}"; [[ "$RUNS" =~ ^[1-9][0-9]*$ ]] || die "--runs needs a positive integer" ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1 (try --help)" ;;
    esac
    shift
done

command -v python3 >/dev/null || die "python3 is needed (peak RSS via getrusage)"
mkdir -p "$PERF_DIR" "$SCRATCH"
TMP_ROOT="$(mktemp -d -p "$SCRATCH")"
trap 'rm -rf "$TMP_ROOT"' EXIT

# --- build ---------------------------------------------------------------
if [[ $DO_BUILD -eq 1 ]]; then
    if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        # PLAYER_MPV_USE_WID mirrors the dev build (software-GL machine).
        cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
            -DPLAYER_MPV_USE_WID=ON -DBUILD_TESTING=ON >/dev/null || die "configure failed"
    fi
    cmake --build "$BUILD_DIR" --parallel 4 --target thumbgrid thumbgrid_perf_scroll_driver \
        >"$TMP_ROOT/build.log" 2>&1 || { tail -30 "$TMP_ROOT/build.log" >&2; die "build failed"; }
fi
[[ -x "$APP" ]] || die "missing $APP (run without --no-build)"
[[ -x "$DRIVER" ]] || die "missing $DRIVER (run without --no-build)"
BUILD_TYPE="$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$BUILD_DIR/CMakeCache.txt")"

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
one_run() {
    local home out tf
    home="$(mktemp -d -p "$TMP_ROOT")"; out="$home/out.txt"; tf="$home/time.txt"
    HOME="$home" QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 \
        QT_LOGGING_RULES="thumbgrid.perf.debug=true" \
        peak_rss "$tf" timeout "$APP_TIMEOUT" "$APP" "$FIXTURE" >/dev/null 2>"$out"
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

scroll_run() {
    local home out
    home="$(mktemp -d -p "$TMP_ROOT")"
    out="$(HOME="$home" QT_QPA_PLATFORM=offscreen THUMBGRID_PERF_FIXTURE="$FIXTURE" \
        timeout "$DRIVER_TIMEOUT" "$DRIVER" 2>&1)"
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

# median of the fastest half: ignores slow-mode outliers and lucky-fast ones
best() { sort -g | awk '{a[NR]=$1} END{n=int((NR+1)/2); if(n%2) print a[(n+1)/2]; else printf "%.3f\n", (a[n/2]+a[n/2+1])/2}'; }
median() { sort -g | awk '{a[NR]=$1} END{ if(NR%2) print a[(NR+1)/2]; else printf "%.3f\n", (a[NR/2]+a[NR/2+1])/2 }'; }

measure() {
    local i line
    local -a st=() fo=() rs=() sc=()
    echo "warm-up run ..." >&2
    one_run >/dev/null || exit 2
    for ((i = 1; i <= RUNS; i++)); do
        line="$(one_run)" || exit 2
        read -r s f r <<<"$line"
        st+=("$s"); fo+=("$f"); rs+=("$r")
        v="$(scroll_run)" || exit 2
        sc+=("$v")
        echo "run $i/$RUNS: startup=$s folder_open=$f rss=$r scroll=$v" >&2
    done
    declare -g -A VAL
    # startup and folder open are bimodal (about 125 vs 195 ms, a scheduling
    # effect), which makes a median flip between modes; the median of the fastest half is stable.
    VAL[startup_ms]="$(printf '%s\n' "${st[@]}" | best)"
    VAL[folder_open_ms]="$(printf '%s\n' "${fo[@]}" | best)"
    VAL[rss_kb]="$(printf '%s\n' "${rs[@]}" | median)"
    VAL[scroll_ms]="$(printf '%s\n' "${sc[@]}" | median)"
}

measure

echo
printf '%-16s %12s\n' metric median
for m in "${METRICS[@]}"; do printf '%-16s %12s\n' "$m" "${VAL[$m]}"; done

# --- record / compare --------------------------------------------------------
if [[ $MODE == record ]]; then
    {
        printf '{\n'
        for m in "${METRICS[@]}"; do printf '  "%s": %s,\n' "$m" "${VAL[$m]}"; done
        printf '  "runs": %s,\n  "commit": "%s",\n  "date": "%s",\n  "build_type": "%s"\n}\n' \
            "$RUNS" "$(git -C "$ROOT_DIR" rev-parse --short HEAD)" "$(date -Iseconds)" "$BUILD_TYPE"
    } >"$BASELINE"
    echo "baseline written to $BASELINE"
elif [[ $MODE == compare ]]; then
    [[ -f "$BASELINE" ]] || die "no baseline; run with --record first"
    echo
    printf '%-16s %12s %12s %9s %7s  %s\n' metric baseline current delta% limit result
    status=0
    for m in "${METRICS[@]}"; do
        base="$(sed -n "s/.*\"$m\": *\([0-9.]*\),*/\1/p" "$BASELINE")"
        [[ -n "$base" ]] || die "baseline lacks $m"
        read -r delta res <<<"$(awk -v b="$base" -v c="${VAL[$m]}" -v l="${LIMIT[$m]}" \
            'BEGIN{d=(c-b)/b*100; printf "%.1f %s\n", d, (d>l)?"FAIL":"PASS"}')"
        [[ $res == FAIL ]] && status=1
        printf '%-16s %12s %12s %+8.1f%% %6s%%  %s\n' "$m" "$base" "${VAL[$m]}" "$delta" "${LIMIT[$m]}" "$res"
    done
    exit $status
fi
