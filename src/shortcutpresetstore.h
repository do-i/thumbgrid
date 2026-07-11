#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>

// Metadata describing a shortcut preset, read from each preset file's `_meta`
// block. The mapping itself (global/document/grid/platform) is parsed by
// ActionManager via ShortcutPresetStore::resolvePath().
struct PresetInfo {
    QString id;
    QString name;
    QString description;
    QStringList os;
    int order = 1000;
    bool valid = false;
};

// Owns the shortcut-preset catalog, mirroring ThemeStore's catalog role. Presets
// are seed data: the active runtime mapping is the user's shortcuts.json, created
// from the selected preset on first run.
class ShortcutPresetStore {
public:
    // "windows" | "macos" | "linux" (FreeBSD normalizes to "linux").
    static QString currentOsToken();

    // Enumerate presets (system PRESETS_PATH dir overlaid on embedded qrc),
    // sorted by _meta.order then id. When currentOsOnly, keep only presets whose
    // _meta.os contains currentOsToken().
    static QList<PresetInfo> available(bool currentOsOnly = true);

    // Metadata for one preset id (invalid PresetInfo if it resolves nowhere).
    static PresetInfo meta(const QString &id);

    // Resolve a preset id to a file path: system PRESETS_PATH/<id>.json if
    // present, else the embedded :/res/presets/<id>.json. Empty if neither.
    static QString resolvePath(const QString &id);

    // Parsed root object of a preset file (empty object if it resolves nowhere).
    static QJsonObject readRoot(const QString &id);

private:
    static PresetInfo parseMeta(const QJsonObject &root);
    static QStringList presetIds();  // ids discovered across all sources
};
