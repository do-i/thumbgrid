#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>

// a preset file's `_meta` block; ActionManager parses the mapping
struct PresetInfo {
    QString id;
    QString name;
    QString description;
    QStringList os;
    int order = 1000;
    bool valid = false;
};

// Preset catalog (like ThemeStore). Presets seed shortcuts.json on first run.
class ShortcutPresetStore {
public:
    // "macos" | "linux" (FreeBSD normalizes to "linux").
    static QString currentOsToken();

    // XDG and PRESETS_PATH over embedded qrc, sorted by _meta.order then id; optionally current OS only
    static QList<PresetInfo> available(bool currentOsOnly = true);

    // Metadata for one preset id (invalid PresetInfo if it resolves nowhere).
    static PresetInfo meta(const QString &id);

    // <id>.json from XDG config dirs, PRESETS_PATH, then qrc; empty if nowhere
    static QString resolvePath(const QString &id);

    // Parsed root object of a preset file (empty object if it resolves nowhere).
    static QJsonObject readRoot(const QString &id);

private:
    static PresetInfo parseMeta(const QJsonObject &root);
    static QStringList presetIds();  // ids discovered across all sources
};
