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
