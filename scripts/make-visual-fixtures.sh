#!/usr/bin/env bash
# Generates the fixtures the visual verification checklist needs but that no
# repo can reasonably carry: a folder large enough to make the grid scroll, and
# a deliberately broken video. Both are throwaway - regenerate rather than back
# up. Nothing is written outside the target directory.
#
#   scripts/make-visual-fixtures.sh [target-dir]
#
# Default target: ~/thumbgrid-fixtures
set -euo pipefail

TARGET="${1:-$HOME/thumbgrid-fixtures}"
IMAGE_COUNT="${IMAGE_COUNT:-500}"

for tool in ffmpeg; do
    command -v "$tool" >/dev/null || { echo "error: $tool is required" >&2; exit 1; }
done

# Refuse to touch a directory we did not create: the marker file is written on
# the way out, so a pre-existing directory without it is somebody else's.
if [[ -e "$TARGET" && ! -e "$TARGET/.thumbgrid-fixtures" ]]; then
    echo "error: $TARGET exists and was not created by this script; remove it or pick another path" >&2
    exit 1
fi

mkdir -p "$TARGET"

# ---------------------------------------------------------------- many images
# Checks the scrollbar-appearance jump: the grid relayouts when the scrollbar
# first appears, so the folder has to overflow the viewport by a wide margin.
# testsrc paints a frame counter into every frame, which is what makes the
# scroll position readable at a glance - a folder of identical tiles would not.
MANY="$TARGET/many-images"
echo "generating $IMAGE_COUNT images in $MANY"
rm -rf "$MANY"
mkdir -p "$MANY"
ffmpeg -loglevel error -y \
    -f lavfi -i "testsrc=size=320x240:rate=1:duration=$IMAGE_COUNT" \
    -f image2 "$MANY/img_%04d.png"

# ------------------------------------------------------------- broken video
# The checklist item is "placeholder, no hang". A file that is merely empty
# exercises a different path (nothing to demux at all), so generate both: a
# valid-header-then-cut file, and a zero-byte one.
BROKEN="$TARGET/broken-video"
echo "generating broken video fixtures in $BROKEN"
rm -rf "$BROKEN"
mkdir -p "$BROKEN"

# Reference clip. The audio is deliberately soft and quiet - a low-passed
# arpeggio at low gain rather than a full-scale sine, so leaving playback
# unmuted during a visual check is not painful.
ffmpeg -loglevel error -y \
    -f lavfi -i "testsrc=size=640x480:rate=25:duration=10" \
    -f lavfi -i "sine=frequency=392:duration=10,tremolo=f=0.6:d=0.4,lowpass=f=1200,volume=0.06" \
    -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest \
    "$BROKEN/playable.mp4"

# Truncate to a third: the moov atom written by the muxer at the end is lost,
# so the file looks like a video and then stops making sense mid-stream.
FULL_SIZE=$(stat -c%s "$BROKEN/playable.mp4")
head -c $((FULL_SIZE / 3)) "$BROKEN/playable.mp4" > "$BROKEN/truncated.mp4"

: > "$BROKEN/zero-bytes.mp4"

# Garbage with a video extension - neither a header nor empty.
head -c 200000 /dev/urandom > "$BROKEN/not-really-a-video.mp4"

date -u +"generated %Y-%m-%dT%H:%M:%SZ" > "$TARGET/.thumbgrid-fixtures"

echo
echo "done. fixtures in $TARGET"
echo "  many-images/    $(find "$MANY" -name '*.png' | wc -l) images - open this folder and check the"
echo "                  grid does not jump when the scrollbar first appears"
echo "  broken-video/   playable.mp4 (reference), truncated.mp4, zero-bytes.mp4,"
echo "                  not-really-a-video.mp4 - each must show a placeholder, not hang"
