#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# Guard against two Arch packaging drift risks that nothing else checks:
#
#   1. depends=() drift between packaging/arch/PKGBUILD and the rendered
#      packaging/arch-bin/PKGBUILD.template. thumbgrid-bin repackages the exact
#      binary built by thumbgrid,
#      so their runtime dependencies must stay identical - nothing here
#      rebuilds against different library versions, so any divergence is a
#      bug, not an intentional choice.
#
#   2. An invalid template or an unreplaced release placeholder. The concrete
#      PKGBUILD and .SRCINFO exist only in the AUR clone during publication.
#
# Deliberately does NOT source either PKGBUILD directly:
# packaging/arch/PKGBUILD runs `cd "$startdir/../.."` at parse time and
# reads $THUMBGRID_PKGVER / $THUMBGRID_PKGREL from the environment, so
# sourcing it here would mean reproducing makepkg's own execution context by
# hand. `makepkg --printsrcinfo` already does that safely (and is what
# AUR/CI trust), so both checks below go through it instead of ad hoc
# parsing.
#
# Usage: scripts/check-packaging.sh
#   Run from anywhere; paths are resolved relative to this script. Requires
#   makepkg (part of pacman/base-devel on Arch) and a non-root user - makepkg
#   refuses to run as root.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARCH_DIR="$ROOT_DIR/packaging/arch"
ARCH_BIN_DIR="$ROOT_DIR/packaging/arch-bin"

for tool in makepkg diff; do
    command -v "$tool" >/dev/null 2>&1 || { printf '%s is required but not found.\n' "$tool" >&2; exit 1; }
done

fail=0

# Render deterministic dummy release metadata exactly as publish-aur.sh does.
# This makes the version-neutral template parseable by makepkg without putting
# a stale real release version or checksum in the source tree.
RENDER_DIR="$(mktemp -d)"
trap 'rm -rf "$RENDER_DIR"' EXIT
cp "$ARCH_BIN_DIR/PKGBUILD.template" "$RENDER_DIR/PKGBUILD"
sed -i \
    -e 's/@PKGVER@/0.0.0/g' \
    -e 's/@PKGREL@/1/g' \
    -e 's/@SRCREL@/1/g' \
    -e 's/@SHA256@/0000000000000000000000000000000000000000000000000000000000000000/g' \
    "$RENDER_DIR/PKGBUILD"

# --- Check 1: depends=() drift between the two PKGBUILDs -------------------
#
# packaging/arch/PKGBUILD's pkgver() falls back to `git describe` when
# $THUMBGRID_PKGVER is unset. That has nothing to do with depends=(), but
# pinning it keeps this check deterministic regardless of the working
# tree's tags (e.g. a shallow CI checkout) instead of relying on the
# fallback behaving the same way everywhere.
arch_srcinfo="$(cd "$ARCH_DIR" && THUMBGRID_PKGVER=0 makepkg --printsrcinfo)"
arch_bin_srcinfo="$(cd "$RENDER_DIR" && makepkg --printsrcinfo)"

# Normalized to just the `depends = ...` lines, in file order, so the
# comparison ignores everything else .SRCINFO carries (pkgver, source,
# sha256sums, etc. are expected to differ between the two packages).
arch_depends="$(printf '%s\n' "$arch_srcinfo" | grep -E '^[[:space:]]*depends = ' | sed -E 's/^[[:space:]]+//')"
arch_bin_depends="$(printf '%s\n' "$arch_bin_srcinfo" | grep -E '^[[:space:]]*depends = ' | sed -E 's/^[[:space:]]+//')"

if [[ "$arch_depends" != "$arch_bin_depends" ]]; then
    printf 'packaging/arch and packaging/arch-bin depends=() arrays have drifted.\n' >&2
    printf 'thumbgrid-bin repackages the exact binary built by thumbgrid, so their\n' >&2
    printf 'runtime dependencies must be identical. Diff (arch vs arch-bin):\n\n' >&2
    diff -u <(printf '%s\n' "$arch_depends") <(printf '%s\n' "$arch_bin_depends") >&2 || true
    printf '\n' >&2
    fail=1
else
    printf 'OK: depends=() matches between packaging/arch and packaging/arch-bin.\n'
fi

# --- Check 2: template rendered cleanly ------------------------------------
if grep -q '@[A-Z][A-Z0-9_]*@' "$RENDER_DIR/PKGBUILD"; then
    printf 'packaging/arch-bin/PKGBUILD.template has an unreplaced placeholder.\n' >&2
    fail=1
else
    printf 'OK: PKGBUILD.template renders valid release metadata.\n'
fi

if (( fail )); then
    printf '\nPackaging consistency checks FAILED.\n' >&2
    exit 1
fi

printf '\nPackaging consistency checks passed.\n'
