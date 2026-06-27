#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# Migrate the old custom theme / color store to the new consolidated scheme.
#
# Old store (pre "Overhaul theme and custom color store"):
#   - base colors lived in   theme.conf      [Colors]
#   - folder-grid colors in  thumbgrid.conf  as folderView*Color keys
#     (stored as Qt @Variant(...) QColor blobs)
#   - the custom theme id (tid) was 5 (COLORS_CUSTOMIZED) or, in one interim
#     build, 6 (COLORS_CUSTOM).
#
# New store:
#   - everything lives in    theme.conf      [Colors]
#   - folder-grid colors are plain #rrggbb under short keys:
#       folderViewLabelBackgroundColor         -> fv_label_bg
#       folderViewSelectionColor               -> fv_selection
#       folderViewParentIconColor              -> fv_parent_icon
#       folderViewSelectedLabelBackgroundColor -> fv_sel_label_bg
#       folderViewCellBackgroundColor          -> fv_cell_bg
#   - the custom theme id is 5 (COLORS_CUSTOM); 6 is normalized to 5.
#
# The migration is non-destructive (backs both files up first), idempotent
# (running it again is a no-op), and never overwrites a value that already
# exists in theme.conf. Pass --dry-run to preview without writing.

set -euo pipefail

DRY_RUN=0
for arg in "$@"; do
    case "$arg" in
        -n|--dry-run) DRY_RUN=1 ;;
        -h|--help)
            printf 'Usage: %s [--dry-run]\n' "$(basename "$0")"
            printf 'Migrate old custom theme/colors into the new theme.conf scheme.\n'
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$arg" >&2
            exit 2
            ;;
    esac
done

if ! command -v python3 >/dev/null 2>&1; then
    printf 'python3 is required for the theme migration but was not found.\n' >&2
    exit 1
fi

# QSettings NativeFormat on Linux/FreeBSD stores under $XDG_CONFIG_HOME (or
# ~/.config) using the organization name "thumbgrid".
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/thumbgrid"

if [[ ! -d "$CONFIG_DIR" ]]; then
    printf 'No thumbgrid config directory found at %s\n' "$CONFIG_DIR"
    printf 'Nothing to migrate.\n'
    exit 0
fi

TG_CONFIG_DIR="$CONFIG_DIR" TG_DRY_RUN="$DRY_RUN" python3 - <<'PY'
import os
import re
import shutil
import sys

config_dir = os.environ["TG_CONFIG_DIR"]
dry_run = os.environ.get("TG_DRY_RUN") == "1"

theme_path = os.path.join(config_dir, "theme.conf")
main_path = os.path.join(config_dir, "thumbgrid.conf")

# old thumbgrid.conf key -> new theme.conf [Colors] key
KEY_MAP = [
    ("folderViewLabelBackgroundColor",         "fv_label_bg"),
    ("folderViewSelectionColor",               "fv_selection"),
    ("folderViewParentIconColor",              "fv_parent_icon"),
    ("folderViewSelectedLabelBackgroundColor", "fv_sel_label_bg"),
    ("folderViewCellBackgroundColor",          "fv_cell_bg"),
]

# ColorSchemes enum values in the new scheme.
COLORS_CUSTOM = 5


def ini_unescape(s):
    """Decode a Qt QSettings INI-escaped byte string back to raw bytes."""
    out = bytearray()
    i, n = 0, len(s)
    simple = {
        "a": 7, "b": 8, "f": 12, "n": 10, "r": 13, "t": 9, "v": 11,
        '"': 0x22, "?": 0x3F, "'": 0x27, "\\": 0x5C,
    }
    while i < n:
        c = s[i]
        if c != "\\":
            out.append(ord(c) & 0xFF)
            i += 1
            continue
        i += 1
        if i >= n:
            break
        e = s[i]
        if e in simple:
            out.append(simple[e]); i += 1
        elif e == "x":
            i += 1
            h = ""
            while i < n and s[i] in "0123456789abcdefABCDEF":
                h += s[i]; i += 1
            if h:
                out.append(int(h, 16) & 0xFF)
        elif e in "01234567":
            o = ""
            while i < n and len(o) < 3 and s[i] in "01234567":
                o += s[i]; i += 1
            out.append(int(o, 8) & 0xFF)
        else:
            out.append(ord(e) & 0xFF); i += 1
    return bytes(out)


def to8(v16):
    # Matches Qt's QColor::red()/green()/blue(): round(channel16 / 257).
    return max(0, min(255, int(round(v16 / 257.0))))


