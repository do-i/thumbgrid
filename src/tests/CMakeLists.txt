find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

if(POLICY CMP0110)
    cmake_policy(SET CMP0110 NEW)
endif()

# Aggregate target that builds every behavior-test executable. Build this (or the
# default "all" target) to get the whole suite before running ctest; building a
# single test executable leaves the others "Not Run".
add_custom_target(thumbgrid_behavior_tests_all)

# Registers a single-behavior test executable. Each behavior is its own binary
# so the app singletons stay isolated between behaviors.
function(thumbgrid_add_behavior_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE thumbgrid_app_objects Qt6::Test)
    add_dependencies(thumbgrid_behavior_tests_all ${name})
endfunction()

thumbgrid_add_behavior_test(thumbgrid_behavior_tests
    behavior/test_starts_in_folder_and_shows_gallery.cpp
)
add_test(
    NAME "Starts in a folder and shows the gallery"
    COMMAND thumbgrid_behavior_tests
)

thumbgrid_add_behavior_test(thumbgrid_open_folder_test
    behavior/test_opening_a_folder_shows_its_contents.cpp
)
add_test(
    NAME "Opening a folder shows what is inside it"
    COMMAND thumbgrid_open_folder_test
)

thumbgrid_add_behavior_test(thumbgrid_folder_thumbnails_test
    behavior/test_opening_a_folder_loads_its_thumbnails.cpp
)
add_test(
    NAME "Opening a folder loads its thumbnails"
    COMMAND thumbgrid_folder_thumbnails_test
)

thumbgrid_add_behavior_test(thumbgrid_open_picture_test
    behavior/test_opening_a_picture_shows_the_picture_view.cpp
)
add_test(
    NAME "Opening a picture shows the picture view"
    COMMAND thumbgrid_open_picture_test
)

thumbgrid_add_behavior_test(thumbgrid_return_from_picture_test
    behavior/test_returning_from_picture_view_shows_the_same_folder_selection.cpp
)
add_test(
    NAME "Returning from picture view shows the same folder selection"
    COMMAND thumbgrid_return_from_picture_test
)

thumbgrid_add_behavior_test(thumbgrid_shortcut_context_dispatch_test
    behavior/test_the_same_key_runs_different_actions_per_context.cpp
)
add_test(
    NAME "The same key runs different actions per view"
    COMMAND thumbgrid_shortcut_context_dispatch_test
)

thumbgrid_add_behavior_test(thumbgrid_shortcut_context_persistence_test
    behavior/test_shortcut_contexts_persist_and_legacy_configs_migrate.cpp
)
add_test(
    NAME "Shortcut contexts persist and legacy configs migrate"
    COMMAND thumbgrid_shortcut_context_persistence_test
)

thumbgrid_add_behavior_test(thumbgrid_dir_thumb_disk_cache_test
    behavior/test_dir_thumbnails_are_cached_on_disk.cpp
)
add_test(
    NAME "Directory and file thumbnails are cached on disk"
    COMMAND thumbgrid_dir_thumb_disk_cache_test
)

thumbgrid_add_behavior_test(thumbgrid_parent_nav_thumb_cache_test
    behavior/test_returning_to_parent_reuses_cached_thumbnails.cpp
)
add_test(
    NAME "Returning to the parent folder reuses cached thumbnails"
    COMMAND thumbgrid_parent_nav_thumb_cache_test
)

thumbgrid_add_behavior_test(thumbgrid_thumbnail_mem_cache_test
    behavior/test_thumbnail_mem_cache.cpp
)
add_test(
    NAME "Thumbnails are kept in a size-limited memory cache"
    COMMAND thumbgrid_thumbnail_mem_cache_test
)

thumbgrid_add_behavior_test(thumbgrid_other_file_types_test
    behavior/test_other_file_types_toggle.cpp
)
add_test(
    NAME "Other file types can be shown and text files open in the viewer"
    COMMAND thumbgrid_other_file_types_test
)
