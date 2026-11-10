#pragma once

#include <QElapsedTimer>
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

// Off by default; enable with QT_LOGGING_RULES="thumbgrid.perf.debug=true".
// Format, one per line: "perf <checkpoint>=<ms>[ n=<count>]", ms with 3 decimals.
// startup_* = ms since process start; all other checkpoints are durations.
Q_DECLARE_LOGGING_CATEGORY(logPerf)

// process-wide monotonic clock, started first thing in main()
extern QElapsedTimer gPerfClock;

// 0 when perf logging is off, so hot paths pay only the category check
inline qint64 perfStart() {
    return logPerf().isDebugEnabled() ? gPerfClock.nsecsElapsed() : 0;
}

inline void perfEnd(const char *checkpoint, qint64 startNs, qsizetype count = -1) {
    if(!startNs)
        return;
    double ms = double(gPerfClock.nsecsElapsed() - startNs) / 1e6;
    if(count < 0)
        qCDebug(logPerf, "perf %s=%.3f", checkpoint, ms);
    else
        qCDebug(logPerf, "perf %s=%.3f n=%lld", checkpoint, ms, (long long)count);
}

inline void perfMark(const char *checkpoint) {
    qCDebug(logPerf, "perf %s=%.3f", checkpoint, double(gPerfClock.nsecsElapsed()) / 1e6);
}
