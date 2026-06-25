# thumbgrid root (hard fork of easymodo/qimgv)
cmake_minimum_required(VERSION 3.13)

# ---------------------------------------------------------------------------
# Version: single source of truth is the latest git tag (vYYYY.M.N).
# The fallback below is only used for source tarballs without git history
# (e.g. some packaging contexts); scripts/release.sh keeps it in sync on tag.
# ---------------------------------------------------------------------------
set(THUMBGRID_VERSION_FALLBACK 2026.6.5)
find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE THUMBGRID_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
if(THUMBGRID_GIT_TAG)
    # Strip leading 'v'; --abbrev=0 yields a clean tag so project() stays numeric.
    string(REGEX REPLACE "^v" "" THUMBGRID_VERSION "${THUMBGRID_GIT_TAG}")
else()
    set(THUMBGRID_VERSION ${THUMBGRID_VERSION_FALLBACK})
endif()
message(STATUS "thumbgrid version: ${THUMBGRID_VERSION}")

project(thumbgrid
    VERSION ${THUMBGRID_VERSION}
    HOMEPAGE_URL "https://github.com/do-i/thumbgrid"
    LANGUAGES CXX)

message(STATUS "Build configuration: " ${CMAKE_BUILD_TYPE})

# check for gcc
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
        CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    message(FATAL_ERROR "\n!!! THIS PROJECT REQUIRES GCC 9.0 OR LATER !!!\n")
endif()

# AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# USER OPTIONS
# Exiv2, mpv (video) and OpenCV are mandatory and always built in. Exiv2 enables
# reading/writing Exif/Iptc/Xmp metadata, metadata-preserving saves and the
# strip-metadata action; for tags inside cr3/heif/jxl/avif, exiv2 itself must be
# built with EXIV2_ENABLE_BMFF (default in exiv2 >= 0.28).
option(KDE_SUPPORT "Support blur when using KDE" OFF)

if(UNIX AND NOT APPLE)
    set(QT_EXTERN_PATH "" CACHE STRING "Tell compile external QT path, example: (/opt/Qt/6.2.0/gcc_64)")
    string(COMPARE EQUAL "${QT_EXTERN_PATH}" "" result)
    if (NOT result)
        set(CMAKE_INSTALL_RPATH "${QT_EXTERN_PATH}/lib")
    endif()
endif()

# FIND PACKAGES
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Svg PrintSupport OpenGLWidgets)
message(STATUS "Qt Version: " ${QT_VERSION})
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg PrintSupport OpenGLWidgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(Exiv2 REQUIRED IMPORTED_TARGET exiv2)

if(KDE_SUPPORT)
    find_package(KF6WindowSystem REQUIRED)
endif()

find_package(OpenCV REQUIRED core imgproc)

##############################################################

add_subdirectory(src)

add_subdirectory(plugins/player_mpv)