def decode_color(raw):
    """Turn a stored thumbgrid.conf color value into '#rrggbb', or None."""
    raw = raw.strip()
    if raw.startswith('"') and raw.endswith('"'):
        raw = raw[1:-1]
    m = re.match(r"^@Variant\((.*)\)$", raw, re.DOTALL)
    if m:
        data = ini_unescape(m.group(1))
        if len(data) < 13:
            return None
        type_id = int.from_bytes(data[0:4], "big")
        if type_id != 0x43:  # QMetaType::QColor
            return None
        # data[4] = color spec (1 == Rgb); data[5:7]=alpha, then r,g,b as u16 BE
        r = int.from_bytes(data[7:9], "big")
        g = int.from_bytes(data[9:11], "big")
        b = int.from_bytes(data[11:13], "big")
        return "#%02x%02x%02x" % (to8(r), to8(g), to8(b))
    if re.match(r"^#[0-9a-fA-F]{6}$", raw):
        return raw.lower()
    return None


def parse_ini(path):
    """Parse into {section: {key: value}} preserving nothing but key/values."""
    sections = {}
    cur = None
    if not os.path.exists(path):
        return sections
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith(";"):
                continue
            m = re.match(r"^\[(.+)\]$", s)
            if m:
                cur = m.group(1)
                sections.setdefault(cur, {})
                continue
            if cur is not None and "=" in s:
                k, v = s.split("=", 1)
                sections[cur][k.strip()] = v.strip()
    return sections


def write_ini(path, sections):
    lines = []
    for sec in sorted(sections):
        lines.append("[%s]" % sec)
        for k in sorted(sections[sec]):
            lines.append("%s=%s" % (k, sections[sec][k]))
        lines.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines).rstrip("\n") + "\n")


def read_raw_value(path, key):
    if not os.path.exists(path):
        return None
    pat = re.compile(r"^%s\s*=(.*)$" % re.escape(key))
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            m = pat.match(line.rstrip("\n"))
            if m:
                return m.group(1).strip()
    return None


def strip_keys_from_file(path, keys):
    """Remove the given top-level keys, preserving every other line verbatim."""
    pat = re.compile(r"^(%s)\s*=" % "|".join(re.escape(k) for k in keys))
    out = []
    removed = 0
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            if pat.match(line):
                removed += 1
                continue
            out.append(line)
    return "".join(out), removed


theme = parse_ini(theme_path)
colors = theme.setdefault("Colors", {})

migrated = []        # (new_key, hex) written into theme.conf
already = []         # new_key already present in theme.conf
removable_keys = []  # old keys safe to drop from thumbgrid.conf

for old_key, new_key in KEY_MAP:
    raw = read_raw_value(main_path, old_key)
    if raw is None:
        continue
    if new_key in colors:
        # theme.conf is already canonical; the old key is now redundant.
        already.append(new_key)
        removable_keys.append(old_key)
        continue
    hexv = decode_color(raw)
    if hexv is None:
        print("  ! could not decode %s (left in place): %s" % (old_key, raw))
        continue
    colors[new_key] = hexv
    migrated.append((new_key, hexv))
    removable_keys.append(old_key)

# Normalize the custom theme id (5/6 -> COLORS_CUSTOM).
tid_changed = False
if "tid" in colors:
    try:
        tid = int(colors["tid"])
    except ValueError:
        tid = None
    if tid in (5, 6) and tid != COLORS_CUSTOM:
        colors["tid"] = str(COLORS_CUSTOM)
        tid_changed = True

theme_dirty = bool(migrated) or tid_changed
main_dirty = bool(removable_keys) and os.path.exists(main_path)

if not theme_dirty and not main_dirty:
    print("Theme store already up to date. Nothing to migrate.")
    sys.exit(0)

print("Migration plan:")
for new_key, hexv in migrated:
    print("  + theme.conf [Colors] %-15s = %s" % (new_key, hexv))
for new_key in already:
    print("  = theme.conf [Colors] %-15s already set (keeping)" % new_key)
if tid_changed:
    print("  ~ theme.conf [Colors] tid             -> %d (Custom)" % COLORS_CUSTOM)
if removable_keys:
    print("  - thumbgrid.conf: drop %d migrated key(s)" % len(removable_keys))

if dry_run:
    print("\nDry run: no files changed.")
    sys.exit(0)

if theme_dirty:
    if os.path.exists(theme_path):
        shutil.copy2(theme_path, theme_path + ".bak")
    write_ini(theme_path, theme)

if main_dirty:
    new_text, removed = strip_keys_from_file(main_path, removable_keys)
    if removed:
        shutil.copy2(main_path, main_path + ".bak")
        with open(main_path, "w", encoding="utf-8") as f:
            f.write(new_text)

print("\nMigration complete. Backups written as *.conf.bak")
PY
