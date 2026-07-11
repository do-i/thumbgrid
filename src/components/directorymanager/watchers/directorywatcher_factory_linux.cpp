#include "linux/linuxwatcher.h"

DirectoryWatcher *DirectoryWatcher::newInstance()
{
    return new LinuxWatcher();
}
