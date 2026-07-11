include(GNUInstallDirs)

set(CMAKE_VERBOSE_MAKEFILE ON)

if(APPLE)
    set(APPLE_ICON_NAME "thumbgrid.icns")
    set(APPLE_ICON_PATH "distrib/${APPLE_ICON_NAME}")
endif()

# TRANSLATIONS
find_package(Qt6 COMPONENTS LinguistTools)
# Example of creating a new translation file.
# 1. cd into the source directory and run:
#   lupdate -recursive . -ts res/translations/thumbgrid_en_US.ts    [replace "en_US" with your locale]
# 2. Add your .ts filename into TS_FILES list below
# 3. Translate it with Qt Linguist
set(TS_FILES
    "zh_CN.ts"
    "uk_UA.ts"
    "es_ES.ts"
    "de_DE.ts"
    "fr_FR.ts"
    "tr_TR.ts"
    "ja_JP.ts"
)
list(TRANSFORM TS_FILES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/res/translations/)
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION ${PROJECT_BINARY_DIR}/src/translations)

# Generate appversion.cpp from the template at *build* time, so the dev-build
# commit-count/hash suffix from "git describe" refreshes on every commit without
# re-running CMake. The script writes the file only when the version string
# actually changes, so this recompiles appversion.cpp only then, not every build.
# Generate it once now too, so the file exists for the first configure/build.
set(APPVERSION_CPP ${CMAKE_CURRENT_BINARY_DIR}/appversion.cpp)
set(GENERATE_APPVERSION_ARGS
    -DAPPVER_SRC=${CMAKE_CURRENT_SOURCE_DIR}
    -DAPPVER_BIN=${CMAKE_CURRENT_BINARY_DIR}
    -DREPO_DIR=${CMAKE_SOURCE_DIR}
    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -P ${CMAKE_SOURCE_DIR}/cmake/GenerateAppVersion.cmake)
execute_process(
    COMMAND ${CMAKE_COMMAND} ${GENERATE_APPVERSION_ARGS}
    RESULT_VARIABLE APPVERSION_GEN_RESULT)
if(NOT APPVERSION_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "failed to generate appversion.cpp (see above).")
endif()
add_custom_target(generate_appversion
    BYPRODUCTS ${APPVERSION_CPP}
    COMMAND ${CMAKE_COMMAND} ${GENERATE_APPVERSION_ARGS}
    COMMENT "Resolving thumbgrid version from git"
    VERBATIM)
set_source_files_properties(${APPVERSION_CPP} PROPERTIES GENERATED TRUE)

set(THUMBGRID_APP_TARGET thumbgrid_app_objects)

set(PLATFORM_DESKTOP_IMPLEMENTATIONS
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/platformdesktop_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/platformdesktop_freebsd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/platformdesktop_macos.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/platformdesktop_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/platformdesktop_fallback.cpp
)
if(WIN32)
    set(PLATFORM_DESKTOP_SOURCE platform/platformdesktop_windows.cpp)
