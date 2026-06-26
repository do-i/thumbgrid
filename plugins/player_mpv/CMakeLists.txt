cmake_minimum_required(VERSION 3.11)
project(player_mpv
    DESCRIPTION "video player widget for thumbgrid (using mpv)"
    LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

# only export CreatePlayerWidget function
ADD_DEFINITIONS(-DQIMGV_PLAYER_MPV_LIBRARY)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Mpv REQUIRED IMPORTED_TARGET mpv)
endif()

include(GNUInstallDirs)

# Two mpv embedding backends:
#  - default (PLAYER_MPV_USE_WID=ON): hand mpv our native window id and let it
#    pick its own video output (gpu -> xv -> x11). Works on both real GPUs and
#    software rendering / headless VMs.
#  - PLAYER_MPV_USE_WID=OFF: libmpv OpenGL render API into a QOpenGLWidget.
#    Tighter Qt integration, but needs a working OpenGL stack -- on software
#    rendering the render-API path produces a blank video frame.
option(PLAYER_MPV_USE_WID "Embed mpv via native window id instead of the OpenGL render API" ON)

if(PLAYER_MPV_USE_WID)
    set(PLAYER_MPV_WIDGET_SRC src/nogl/mpvwidget.cpp src/nogl/videoplayermpv.cpp)
    set(PLAYER_MPV_WIDGET_INC src/nogl)
else()
    set(PLAYER_MPV_WIDGET_SRC src/mpvwidget.cpp src/videoplayermpv.cpp)
    set(PLAYER_MPV_WIDGET_INC)
endif()

add_library(player_mpv MODULE
    src/videoplayer.cpp
    ${PLAYER_MPV_WIDGET_SRC}
    src/qthelper.hpp)

target_compile_features(player_mpv PRIVATE cxx_std_11)

if(WIN32)
    target_link_libraries(player_mpv PRIVATE
        Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Mpv)
else()
    target_link_libraries(player_mpv PRIVATE
        Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets PkgConfig::Mpv)
endif()

if(QT_VERSION_MAJOR GREATER_EQUAL 6 AND NOT PLAYER_MPV_USE_WID)
    target_link_libraries(player_mpv PRIVATE Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
endif()

set_target_properties(player_mpv PROPERTIES
    CXX_EXTENSIONS OFF
    PREFIX "")

# Deploy the freshly built plugin next to where the in-tree binary looks for it
# (<build>/lib/thumbgrid, i.e. ../lib/thumbgrid relative to the app), so running
# from the build tree always picks up plugin changes instead of a stale copy.
add_custom_command(TARGET player_mpv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lib/thumbgrid"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:player_mpv>" "${CMAKE_BINARY_DIR}/lib/thumbgrid/"
    VERBATIM)

target_include_directories(player_mpv PRIVATE ${PLAYER_MPV_WIDGET_INC} src)

if(WIN32)
    target_include_directories(player_mpv PRIVATE
        ${MPV_DIR}/include)
    target_link_directories(player_mpv PRIVATE
        ${MPV_DIR}/lib/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x86_64,i686>)
endif()

if(WIN32)
    install(TARGETS player_mpv LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins")
else()
    install(TARGETS player_mpv LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/thumbgrid")
endif()
