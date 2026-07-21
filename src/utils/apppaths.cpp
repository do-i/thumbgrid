#include "apppaths.h"

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>

namespace {

// GenericConfigLocation/GenericDataLocation are the *unbranded* bases
// (~/.config, /etc/xdg, ...), so the app directory is appended here rather than
// using AppConfigLocation, whose org+app nesting would give
// ~/.config/thumbgrid/thumbgrid/<subdir>.
QStringList dirsFor(QStandardPaths::StandardLocation base,
                    const QString &subdir,
                    const QString &compiledFallback)
{
    const QString suffix = QLatin1Char('/') + QCoreApplication::applicationName() +
                           QLatin1Char('/') + subdir;
    QStringList dirs;
    const QStringList bases = QStandardPaths::standardLocations(base);
    for(const QString &dir : bases) {
        const QString candidate = dir + suffix;
        if(!dirs.contains(candidate))
            dirs << candidate;
    }
    // Last resort: the location this build was configured to install into. It
    // may coincide with one of the XDG dirs above, hence the dedupe.
    if(!compiledFallback.isEmpty() && !dirs.contains(compiledFallback))
        dirs << compiledFallback;
    return dirs;
}

} // namespace

QStringList AppPaths::configDirs(const QString &subdir, const QString &compiledFallback) {
    return dirsFor(QStandardPaths::GenericConfigLocation, subdir, compiledFallback);
}

QStringList AppPaths::dataDirs(const QString &subdir, const QString &compiledFallback) {
    return dirsFor(QStandardPaths::GenericDataLocation, subdir, compiledFallback);
}

QString AppPaths::findFirst(const QStringList &dirs, const QString &fileName) {
    for(const QString &dir : dirs) {
        const QString path = dir + QLatin1Char('/') + fileName;
        if(QFile::exists(path))
            return path;
    }
    return {};
}
