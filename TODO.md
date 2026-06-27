# TODO

Action items for maintaining this hard fork of [easymodo/qimgv](https://github.com/easymodo/qimgv).

## Test automation framework

- Wire the existing `src/tests` Qt Test setup into the normal CMake build with `BUILD_TESTING` / CTest.
- Rename the current generic `unit_tests` target to a project-specific target such as `thumbgrid_unit_tests`.
- Keep tests under `src/tests`, but split by purpose as coverage grows:
  - `src/tests/unit/` for deterministic logic and widget unit tests.
  - `src/tests/gui/` for interaction tests that need visible widgets, dialogs, focus, or drag/drop.
  - `src/tests/support/` for reusable test fixtures and GUI helpers.
- Keep the existing `test_<thing>.cpp` naming style, for example `test_mapoverlay.cpp` and `test_fileoperations.cpp`.
- Add a documented headless test command, for example `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`.
- Add an opt-in headed visual verification mode, for example `THUMBGRID_TEST_VISUAL=1`, so GUI tests can be watched by a human when needed.
- Make tests use temporary directories and isolated settings/config paths so they do not touch real user files.
- Add first useful coverage around current pain points:
  - file delete failure and permission/error reporting.
  - copy/move drag-drop confirmation behavior.
  - overwrite dialog flow.
  - folder-grid and viewer transition regressions.
- Add CI coverage after local tests are stable, starting with build plus CTest on Linux.

## First behavior tests

These should be GUI behavior tests with plain-language test names. Use a temporary test folder with a few small generated images and subfolders, plus isolated app settings, so the tests never depend on the user's real files.

- `Starts in a folder and shows the gallery`
  - Start thumbgrid pointed at a temporary folder containing images and one child folder.
  - Verify the app opens in grid view.
  - Verify image thumbnails appear for the files in that folder.
  - Verify the child folder appears as a folder thumbnail/tile.

- `Opening a folder shows what is inside it`
  - Start in a temporary parent folder with one child folder.
  - Click the child folder tile.
  - Verify the grid changes to the child folder.
  - Verify thumbnails from the child folder are shown instead of the parent folder.

- `Opening a picture shows the picture view`
  - Start in a temporary folder with at least one image.
  - Click an image thumbnail.
  - Verify thumbgrid switches from grid view to the picture/content view.
  - Verify the selected image is the content being displayed.
