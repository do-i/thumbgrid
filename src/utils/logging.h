#pragma once

#include <QLoggingCategory>

// Logging categories, one per subsystem, so logging can be filtered/enabled or
// disabled per subsystem (via QT_LOGGING_RULES / qtlogging.ini) instead of being
// unconditionally compiled in as it was with plain qDebug().
Q_DECLARE_LOGGING_CATEGORY(logCore)
Q_DECLARE_LOGGING_CATEGORY(logDirManager)
Q_DECLARE_LOGGING_CATEGORY(logCache)
Q_DECLARE_LOGGING_CATEGORY(logLoader)
Q_DECLARE_LOGGING_CATEGORY(logThumbnailer)
Q_DECLARE_LOGGING_CATEGORY(logVideo)
Q_DECLARE_LOGGING_CATEGORY(logGui)
Q_DECLARE_LOGGING_CATEGORY(logSettings)
