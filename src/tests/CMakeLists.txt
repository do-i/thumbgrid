find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

if(POLICY CMP0110)
    cmake_policy(SET CMP0110 NEW)
endif()

add_executable(thumbgrid_behavior_tests
    behavior/test_starts_in_folder_and_shows_gallery.cpp
)

target_link_libraries(thumbgrid_behavior_tests PRIVATE
    thumbgrid_app_objects
    Qt6::Test
)

add_test(
    NAME "Starts in a folder and shows the gallery"
    COMMAND thumbgrid_behavior_tests
)
