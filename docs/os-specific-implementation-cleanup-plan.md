# OS-Specific Implementation Cleanup Plan

## Goal

Track the remaining files that still mix multiple OS-specific code paths in one
translation unit after the platform desktop refactor. The main cleanup direction
is to split larger OS behaviors into small platform-specific files while leaving
minor, low-risk conditionals alone unless they get in the way.

## Inventory

### Primary Split Candidates

- `src/utils/fileoperations.cpp` - done
  - Common operation flow now delegates platform policy to
    `src/utils/fileoperations_platform.h`.
  - Platform-specific implementations now live in
    `src/utils/fileoperations_linux.cpp`,
    `src/utils/fileoperations_windows.cpp`,
    `src/utils/fileoperations_macos.cpp`,
    `src/utils/fileoperations_fallback.cpp`.
  - CMake selects one platform source in `src/utils/CMakeLists.txt`.

- `src/utils/inputmap.cpp` - done
  - The native scan-code tables and macOS modifier names are now data-driven
    keymap resources.
  - The common implementation only selects the per-OS resource path and loads
    the JSON mapping.

- `src/components/directorymanager/watchers/directorywatcher.cpp` - done
  - Common watcher lifecycle code no longer includes concrete platform
    watchers or platform selection macros.
  - `DirectoryWatcher::newInstance()` now lives in CMake-selected factory
    sources:
    `directorywatcher_factory_linux.cpp`,
    `directorywatcher_factory_windows.cpp`,
    `directorywatcher_factory_macos.cpp`,
    `directorywatcher_factory_fallback.cpp`.
  - CMake selects the native Linux/FreeBSD and Windows watcher sources only
    when their factory is selected; macOS uses `PortableWatcher`, and unknown
    platforms use `DummyWatcher`.

### Smaller Mixed-Platform Conditionals

- `src/main.cpp`
  - Uses macOS-only `MacOSApplication` include, application object creation, and
    `fileOpened` signal hookup.
  - Small and localized; probably not worth splitting unless the project wants
    almost no platform macros in entry-point code.

- `src/gui/viewers/videoplayerinitproxy.cpp`
  - Uses a Windows-specific plugin search directory and Unix-style shared library
    search paths elsewhere.
  - Small but may be worth moving into `PlatformDesktop` if plugin path behavior
    grows.

- `src/utils/stuff.h`
  - Selects `StdString` as `std::wstring` on Windows and `std::string`
    elsewhere.
  - Very small compatibility shim.

- `src/utils/stuff.cpp`
  - Converts between `QString` and `StdString` with Windows wide-string handling
    and normal string handling elsewhere.
  - Pairs with `src/utils/stuff.h`; likely low priority unless string conversion
    behavior changes.

- `src/components/actionmanager/actionmanager.cpp`
  - Chooses Apple-specific shortcut defaults from JSON on macOS and default
    shortcut entries elsewhere.
  - This is data selection rather than a full OS implementation. It may be fine
    to keep as-is.

- `src/shortcutbuilder.cpp`
  - Uses native scan-code shortcut handling on Linux, FreeBSD, and Windows, and
    text fallback elsewhere.
  - Tied to `InputMap`; if `inputmap.cpp` is split, revisit this branch at the
    same time.

## Suggested Order

1. Split `src/utils/fileoperations.cpp`. - done
   - Keep common validation and operation flow in one file.
   - Move trash implementation and any truly platform-specific permission checks
     into platform files.
   - Add CMake validation similar to the `PlatformDesktop` contract if the split
     creates required per-platform functions.

2. Split or data-drive `src/utils/inputmap.cpp`. - done
   - Move large native scan-code tables out of common logic.
   - Decide whether macOS modifier symbols belong in the same platform layer or
     in a smaller display-name helper.

3. Simplify `src/components/directorymanager/watchers/directorywatcher.cpp`. - done
   - Leave it alone if a small compile-time factory is acceptable.
   - Otherwise create per-platform factory files selected by CMake.

4. Revisit the smaller conditionals only if they start growing.
   - `main.cpp`, `videoplayerinitproxy.cpp`, `stuff.h`, `stuff.cpp`,
     `actionmanager.cpp`, and `shortcutbuilder.cpp` are currently small enough
     that splitting them may add more structure than value.

## Validation Notes

- After each split, build with limited parallelism to avoid OOM in this checkout:
  `cmake --build build --parallel 2`.
- Run behavior tests when the changed area affects runtime behavior:
  `ctest --test-dir build --output-on-failure`.
- Always run `git diff --check` before committing.
