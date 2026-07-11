#include "portablewatcher.h"

DirectoryWatcher *DirectoryWatcher::newInstance()
{
    return new PortableWatcher();
}
