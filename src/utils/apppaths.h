#pragma once

#include <QString>
#include <QStringList>

// Search paths for shipped resources (presets, themes, translations) across the XDG base dirs,
// so users and packagers can override them.
namespace AppPaths {

// <config dirs>/thumbgrid/<subdir>, then compiledFallback if non-empty.
QStringList configDirs(const QString &subdir, const QString &compiledFallback = QString());

// <data dirs>/thumbgrid/<subdir>, then compiledFallback if non-empty.
QStringList dataDirs(const QString &subdir, const QString &compiledFallback = QString());

// First entry of dirs that contains fileName, or an empty string.
QString findFirst(const QStringList &dirs, const QString &fileName);

} // namespace AppPaths
