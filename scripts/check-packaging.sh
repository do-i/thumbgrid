#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# Guard against two Arch packaging drift risks that nothing else checks:
#
#   1. depends=() drift between packaging/arch/PKGBUILD and
#      packaging/arch-bin/PKGBUILD. thumbgrid-bin repackages the exact
#      binary built by thumbgrid (see packaging/arch-bin/PKGBUILD's header),
#      so their runtime dependencies must stay identical - nothing here
#      rebuilds against different library versions, so any divergence is a
#      bug, not an intentional choice.
#
#   2. A stale packaging/arch-bin/.SRCINFO. That file is what AUR actually
#      consumes, generated from PKGBUILD via `makepkg --printsrcinfo`.
#      scripts/publish-aur.sh regenerates it automatically, but a manual
#      PKGBUILD edit can easily forget to, silently shipping stale metadata
#      to AUR.
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

# --- Check 1: depends=() drift between the two PKGBUILDs -------------------
#
# packaging/arch/PKGBUILD's pkgver() falls back to `git describe` when
# $THUMBGRID_PKGVER is unset. That has nothing to do with depends=(), but
# pinning it keeps this check deterministic regardless of the working
# tree's tags (e.g. a shallow CI checkout) instead of relying on the
# fallback behaving the same way everywhere.
arch_srcinfo="$(cd "$ARCH_DIR" && THUMBGRID_PKGVER=0 makepkg --printsrcinfo)"
arch_bin_srcinfo="$(cd "$ARCH_BIN_DIR" && makepkg --printsrcinfo)"

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

# --- Check 2: stale packaging/arch-bin/.SRCINFO -----------------------------
if diff_output="$(diff -u "$ARCH_BIN_DIR/.SRCINFO" <(printf '%s\n' "$arch_bin_srcinfo"))"; then
    printf 'OK: packaging/arch-bin/.SRCINFO matches `makepkg --printsrcinfo`.\n'
else
    printf 'packaging/arch-bin/.SRCINFO is stale: it does not match what\n' >&2
    printf '`makepkg --printsrcinfo` generates from packaging/arch-bin/PKGBUILD.\n' >&2
    printf 'Regenerate it:\n' >&2
    printf '  (cd packaging/arch-bin && makepkg --printsrcinfo > .SRCINFO)\n\n' >&2
    printf '%s\n' "$diff_output" >&2
    fail=1
fi

if (( fail )); then
    printf '\nPackaging consistency checks FAILED.\n' >&2
    exit 1
fi

printf '\nPackaging consistency checks passed.\n'
