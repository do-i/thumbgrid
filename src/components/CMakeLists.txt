## SOURCES FOR QIMGV COMPONENTS

target_sources(${THUMBGRID_APP_TARGET} PRIVATE
    directorymodel.cpp
    directorypresenter.cpp
    fileoperationscontroller.cpp

    scriptmanager/scriptmanager.cpp
    actionmanager/actionmanager.cpp

    cache/cache.cpp
    cache/cacheitem.cpp
    cache/thumbnailcache.cpp

    loader/loader.cpp
    loader/loaderrunnable.cpp

    scaler/scaler.cpp
    scaler/scalerrunnable.cpp

    thumbnailer/thumbnailer.cpp
    thumbnailer/thumbnailerrunnable.cpp

    directorymanager/directorymanager.cpp

    duplicatefinder/imagehasher.cpp
    duplicatefinder/duplicatefinder.cpp

    directorymanager/watchers/directorywatcher.cpp
    directorymanager/watchers/dummywatcher.cpp
    directorymanager/watchers/portablewatcher.cpp
    directorymanager/watchers/watcherevent.cpp
    directorymanager/watchers/watcherworker.cpp

)

if(WIN32)
    target_sources(${THUMBGRID_APP_TARGET} PRIVATE
        directorymanager/watchers/directorywatcher_factory_windows.cpp
        directorymanager/watchers/windows/windowswatcher.cpp
        directorymanager/watchers/windows/windowsworker.cpp)
elseif(APPLE)
    target_sources(${THUMBGRID_APP_TARGET} PRIVATE
        directorymanager/watchers/directorywatcher_factory_macos.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_sources(${THUMBGRID_APP_TARGET} PRIVATE
        directorymanager/watchers/directorywatcher_factory_linux.cpp
        directorymanager/watchers/linux/linuxfsevent.cpp
        directorymanager/watchers/linux/linuxwatcher.cpp
        directorymanager/watchers/linux/linuxworker.cpp)
else()
    target_sources(${THUMBGRID_APP_TARGET} PRIVATE
        directorymanager/watchers/directorywatcher_factory_fallback.cpp)
endif()
