#include "windows/windowswatcher.h"

DirectoryWatcher *DirectoryWatcher::newInstance()
{
    return new WindowsWatcher();
}
