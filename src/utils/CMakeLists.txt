## SOURCES FOR QIMGV UTILITY CLASSES

target_sources(${THUMBGRID_APP_TARGET} PRIVATE
    actions.cpp
    cmdoptionsrunner.cpp
    imagefactory.cpp
    imagelib.cpp
    inputmap.cpp
    randomizer.cpp
    script.cpp
    stuff.cpp
    fileoperations.cpp
)

if(WIN32)
    set(FILEOPERATIONS_PLATFORM_SOURCE fileoperations_windows.cpp)
elseif(APPLE)
    set(FILEOPERATIONS_PLATFORM_SOURCE fileoperations_macos.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(FILEOPERATIONS_PLATFORM_SOURCE fileoperations_linux.cpp)
else()
    set(FILEOPERATIONS_PLATFORM_SOURCE fileoperations_fallback.cpp)
endif()

target_sources(${THUMBGRID_APP_TARGET} PRIVATE
    ${FILEOPERATIONS_PLATFORM_SOURCE}
)
