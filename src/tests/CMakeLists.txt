find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

if(POLICY CMP0110)
    cmake_policy(SET CMP0110 NEW)
endif()

# Registers a single-behavior test executable. Each behavior is its own binary
# so the app singletons stay isolated between behaviors.
function(thumbgrid_add_behavior_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE thumbgrid_app_objects Qt6::Test)
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