elseif(APPLE)
    set(PLATFORM_DESKTOP_SOURCE platform/platformdesktop_macos.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(PLATFORM_DESKTOP_SOURCE platform/platformdesktop_freebsd.cpp)
elseif(UNIX)
    set(PLATFORM_DESKTOP_SOURCE platform/platformdesktop_linux.cpp)
else()
    set(PLATFORM_DESKTOP_SOURCE platform/platformdesktop_fallback.cpp)
endif()

# ADD APPLICATION OBJECTS
add_library(${THUMBGRID_APP_TARGET} OBJECT
    ${CMAKE_CURRENT_BINARY_DIR}/appversion.cpp
    core.cpp
    ${PLATFORM_DESKTOP_SOURCE}
    settings.cpp
    themestore.cpp
    shortcutpresetstore.cpp
    sharedresources.cpp
    shortcutbuilder.cpp
    proxystyle.cpp
    macosapplication.cpp
    resources.qrc
    thumbgrid.rc
    ${QM_FILES}
)
target_include_directories(${THUMBGRID_APP_TARGET} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
# Re-resolve the version from git before compiling appversion.cpp each build.
add_dependencies(${THUMBGRID_APP_TARGET} generate_appversion)

add_custom_target(validate_platform_impls ALL
    COMMAND ${CMAKE_COMMAND}
        -DPLATFORM_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/platform/platformdesktop.h
        "-DPLATFORM_IMPLEMENTATIONS=${PLATFORM_DESKTOP_IMPLEMENTATIONS}"
        -P ${CMAKE_SOURCE_DIR}/cmake/ValidatePlatformImpls.cmake
    COMMENT "Validating platform implementation contracts"
    VERBATIM)
add_dependencies(${THUMBGRID_APP_TARGET} validate_platform_impls)

# ADD EXECUTABLE
add_executable(thumbgrid
    main.cpp
    ${APPLE_ICON_PATH}
)
target_link_libraries(thumbgrid PRIVATE ${THUMBGRID_APP_TARGET})

# update .ts, compile .qm (requires Qt6 LinguistTools; skipped if unavailable)
if(Qt6LinguistTools_FOUND)
    qt_add_lupdate(${THUMBGRID_APP_TARGET} TS_FILES ${TS_FILES})
    qt_add_lrelease(${THUMBGRID_APP_TARGET} TS_FILES ${TS_FILES})
else()
    message(WARNING "Qt6 LinguistTools not found; translations will not be built.")
endif()

# ADD SOURCES
add_subdirectory(components)
add_subdirectory(sourcecontainers)
add_subdirectory(gui)
add_subdirectory(utils)
add_subdirectory(3rdparty/QtOpenCV)

# C++ STANDARD
target_compile_features(${THUMBGRID_APP_TARGET} PUBLIC cxx_std_17)
set_target_properties(${THUMBGRID_APP_TARGET} thumbgrid PROPERTIES CXX_EXTENSIONS OFF)

# LINK STUFF
target_link_libraries(${THUMBGRID_APP_TARGET} PUBLIC Qt6::Core Qt6::Widgets Qt6::Svg Qt6::PrintSupport Qt6::OpenGLWidgets)

# OPTION DEFINITIONS, LINKING
target_link_libraries(${THUMBGRID_APP_TARGET} PUBLIC PkgConfig::Exiv2)
target_compile_definitions(${THUMBGRID_APP_TARGET} PUBLIC USE_EXIV2)
if(KDE_SUPPORT)
    target_link_libraries(${THUMBGRID_APP_TARGET} PUBLIC KF6::WindowSystem)
    target_compile_definitions(${THUMBGRID_APP_TARGET} PUBLIC USE_KDE_BLUR)
endif()
target_compile_definitions(${THUMBGRID_APP_TARGET} PUBLIC USE_MPV)
if(WIN32)
    ADD_DEFINITIONS(-D_QIMGV_PLUGIN_DIR="plugins")
    ADD_DEFINITIONS(-D_QIMGV_PLAYER_PLUGIN="player_mpv.dll")
else()
    ADD_DEFINITIONS(-D_QIMGV_PLUGIN_DIR="${QIMGV_PLUGIN_DIR}")
    ADD_DEFINITIONS(-D_QIMGV_PLAYER_PLUGIN="player_mpv.so")
endif()
target_link_libraries(${THUMBGRID_APP_TARGET} PUBLIC ${OpenCV_LIBS})
target_compile_definitions(${THUMBGRID_APP_TARGET} PUBLIC USE_OPENCV)
# optional: KDE KSyntaxHighlighting for the text viewer; without it the
# built-in fallback highlighter (textsyntaxhighlighter.cpp) is used
find_package(KF6SyntaxHighlighting QUIET)
if(KF6SyntaxHighlighting_FOUND)
    target_link_libraries(${THUMBGRID_APP_TARGET} PUBLIC KF6::SyntaxHighlighting)
    target_compile_definitions(${THUMBGRID_APP_TARGET} PUBLIC USE_KSYNTAXHIGHLIGHTING)
    message(STATUS "Using KSyntaxHighlighting for the text viewer")
else()
    message(STATUS "KF6SyntaxHighlighting not found - text viewer uses the built-in highlighter")
endif()

# generate proper GUI program on specified platform
if(WIN32) # Check if we are on Windows
    if(MSVC) # Check if we are using the Visual Studio compiler
        set_target_properties(thumbgrid PROPERTIES WIN32_EXECUTABLE ON)
        target_link_options(thumbgrid PRIVATE "/ENTRY:mainCRTStartup")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(thumbgrid PRIVATE "-mwindows")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set(CMAKE_CXX_FLAGS "$ENV{CMAKE_CXX_FLAGS} -Wno-shift-negative-value")
        set(CMAKE_EXE_LINKER_FLAGS "$ENV{CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
        target_link_options(thumbgrid PRIVATE "-mwindows")
    else()
        message(SEND_ERROR "You are using an unsupported Windows compiler! (Not MSVC or GCC)")
    endif(MSVC)
elseif(APPLE)
    set_target_properties(thumbgrid PROPERTIES
        MACOSX_BUNDLE YES
        MACOSX_BUNDLE_BUNDLE_NAME "thumbgrid"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.do-i.thumbgrid"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/distrib/MyBundleInfo.plist.in
        MACOSX_BUNDLE_ICON_FILE ${APPLE_ICON_NAME}
    )
    set_source_files_properties(${APPLE_ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
elseif(NOT UNIX)
    message(SEND_ERROR "You are on an unsupported platform! (Not Win32, Mac OS X or Unix)")
endif()

## INSTALLATION
install(TARGETS thumbgrid DESTINATION bin)

# .desktop
install(FILES distrib/thumbgrid.desktop
    DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/applications)

# icons
foreach(icon IN ITEMS 16x16 22x22 24x24 32x32 36x36 48x48 64x64 128x128 256x256)
    install(FILES distrib/hicolor/${icon}/apps/thumbgrid.png DESTINATION
        ${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor/${icon}/apps)
endforeach()
install(FILES distrib/hicolor/scalable/apps/thumbgrid.svg
    DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor/scalable/apps)

# AppData manifest
install(FILES distrib/thumbgrid.appdata.xml DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/metainfo)

# Preset color themes. Installed to the system config dir (FHS convention for
# editable config) so admins/packagers can adjust or add presets. The same
# files are embedded via resources.qrc as a fallback when this dir is absent
# (dev tree, Windows/macOS); THEMES_PATH tells the app where to look first.
if(NOT WIN32)
    ADD_DEFINITIONS(-DTHEMES_PATH="${CMAKE_INSTALL_FULL_SYSCONFDIR}/thumbgrid/themes")
    install(DIRECTORY res/themes DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/thumbgrid)
endif()

# Shortcut presets are seed catalogs (the active mapping is the user's
# shortcuts.json). Which presets get embedded is inferred per-target-OS from each
# file's _meta.os via string(JSON) (needs CMake >= 3.19); qimgv is always
# embedded because it is the default and first-run seed. Override the set with
# -DTHUMBGRID_PRESETS="qimgv;xnviewmp". PRESETS_PATH lets admins/packagers add
# presets at runtime (mirrors THEMES_PATH).
if(WIN32)
    set(THUMBGRID_TARGET_OS "windows")
elseif(APPLE)
    set(THUMBGRID_TARGET_OS "macos")
else()
    set(THUMBGRID_TARGET_OS "linux")  # incl. *BSD (shares keymap_linux)
endif()

file(GLOB _preset_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/presets/*.json")
set(THUMBGRID_PRESET_FILES "")
foreach(_pf IN LISTS _preset_files)
    file(READ "${_pf}" _pj)
    string(JSON _pid ERROR_VARIABLE _e1 GET "${_pj}" _meta id)
    if(_e1)
        continue()  # not a preset file (no _meta.id)
    endif()
    set(_supported FALSE)
    string(JSON _oslen ERROR_VARIABLE _e2 LENGTH "${_pj}" _meta os)
    if(NOT _e2 AND _oslen GREATER 0)
        math(EXPR _oslast "${_oslen} - 1")
        foreach(_i RANGE 0 ${_oslast})
            string(JSON _osi GET "${_pj}" _meta os ${_i})
            if(_osi STREQUAL THUMBGRID_TARGET_OS)
                set(_supported TRUE)
                break()
            endif()
        endforeach()
    endif()
    # THUMBGRID_PRESETS override (explicit id list) wins if provided
    if(DEFINED THUMBGRID_PRESETS)
        if(_pid IN_LIST THUMBGRID_PRESETS)
            set(_supported TRUE)
        else()
            set(_supported FALSE)
        endif()
    endif()
    if(_supported OR _pid STREQUAL "qimgv")  # qimgv always embedded
        list(APPEND THUMBGRID_PRESET_FILES "${_pf}")
        message(STATUS "thumbgrid preset embedded: ${_pid}")
    endif()
endforeach()

qt_add_resources(${THUMBGRID_APP_TARGET} "shortcut_presets"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${THUMBGRID_PRESET_FILES})

if(NOT WIN32)
    ADD_DEFINITIONS(-DPRESETS_PATH="${CMAKE_INSTALL_FULL_SYSCONFDIR}/thumbgrid/presets")
    install(DIRECTORY res/presets DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/thumbgrid
            FILES_MATCHING PATTERN "*.json")
endif()

# Install internationalization language files
if(WIN32)
    install(DIRECTORY "${PROJECT_BINARY_DIR}/src/translations" DESTINATION "${CMAKE_INSTALL_BINDIR}")
else()
    ADD_DEFINITIONS(-DTRANSLATIONS_PATH="${CMAKE_INSTALL_FULL_DATAROOTDIR}/thumbgrid/translations")
    install(DIRECTORY "${PROJECT_BINARY_DIR}/src/translations" DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/thumbgrid")
endif()

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()
