#pragma once

#include <QString>
#include <QVersionNumber>

extern QVersionNumber appVersion;
// Clean release string ("2026.7.1"); dev-suffixed describe string
// ("2026.7.1-22-g3bafbd99"). Both resolved at build time from the latest tag.
extern const QString appVersionShort;
extern const QString appVersionFull;
