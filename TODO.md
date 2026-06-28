# TODO

Action items for maintaining this hard fork of [easymodo/qimgv](https://github.com/easymodo/qimgv).

## Test automation framework

Qt Test + CTest, wired into the build behind `BUILD_TESTING`. Tests link the
`thumbgrid_app_objects` object library so they exercise the real app code.

Done:

- `src/tests` is wired into the build behind `BUILD_TESTING`; `ctest` runs the suite.
- Behavior tests live in `src/tests/behavior/`, one executable per behavior so the
  app singletons stay isolated between behaviors (registered via the
  `thumbgrid_add_behavior_test` helper in `src/tests/CMakeLists.txt`).
- Shared fixtures, singleton bootstrap, and the `TG_BEHAVIOR_TEST_MAIN` macro live
  in `src/tests/support/thumbgrid_test_support.h`.
- File naming follows the project-consistent `test_<behavior>.cpp` style.
- Headless by default (`QT_QPA_PLATFORM=offscreen`), with an opt-in headed visual
  mode via `THUMBGRID_TEST_VISUAL=1`.
- Config/cache isolated via throwaway `XDG_*` dirs plus
  `QStandardPaths::setTestModeEnabled(true)` and a test-only org/app name, so tests
  never touch real user files.
- CI builds with `-DBUILD_TESTING=ON` and runs `ctest` on Linux
  (`.github/workflows/tests.yml`).

Commands:

- Headless: `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
- Watch a GUI test: `THUMBGRID_TEST_VISUAL=1 ./build/src/tests/thumbgrid_behavior_tests`

To do — add coverage around current pain points (each as its own behavior binary):

- file delete failure and permission/error reporting.
- copy/move drag-drop confirmation behavior.
- overwrite dialog flow.
- folder-grid and viewer transition regressions.

Notes / scope:

- Use `QSignalSpy` + `QTRY_*` for anything touching the threaded loader/scaler/
  thumbnailer or directory watchers; prefer the existing `dummywatcher` for
  deterministic, platform-independent watcher tests.
- Video playback (`player_mpv`) is out of scope for automated tests — it needs the
  WID backend on software-GL hosts and is not unit-testable.

## First behavior tests

These should be GUI behavior tests with plain-language test names. Use a temporary test folder with a few small generated images and subfolders, plus isolated app settings, so the tests never depend on the user's real files.

- `Starts in a folder and shows the gallery` — **done** (`behavior/test_starts_in_folder_and_shows_gallery.cpp`)
  - Start thumbgrid pointed at a temporary folder containing images and one child folder.
  - Verify the app opens in grid view.
  - Verify image thumbnails appear for the files in that folder.
  - Verify the child folder appears as a folder thumbnail/tile.

- `Opening a folder shows what is inside it` — **done** (`behavior/test_opening_a_folder_shows_its_contents.cpp`)
  - Start in a temporary parent folder with one child folder.
  - Click the child folder tile.
  - Verify the grid changes to the child folder.
  - Verify thumbnails from the child folder are shown instead of the parent folder.

- `Opening a picture shows the picture view`
  - Start in a temporary folder with at least one image.
  - Click an image thumbnail.
  - Verify thumbgrid switches from grid view to the picture/content view.
  - Verify the selected image is the content being displayed.
