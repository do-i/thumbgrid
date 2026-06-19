#!/usr/bin/env bash
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
    printf '\nqimgv build menu\n'
    printf 'Source: %s\n' "$ROOT_DIR"
    printf 'Build:  %s\n' "$BUILD_DIR"
    printf 'Type:   %s\n\n' "$BUILD_TYPE"
}

pause() {
    read -r -p "Press Enter to continue..."
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

    printf 'This installs dependencies for the minimal qimgv build.\n'
    printf 'Optional feature deps for Exiv2, OpenCV, and mpv are not included.\n\n'
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

install_required_deps() {
    local packages=()

    if command -v apt-get >/dev/null 2>&1; then
        packages=(
            build-essential
            cmake
            pkg-config
            qtbase5-dev
            qttools5-dev
            qttools5-dev-tools
            libqt5svg5-dev
        )
        confirm_install "apt-get" "${packages[@]}" || return 0
        run_as_root apt-get update || return
        run_as_root apt-get install -y "${packages[@]}"
    elif command -v dnf >/dev/null 2>&1; then
        packages=(
            gcc-c++
            cmake
            pkgconf-pkg-config
            qt5-qtbase-devel
            qt5-qtsvg-devel
            qt5-qttools-devel
        )
        confirm_install "dnf" "${packages[@]}" || return 0
        run_as_root dnf install -y "${packages[@]}"
    elif command -v pacman >/dev/null 2>&1; then
        packages=(
            base-devel
            cmake
            pkgconf
            qt5-base
            qt5-svg
            qt5-tools
        )
        confirm_install "pacman" "${packages[@]}" || return 0
        run_as_root pacman -S --needed "${packages[@]}"
    elif command -v zypper >/dev/null 2>&1; then
        packages=(
            patterns-devel-C-C++
            cmake
            pkg-config
            libqt5-qtbase-devel
            libqt5-qtsvg-devel
            libqt5-qttools-devel
        )
        confirm_install "zypper" "${packages[@]}" || return 0
        run_as_root zypper install -y "${packages[@]}"
    elif command -v apk >/dev/null 2>&1; then
        packages=(
            build-base
            cmake
            pkgconf
            qt5-qtbase-dev
            qt5-qtsvg-dev
            qt5-qttools-dev
        )
        confirm_install "apk" "${packages[@]}" || return 0
        run_as_root apk add "${packages[@]}"
    elif command -v brew >/dev/null 2>&1; then
        packages=(
            cmake
            pkg-config
            qt
        )
        confirm_install "Homebrew" "${packages[@]}" || return 0
        brew install "${packages[@]}"
    else
        printf 'No supported package manager found.\n' >&2
        printf 'Install a C++ compiler, CMake, pkg-config, Qt Widgets, Qt Svg, Qt PrintSupport, and Qt LinguistTools.\n' >&2
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

    pause
}

configure_default() {
    require_cmake || return

    "$CMAKE_BIN" -S "$ROOT_DIR" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
}

configure_minimal() {
    require_cmake || return

    "$CMAKE_BIN" -S "$ROOT_DIR" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DEXIV2=OFF \
        -DOPENCV_SUPPORT=OFF \
        -DVIDEO_SUPPORT=OFF
}

build_project() {
    require_cmake || return

    if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        printf 'No CMake build directory found. Configuring a minimal build first.\n'
        configure_minimal || return
    fi

    "$CMAKE_BIN" --build "$BUILD_DIR" --parallel "$JOBS"
}

find_executable() {
    local candidate
    for candidate in \
        "$BUILD_DIR/qimgv/qimgv" \
        "$BUILD_DIR/qimgv/qimgv.exe" \
        "$BUILD_DIR/qimgv/qimgv.app/Contents/MacOS/qimgv"; do
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
        printf 'Could not find a built qimgv executable under %s.\n' "$BUILD_DIR" >&2
        printf 'Build the project first, then try again.\n' >&2
        return 1
    fi

    "$executable" "${APP_ARGS[@]}"
}

build_and_run() {
    build_project || return
    run_executable
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

while true; do
    print_header
    printf '1) Install required deps (minimal build)\n'
    printf '2) Configure minimal build\n'
    printf '3) Build\n'
    printf '4) Run executable\n'
    printf '5) Build and run\n'
    printf '6) Configure full build (requires optional deps)\n'
    printf '7) Clean build directory\n'
    printf '8) Exit\n\n'

    printf 'Note: full build enables Exiv2, OpenCV, and mpv support.\n\n'

    read -r -p "Choose an option: " choice
    case "$choice" in
        1) run_menu_action install_required_deps ;;
        2) run_menu_action configure_minimal ;;
        3) run_menu_action build_project ;;
        4) run_menu_action run_executable ;;
        5) run_menu_action build_and_run ;;
        6) run_menu_action configure_default ;;
        7) run_menu_action clean_build_dir ;;
        8|q|Q) exit 0 ;;
        *) printf 'Invalid option: %s\n' "$choice"; pause ;;
    esac
done
