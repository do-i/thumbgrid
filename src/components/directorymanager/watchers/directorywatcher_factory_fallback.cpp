#include "dummywatcher.h"

DirectoryWatcher *DirectoryWatcher::newInstance()
{
    return new DummyWatcher();
}
