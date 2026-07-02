# Fix Root-Folder Thumbnail Loading

## Summary

Fix the folder-grid bug where navigating to `/` and activating a root child leaves the child folder's cells stuck on loading icons. The fix will target thumbnail job cancellation/rescheduling during folder-view repopulation and add a root-specific behavior regression test.

## Key Changes

- Remove the temporary `[DBG ...]` `qDebug()` lines currently in `src/components/directorypresenter.cpp`.
- Change thumbnail scheduling so `DirectoryPresenter::generateThumbnails()` no longer clears all thumbnail tasks on every visible-thumbnail request.
- Clear pending thumbnail work only when the presenter repopulates for a new model/directory, so stale root jobs do not delay or cancel the newly opened child folder.
- Make `Thumbnailer` track scheduled jobs when they are queued, not only after worker start, and ensure `clearTasks()` also clears that tracking map so cancelled queued jobs can be requested again.
- Keep stale callback safety: completed thumbnails whose paths are no longer in the active `DirectoryModel` remain ignored by `indexOfFile()` / `indexOfDir()` returning `-1`.

## Test Plan

- Add a behavior test, e.g. `Opening a root folder shows thumbnails after activation`.
- Test flow:
  - Enable `settings->setAllowBrowseRoot(true)`.
  - Create a temporary fixture under `/tmp` with at least one visible child folder and image.
  - Load `/`, show the GUI, find the `/tmp` root tile by mirroring the app's root directory sorting, select it, and press Enter.
  - Assert the grid is now showing `/tmp` contents and includes the fixture entry.
  - Wait until at least one non-parent `ThumbnailWidget` in the visible grid has `isLoaded == true`, proving the cells leave the loading-icon state.
- Register the new behavior executable/test in `src/tests/CMakeLists.txt`.
- Run:
  - `cmake --build build --target thumbgrid_behavior_tests_all --parallel 2`
  - `ctest --test-dir build -R "Opening a root folder shows thumbnails after activation|Opening a folder shows what is inside it" --output-on-failure`
  - `git diff --check`

## Assumptions

- The bug is Linux/root-path behavior; the regression test will use the real `/` directory but only writes fixture files under the normal temp directory.
- The existing folder-open behavior remains unchanged: non-root parent tile navigation and normal child-folder activation should continue to pass.
- Existing user work in `src/components/directorypresenter.cpp` is diagnostic logging for this bug and should be removed as part of the fix.
