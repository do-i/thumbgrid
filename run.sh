#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
CMAKE_BIN="${CMAKE_BIN:-cmake}"
APP_ARGS=("$@")

if command -v nproc >/dev/null 2>&1; then
    JOBS="${JOBS:-$(nproc)}"
else
    JOBS="${JOBS:-4}"
fi

print_header() {
    printf '\nthumbgrid build menu\n'
    printf 'Source: %s\n' "$ROOT_DIR"
    printf 'Build:  %s\n' "$BUILD_DIR"
    printf 'Type:   %s\n\n' "$BUILD_TYPE"
}

run_as_root() {
    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        printf 'This action needs root privileges, but sudo was not found.\n' >&2
        printf 'Run this script as root or install the listed packages manually.\n' >&2
        return 1
    fi
}

confirm_install() {
    local package_manager="$1"
    shift

    printf 'This installs dependencies for the full thumbgrid build.\n'
    printf 'Includes feature deps for Exiv2, OpenCV, and mpv.\n\n'
    printf 'Package manager: %s\n' "$package_manager"
    printf 'Packages:\n'
    printf '  %s\n' "$@"
    printf '\n'

    read -r -p "Install these packages now? [y/N] " answer
    case "$answer" in
        [yY]|[yY][eE][sS]) return 0 ;;
        *) printf 'Install cancelled.\n'; return 1 ;;
    esac
}

install_full_deps() {
    local packages=()

    if command -v apt-get >/dev/null 2>&1; then
        packages=(
            build-essential
            cmake
            pkg-config
            qt6-base-dev
            qt6-tools-dev
            qt6-tools-dev-tools
            qt6-image-formats-plugins
            libqt6svg6-dev
            libexiv2-dev
            libopencv-dev
            libmpv-dev
        )
        confirm_install "apt-get" "${packages[@]}" || return 0
        run_as_root apt-get update || return
        run_as_root apt-get install -y "${packages[@]}"
    elif command -v dnf >/dev/null 2>&1; then
        packages=(
            gcc-c++
            cmake
            pkgconf-pkg-config
            qt6-qtbase-devel
            qt6-qtimageformats
            qt6-qtsvg-devel
            qt6-qttools-devel
            exiv2-devel
            opencv-devel
            mpv-libs-devel
        )
        confirm_install "dnf" "${packages[@]}" || return 0
        run_as_root dnf install -y "${packages[@]}"
    elif command -v pacman >/dev/null 2>&1; then
        packages=(
            base-devel
            cmake
            pkgconf
            qt6-base
            qt6-imageformats
            qt6-svg
            qt6-tools
            exiv2
            opencv
            mpv
        )
        confirm_install "pacman" "${packages[@]}" || return 0
        run_as_root pacman -S --needed "${packages[@]}"
    elif command -v zypper >/dev/null 2>&1; then
        packages=(
            patterns-devel-C-C++
            cmake
            pkg-config
            qt6-base-devel
            qt6-imageformats-devel
            qt6-svg-devel
            qt6-tools-devel
            libexiv2-devel
            opencv-devel
            mpv-devel
        )
        confirm_install "zypper" "${packages[@]}" || return 0
        run_as_root zypper install -y "${packages[@]}"
    elif command -v apk >/dev/null 2>&1; then
        packages=(
            build-base
            cmake
            pkgconf
            qt6-qtbase-dev
            qt6-qtimageformats
            qt6-qtsvg-dev
            qt6-qttools-dev
            exiv2-dev
            opencv-dev
            mpv-dev
        )
        confirm_install "apk" "${packages[@]}" || return 0
        run_as_root apk add "${packages[@]}"
    elif command -v brew >/dev/null 2>&1; then
        packages=(
            cmake
            pkg-config
            qt
            exiv2
            opencv
            mpv
        )
        confirm_install "Homebrew" "${packages[@]}" || return 0
        brew install "${packages[@]}"
    else
        printf 'No supported package manager found.\n' >&2
        printf 'Install a C++ compiler, CMake, pkg-config, Qt Widgets, Qt ImageFormats, Qt Svg, Qt Tools, plus Exiv2, OpenCV, and mpv.\n' >&2
        return 1
    fi
}

require_cmake() {
    if command -v "$CMAKE_BIN" >/dev/null 2>&1; then
        return 0
    fi

    printf 'CMake executable not found: %s\n' "$CMAKE_BIN" >&2
    printf 'Install CMake or set CMAKE_BIN=/path/to/cmake before running this script.\n' >&2
    return 127
}

run_menu_action() {
    "$@"
    local status=$?

    if [[ "$status" -ne 0 ]]; then
        printf '\nCommand failed with exit code %s.\n' "$status" >&2
    fi

    exit "$status"
}

configure_default() {
    require_cmake || return

    "$CMAKE_BIN" -S "$ROOT_DIR" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
}

build_project() {
    require_cmake || return

    if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        printf 'No CMake build directory found. Configuring a full build first.\n'
        configure_default || return
    fi

    "$CMAKE_BIN" --build "$BUILD_DIR" --parallel "$JOBS"
}

find_executable() {
    local candidate
    for candidate in \
        "$BUILD_DIR/src/thumbgrid" \
        "$BUILD_DIR/src/thumbgrid.exe" \
        "$BUILD_DIR/src/thumbgrid.app/Contents/MacOS/thumbgrid"; do
        if [[ -x "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

run_executable() {
    local executable
    if ! executable="$(find_executable)"; then
        printf 'Could not find a built thumbgrid executable under %s.\n' "$BUILD_DIR" >&2
        printf 'Build the project first, then try again.\n' >&2
        return 1
    fi

    "$executable" "${APP_ARGS[@]}"
}

migrate_theme() {
    "$ROOT_DIR/scripts/migrate-theme.sh" "${APP_ARGS[@]}"
}

clean_build_dir() {
    if [[ ! -d "$BUILD_DIR" ]]; then
        printf 'Build directory does not exist: %s\n' "$BUILD_DIR"
        return 0
    fi

    read -r -p "Delete $BUILD_DIR? [y/N] " answer
    case "$answer" in
        [yY]|[yY][eE][sS])
            rm -rf "$BUILD_DIR"
            printf 'Deleted %s\n' "$BUILD_DIR"
            ;;
        *)
            printf 'Clean cancelled.\n'
            ;;
    esac
}

print_header
printf '  i) Init   - install full dependencies\n'
printf '  b) Build\n'
printf '  r) Run\n'
printf '  c) Clean  - delete build directory\n'
printf '  m) Migrate - migrate custom theme and colors\n'
printf '  q) Quit\n\n'

# Single keypress, no Enter needed. Each choice runs once and then exits
# (run_menu_action exits for i/b/r/c), so the menu does not loop.
read -rsn1 -p "Choose [i/b/c/r/m/q]: " choice
printf '%s\n\n' "$choice"
case "$choice" in
    i|I) run_menu_action install_full_deps ;;
    b|B) run_menu_action build_project ;;
    r|R) run_menu_action run_executable ;;
    c|C) run_menu_action clean_build_dir ;;
    m|M) run_menu_action migrate_theme ;;
    q|Q) exit 0 ;;
    *) printf 'Invalid option: %s\n' "$choice"; exit 1 ;;
esac
