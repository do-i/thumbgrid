#include "appversion.h"

// Versioning scheme (do-i fork, independent of upstream):
//   major = year, minor = month, micro = sequential release that month (from 1).
// Must stay monotonically increasing; drives the onUpdate()/changelog and
// new-default-shortcut migration in core.cpp / actionmanager.cpp.
QVersionNumber appVersion(2026, 6, 1);
