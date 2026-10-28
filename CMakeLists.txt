# thumbgrid root (hard fork of easymodo/qimgv)
cmake_minimum_required(VERSION 3.13)

# ---------------------------------------------------------------------------
# Version: single source of truth is the latest git tag (vYYYY.M.N).
# The numeric MAJOR.MINOR.PATCH below feeds project() (and the macOS bundle
# version); the full "git describe" string with the commit-count/hash suffix
# for dev builds is resolved at *build* time in cmake/GenerateAppVersion.cmake
# so it stays fresh on every commit without re-running CMake. There is no
# hand-maintained fallback: a build requires git history and a tag.
# ---------------------------------------------------------------------------
find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE THUMBGRID_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
if(NOT THUMBGRID_GIT_TAG)
    message(FATAL_ERROR
        "thumbgrid version could not be determined: no git tag found "
        "(need git history with at least one vYYYY.M.N tag; use a full clone, "
        "e.g. actions/checkout with fetch-depth: 0).")
endif()
# Strip leading 'v'; --abbrev=0 yields a clean tag so project() stays numeric.
string(REGEX REPLACE "^v" "" THUMBGRID_VERSION "${THUMBGRID_GIT_TAG}")
message(STATUS "thumbgrid version: ${THUMBGRID_VERSION}")

project(thumbgrid
    VERSION ${THUMBGRID_VERSION}
    HOMEPAGE_URL "https://github.com/do-i/thumbgrid"
    LANGUAGES CXX)

message(STATUS "Build configuration: " ${CMAKE_BUILD_TYPE})

include(CTest)

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
# SVGs in browsed folders are thumbnailed: refuse QtSvg still open to CVE-2026-8168
# (fixed in 6.8.9, 6.11.2 and 6.12.0)
if(Qt6Svg_VERSION VERSION_LESS 6.8.9 OR
   (Qt6Svg_VERSION VERSION_GREATER_EQUAL 6.9.0 AND Qt6Svg_VERSION VERSION_LESS 6.11.2))
    message(FATAL_ERROR "QtSvg ${Qt6Svg_VERSION} is affected by CVE-2026-8168; use 6.8.9, 6.11.2, 6.12.0 or later")
endif()

find_package(PkgConfig REQUIRED)
# 0.28.8 fixes CVE-2026-25884/27596/27631; exiv2 parses every browsed file
pkg_check_modules(Exiv2 REQUIRED IMPORTED_TARGET exiv2>=0.28.8)

if(KDE_SUPPORT)
    find_package(KF6WindowSystem REQUIRED)
endif()

find_package(OpenCV REQUIRED core imgproc)

##############################################################

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(src/tests)
endif()

add_subdirectory(plugins/player_mpv)
