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

Proposed behavior coverage — pending review before implementation (each as its
own behavior binary):

- file delete/trash preflight failure and permission/error reporting.
- copy/move drag-drop confirmation behavior.
- overwrite dialog flow for copy and move operations.
- folder-grid navigation and sorting regressions.
- viewer transition regressions between grid view and picture/content view.
- clipboard/file-operation state after copy, cut, paste, move, and cancel paths.
- app startup/settings isolation for first-run and folder-view defaults.
- thumbnail refresh behavior after file creation, rename, delete, and directory changes.

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

- `Opening a picture shows the picture view` — **done** (`behavior/test_opening_a_picture_shows_the_picture_view.cpp`)
  - Start in a temporary folder with at least one image.
  - Click an image thumbnail.
  - Verify thumbgrid switches from grid view to the picture/content view.
  - Verify the selected image is the content being displayed.

- `Returning from picture view shows the same folder selection` — **done** (`behavior/test_returning_from_picture_view_shows_the_same_folder_selection.cpp`)
  - Start in a temporary folder with multiple images and one child folder.
  - Open an image thumbnail, then return to folder view.
  - Verify the grid is still showing the original folder.
  - Verify the previously opened image remains selected or focused.

- `Opening a different picture from the grid replaces the current picture`
  - Start in a temporary folder with two generated images.
  - Open the first image, return to the grid, then open the second image.
  - Verify the content view displays the second image, not stale content from the first.

- `Deleting a file from the grid removes it from the folder`
  - Start in a temporary folder with two generated images.
  - Select one image and trigger the delete/trash action through the same app path a user uses.
  - Confirm the operation when prompted.
  - Verify the file is gone from the filesystem and no longer appears in the grid.

- `Cancelling delete leaves the file and grid unchanged`
  - Start in a temporary folder with one generated image.
  - Trigger the delete/trash action and cancel the confirmation.
  - Verify the file still exists and the grid item remains visible.

- `Delete failure reports an error and keeps the item visible`
  - Start in a temporary folder with a file that cannot be deleted by the app path under test.
  - Trigger delete/trash.
  - Verify an error is shown.
  - Verify the file still exists and the grid still lists it.

- `Dragging a file to a folder asks before moving it`
  - Start in a temporary folder with one image and one child folder.
  - Drag the image tile onto the child folder tile using the folder-grid drop path.
  - Verify a move/copy confirmation is shown before the filesystem changes.
  - Accept the move and verify the file ends up inside the child folder.

- `Cancelling drag-drop move leaves both folders unchanged`
  - Start in a temporary folder with one image and one child folder.
  - Drag the image tile onto the child folder tile.
  - Cancel the confirmation.
  - Verify the source file remains in the parent folder and nothing is added to the child folder.

- `Copying onto an existing filename shows the overwrite dialog`
  - Start with source and destination folders containing files with the same name.
  - Trigger the app copy path into the destination folder.
  - Verify the overwrite dialog appears.
  - Cancel the overwrite and verify the destination file remains unchanged.

- `Moving onto an existing filename shows the overwrite dialog`
  - Start with source and destination folders containing files with the same name.
  - Trigger the app move path into the destination folder.
  - Verify the overwrite dialog appears.
  - Cancel the overwrite and verify both original files remain unchanged.

- `Copy paste keeps the source file and clears clipboard state after paste`
  - Start with one image in a parent folder and an empty child folder.
  - Copy the image, open the child folder, and paste.
  - Verify both source and copied files exist.
  - Verify a second paste does not repeat unexpectedly after clipboard state is cleared.

- `Cut paste moves the source file and clears clipboard state after paste`
  - Start with one image in a parent folder and an empty child folder.
  - Cut the image, open the child folder, and paste.
  - Verify the source file is removed from the parent folder and appears in the child folder.
  - Verify a second paste does not repeat unexpectedly after clipboard state is cleared.

- `Renaming a file updates the grid item without changing folders`
  - Start in a temporary folder with one generated image.
  - Trigger the rename action and provide a new filename.
  - Verify the filesystem rename succeeded.
  - Verify the grid shows the new name and stays in the same folder.

- `Creating a folder adds a folder tile`
  - Start in an otherwise empty temporary folder.
  - Trigger the new-folder action and provide a folder name.
  - Verify the folder exists on disk.
  - Verify the grid shows the new folder tile.

- `Folder sorting keeps folders before files`
  - Start in a temporary folder with multiple images and child folders whose names sort between file names.
  - Open the folder view.
  - Verify the parent tile is first, child folders are grouped before files, and names are ordered consistently.

- `Refreshing after an external file appears updates the grid`
  - Start in a temporary folder with one image.
  - Add a second generated image outside the app path.
  - Trigger the app refresh/reload path or deterministic watcher test path.
  - Verify the new image appears without restarting thumbgrid.
