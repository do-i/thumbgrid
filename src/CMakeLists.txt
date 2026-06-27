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

# Generate appversion.cpp from the template, filling in the version that the
# root CMakeLists derived from the latest git tag (single source of truth).
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/appversion.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/appversion.cpp
    @ONLY)

set(THUMBGRID_APP_TARGET thumbgrid_app_objects)

# ADD APPLICATION OBJECTS
add_library(${THUMBGRID_APP_TARGET} OBJECT
    ${CMAKE_CURRENT_BINARY_DIR}/appversion.cpp
    core.cpp
    settings.cpp
    themestore.cpp
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
