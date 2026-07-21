#include "shortcutpresetstore.h"

#include "utils/apppaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <algorithm>

namespace {
constexpr const char *kQrcDir = ":/res/presets";

// Directories searched for presets, highest priority first: the XDG config
// dirs (user's ~/.config/thumbgrid/presets first, then the system ones), the
// compiled-in PRESETS_PATH, and finally the embedded qrc copies. Anything found
// earlier overlays a later copy with the same id.
QStringList presetDirs() {
#ifdef PRESETS_PATH
    QStringList dirs = AppPaths::configDirs(QStringLiteral("presets"), QStringLiteral(PRESETS_PATH));
#else
    QStringList dirs = AppPaths::configDirs(QStringLiteral("presets"));
#endif
    dirs << QLatin1String(kQrcDir);
    return dirs;
}

QJsonObject readObject(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return {};
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if(err.error != QJsonParseError::NoError)
        return {};
    return doc.object();
}
} // namespace

QString ShortcutPresetStore::currentOsToken() {
#if defined(_WIN32)
    return QStringLiteral("windows");
#elif defined(__APPLE__)
    return QStringLiteral("macos");
#else
    // linux and *BSD share the linux keymap/scheme
    return QStringLiteral("linux");
#endif
}

QString ShortcutPresetStore::resolvePath(const QString &id) {
    const QString file = id + QStringLiteral(".json");
    for(const QString &dir : presetDirs()) {
        const QString path = dir + QLatin1Char('/') + file;
        if(QFile::exists(path))
            return path;
    }
    return {};
}

QStringList ShortcutPresetStore::presetIds() {
    QStringList ids;
    QSet<QString> seen;
    for(const QString &dir : presetDirs()) {
        const QStringList files = QDir(dir).entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
        for(const QString &f : files) {
            const QString id = QFileInfo(f).completeBaseName();
            if(!seen.contains(id)) {
                seen.insert(id);
                ids.append(id);
            }
        }
    }
    return ids;
}

PresetInfo ShortcutPresetStore::parseMeta(const QJsonObject &root) {
    PresetInfo info;
    const QJsonObject meta = root.value(QStringLiteral("_meta")).toObject();
    if(meta.isEmpty())
        return info;
    info.id = meta.value(QStringLiteral("id")).toString();
    info.name = meta.value(QStringLiteral("name")).toString(info.id);
    info.description = meta.value(QStringLiteral("description")).toString();
    info.order = meta.value(QStringLiteral("order")).toInt(1000);
    for(const QJsonValue &v : meta.value(QStringLiteral("os")).toArray())
        info.os << v.toString();
    info.valid = !info.id.isEmpty();
    return info;
}

PresetInfo ShortcutPresetStore::meta(const QString &id) {
    return parseMeta(readRoot(id));
}

QJsonObject ShortcutPresetStore::readRoot(const QString &id) {
    const QString path = resolvePath(id);
    return path.isEmpty() ? QJsonObject() : readObject(path);
}

QList<PresetInfo> ShortcutPresetStore::available(bool currentOsOnly) {
    const QString os = currentOsToken();
    QList<PresetInfo> out;
    for(const QString &id : presetIds()) {
        const PresetInfo info = meta(id);
        if(!info.valid)
            continue;
        if(currentOsOnly && !info.os.isEmpty() && !info.os.contains(os))
            continue;
        out.append(info);
    }
    std::sort(out.begin(), out.end(), [](const PresetInfo &a, const PresetInfo &b) {
        if(a.order != b.order)
            return a.order < b.order;
        return a.id < b.id;
    });
    return out;
}
