#pragma once

#include <QString>
#include <QStringList>

// Search paths for the file-based resources thumbgrid ships (shortcut presets,
// themes, translations).
//
// Each kind is looked up across the XDG base directories rather than a single
// compiled-in location, so a user can drop their own copy into
// ~/.config/thumbgrid/<kind>/ and an admin/packager can use XDG_CONFIG_DIRS or
// XDG_DATA_DIRS. Qt resolves the XDG variables for us (QStandardPaths
// implements the base-directory spec, including its documented defaults of
// ~/.config, /etc/xdg, ~/.local/share and /usr/local/share:/usr/share), so the
// env vars are never parsed by hand and non-XDG platforms still get their
// native locations.
//
// The compiled-in path (PRESETS_PATH / THEMES_PATH / TRANSLATIONS_PATH) is
// appended last, as the default that applies when the XDG dirs hold nothing.
// Order is highest priority first; the first existing match wins.
namespace AppPaths {

// <config dirs>/thumbgrid/<subdir>, then compiledFallback if non-empty.
QStringList configDirs(const QString &subdir, const QString &compiledFallback = QString());

// <data dirs>/thumbgrid/<subdir>, then compiledFallback if non-empty.
QStringList dataDirs(const QString &subdir, const QString &compiledFallback = QString());

// First entry of dirs that contains fileName, or an empty string.
QString findFirst(const QStringList &dirs, const QString &fileName);

} // namespace AppPaths
