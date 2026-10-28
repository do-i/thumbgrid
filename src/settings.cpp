#include "settings.h"
#include "platform/platformdesktop.h"

#include "appversion.h"
#include "utils/logging.h"

#include <QDateTime>
#include <QLockFile>

Settings *settings = nullptr;

namespace {

QColor colorFromVariant(const QVariant &value, const QColor &fallback) {
    QColor color = value.value<QColor>();
    if(!color.isValid())
        color = QColor(value.toString());
    return color.isValid() ? color : fallback;
}

QColor colorFromSettings(QSettings *primary, const QString &primaryKey,
                         QSettings *fallbackSettings, const QString &fallbackKey,
                         const QColor &fallbackColor) {
    if(primary->contains(primaryKey))
        return colorFromVariant(primary->value(primaryKey), fallbackColor);
    if(fallbackSettings->contains(fallbackKey))
        return colorFromVariant(fallbackSettings->value(fallbackKey), fallbackColor);
    return fallbackColor;
}

int normalizedThemeTid(int tid) {
    // older builds stored custom as 5 or 6
    if(tid == 5 || tid == 6)
        return COLORS_CUSTOM;
    return tid;
}

// tid <-> display name/token; unlisted tids (COLORS_CUSTOM) map to "Custom"
struct ThemeIdMapping {
    int tid;
    const char *name;  // display name, mirrors the system theme files
    const char *token; // lowercase token stored in [General]/theme
};

constexpr ThemeIdMapping kThemeIdMappings[] = {
    { COLORS_SYSTEM,       "System",     "system" },
    { COLORS_LIGHT,        "Light",      "light" },
    { COLORS_BLACK,        "Black",      "black" },
    { COLORS_DARK,         "Dark",       "dark" },
    { COLORS_DARKBLUE,     "Dark Blue",  "darkblue" },
    { COLORS_LIGHT_YELLOW, "Light Blue", "light_yellow" },
};

// Display name stored alongside the tid, mirroring the system theme files.
QString themeName(int tid) {
    for(const auto &m : kThemeIdMappings)
        if(m.tid == tid)
            return QString::fromLatin1(m.name);
    return QStringLiteral("Custom");
}

// lowercase [General]/theme token: preset file basename, or "system"/"custom"
QString themeToken(int tid) {
    for(const auto &m : kThemeIdMappings)
        if(m.tid == tid)
            return QString::fromLatin1(m.token);
    return QStringLiteral("custom");
}

int themeTidFromToken(const QString &token) {
    const QString t = token.trimmed().toLower();
    for(const auto &m : kThemeIdMappings)
        if(t == QLatin1String(m.token))
            return m.tid;
    return COLORS_CUSTOM;
}

// Config group per key, mirroring the settings dialog; unlisted keys manage their own group.
QString settingGroupFor(const QString &key) {
    static const QHash<QString, QString> groups = {
        // General
        {"theme", "General"}, {"backgroundOpacity", "General"}, {"blurBackground", "General"},
        {"autoResizeWindow", "General"}, {"autoResizeLimit", "General"}, {"cursorAutohiding", "General"}, {"cursorAutohideMode", "General"},
        {"defaultViewMode", "General"}, {"enableSmoothScroll", "General"}, {"infoBarFullscreen", "General"},
        {"infoBarWindowed", "General"}, {"language", "General"}, {"openInFullscreen", "General"},
        {"showFullMetadata", "General"}, {"windowTitleExtendedInfo", "General"}, {"zoomIndicatorMode", "General"},
        {"firstRun", "General"}, {"lastVerMajor", "General"}, {"lastVerMinor", "General"},
        {"lastVerMicro", "General"},
        // Grid
        {"sortingMode", "Grid"}, {"sortFolders", "Grid"}, {"allowBrowseRoot", "Grid"},
        {"showHiddenFiles", "Grid"}, {"showOtherFileTypes", "Grid"}, {"folderEndAction", "Grid"},
        {"folderViewTopBar", "Grid"}, {"folderViewFontPointSize", "Grid"}, {"folderViewMode", "Grid"},
        {"folderViewShowInfo", "Grid"}, {"folderViewPreviewFit", "Grid"}, {"folderViewIconSize", "Grid"},
        // Document
        {"videoPlayback", "Document"}, {"playVideoSounds", "Document"}, {"showVideoControls", "Document"},
        {"panelEnabled", "Document"}, {"panelFullscreenOnly", "Document"}, {"squareThumbnails", "Document"},
        {"drawTransparencyGrid", "Document"}, {"smoothUpscaling", "Document"}, {"expandImage", "Document"},
        {"expandLimit", "Document"}, {"smoothAnimatedImages", "Document"}, {"loopSlideshow", "Document"},
        {"slideshowInterval", "Document"}, {"defaultFitMode", "Document"}, {"keepFitMode", "Document"},
        {"focusPointIn1to1Mode", "Document"}, {"unlockMinZoom", "Document"}, {"zoomStep", "Document"},
        {"useFixedZoomLevels", "Document"}, {"fixedZoomLevels", "Document"}, {"scalingFilter", "Document"}, {"scalingEngine", "Document"}, {"scalingSharpen", "Document"},
        {"resizeFilter", "Document"},
        {"imageScrolling", "Document"}, {"mouseScrollingSpeed", "Document"}, {"trackpadDetection", "Document"},
        {"clickableEdges", "Document"}, {"clickableEdgesVisible", "Document"}, {"thumbPanelStyle", "Document"},
        {"panelPosition", "Document"}, {"panelPinned", "Document"}, {"panelPreviewsSize", "Document"},
        {"panelCenterSelection", "Document"},
        // Advanced
        {"usePreloader", "Advanced"}, {"thumbnailCache", "Advanced"}, {"unloadThumbs", "Advanced"},
        {"confirmDelete", "Advanced"}, {"confirmTrash", "Advanced"}, {"showSaveOverlay", "Advanced"},
        {"jxlAnimation", "Advanced"}, {"mpvBinary", "Advanced"}, {"JPEGSaveQuality", "Advanced"},
        {"thumbnailerThreads", "Advanced"}, {"thumbnailerMemCacheLimit", "Advanced"},
        {"duplicateSearchThreads", "Advanced"},
        {"memoryAllocationLimit", "Advanced"}, {"cacheDir", "Advanced"},
        // Shortcuts (preset pointer + dirty flag; the mapping lives in shortcuts.json)
        {"preset", "Shortcuts"}, {"modified", "Shortcuts"},
        // State (runtime state persisted between sessions; not part of the settings dialog)
        {"volume", "State"}, {"maximizedWindow", "State"}, {"lastDisplay", "State"},
        {"windowGeometry", "State"}, {"savedPaths", "State"}, {"bookmarks", "State"},
        {"placesPanel", "State"}, {"placesPanelBookmarksExpanded", "State"},
        {"placesPanelTreeExpanded", "State"}, {"placesPanelWidth", "State"},
        {"duplicateFinderGeometry", "State"}, {"duplicateFinderHeader", "State"},
        {"fileInfoDialogGeometry", "State"},
        {"duplicateFinderSimilarity", "State"}, {"duplicateFinderRecursive", "State"},
        {"duplicateFinderRotated", "State"}, {"duplicateFinderMirrored", "State"},
        {"duplicateFinderTargets", "State"},
        {"printLandscape", "State"}, {"printPdfDefault", "State"},
        {"printColor", "State"}, {"printFitToPage", "State"}, {"lastPrinter", "State"},
        {"shortcutsSortColumn", "State"}, {"shortcutsSortOrder", "State"},
        {"clearOnExit", "State"},
    };
    return groups.value(key);
}

QString groupedKey(const QString &key) {
    const QString group = settingGroupFor(key);
    // QSettings escapes an explicit "General" group, so General keys stay ungrouped
    if(group.isEmpty() || group == QLatin1String("General"))
        return key;
    return group + QLatin1Char('/') + key;
}

// one-time theme.conf -> theme.ini migration, only when theme.ini is absent
void migrateLegacyThemeConf(const QString &confDir, QSettings *themeConf) {
    const QString legacyPath = confDir + "/theme.conf";
    if(!QFile::exists(legacyPath) || QFile::exists(confDir + "/theme.ini"))
        return;
    QSettings legacy(legacyPath, QSettings::IniFormat);
    legacy.beginGroup("Colors");
    const int tid = normalizedThemeTid(legacy.value("tid", COLORS_CUSTOM).toInt());
    const QStringList colorKeys = legacy.childKeys();
    themeConf->beginGroup("Theme");
    themeConf->setValue("tid", tid);
    themeConf->setValue("name", themeName(tid));
    themeConf->endGroup();
    themeConf->beginGroup("Colors");
    for(const QString &key : colorKeys) {
        if(key != QLatin1String("tid"))
            themeConf->setValue(key, legacy.value(key));
    }
    themeConf->endGroup();
    legacy.endGroup();
    themeConf->sync();
}

// Context token strings must match ActionManager::contextToString().
QString shortcutContextToken(ViewMode context) {
    if(context == MODE_GLOBAL)
        return QStringLiteral("global");
    if(context == MODE_COPYMOVE)
        return QStringLiteral("copyMove");
    return context == MODE_FOLDERVIEW ? QStringLiteral("grid")
                                      : QStringLiteral("document");
}

ViewMode shortcutContextFromToken(const QString &token) {
    if(token == QLatin1String("global"))
        return MODE_GLOBAL;
    if(token == QLatin1String("copyMove"))
        return MODE_COPYMOVE;
    return (token == QLatin1String("grid") || token == QLatin1String("folderview"))
        ? MODE_FOLDERVIEW
        : MODE_DOCUMENT;
}

bool isShortcutContextToken(const QString &token) {
    return token == QLatin1String("grid") ||
           token == QLatin1String("folderview") ||
           token == QLatin1String("global") ||
           token == QLatin1String("document") ||
           token == QLatin1String("copyMove");
}

QStringList normalizedShortcutKeys(QStringList keys) {
    for(QString &key : keys)
        key = key.trimmed();
    keys.removeAll(QString());
    keys.removeDuplicates();
    keys.sort(Qt::CaseInsensitive);
    return keys;
}

QJsonArray shortcutKeysToJson(const QStringList &keys) {
    QJsonArray arr;
    for(const QString &key : normalizedShortcutKeys(keys))
        arr.append(key);
    return arr;
}

QStringList shortcutKeysFromJson(const QJsonValue &value) {
    QStringList keys;
    const QJsonArray arr = value.isArray()
        ? value.toArray()
        : value.toObject().value("keys").toArray();
    for(const QJsonValue &key : arr)
        keys.append(key.toString());
    return normalizedShortcutKeys(keys);
}

QString shortcutPrimaryFromEntry(const QJsonValue &value) {
    if(!value.isObject())
        return QString();
    return value.toObject().value("primacy").toString().trimmed();
}

QJsonValue shortcutEntryToJson(const QStringList &keys, const QString &primary) {
    const QJsonArray keyArray = shortcutKeysToJson(keys);
    const QString normalizedPrimary = primary.trimmed();
    if(normalizedPrimary.isEmpty())
        return keyArray;

    QJsonObject entry;
    entry.insert("keys", keyArray);
    entry.insert("primacy", normalizedPrimary);
    return entry;
}

bool shortcutContextUsesLegacyKeyActionShape(const QJsonObject &bindings) {
    for(auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
        if(it.value().isString())
            return true;
    }
    return false;
}

QMap<QString, QStringList> actionToKeys(const QMap<QString, QString> &shortcutToAction) {
    QMap<QString, QStringList> out;
    for(auto it = shortcutToAction.cbegin(); it != shortcutToAction.cend(); ++it)
        out[it.value()].append(it.key());
    for(auto it = out.begin(); it != out.end(); ++it)
        it.value() = normalizedShortcutKeys(it.value());
    return out;
}

void insertActionKeys(QMap<QString, QString> &shortcutToAction,
                      const QString &action,
                      const QStringList &keys) {
    if(action.isEmpty())
        return;
    for(const QString &key : normalizedShortcutKeys(keys))
        shortcutToAction.insert(key, action);
}

void readEmbeddedShortcutPrimary(const QJsonObject &root,
                                 QMap<ViewMode, QMap<QString, QString>> &primary) {
    for(auto ctx = root.constBegin(); ctx != root.constEnd(); ++ctx) {
        if(!isShortcutContextToken(ctx.key()))
            continue;
        const ViewMode mode = shortcutContextFromToken(ctx.key());
        const QJsonObject bindings = ctx.value().toObject();
        if(shortcutContextUsesLegacyKeyActionShape(bindings))
            continue;
        for(auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
            const QString primacy = shortcutPrimaryFromEntry(it.value());
            if(!primacy.isEmpty())
                primary[mode].insert(it.key(), primacy);
        }
    }
}

bool collapseGlobalShortcutDuplicates(QMap<ViewMode, QMap<QString, QString>> &shortcuts) {
    const QMap<ViewMode, QMap<QString, QString>> before = shortcuts;
    QMap<QString, QString> &global = shortcuts[MODE_GLOBAL];
    QMap<QString, QString> &document = shortcuts[MODE_DOCUMENT];
    QMap<QString, QString> &grid = shortcuts[MODE_FOLDERVIEW];

    for(auto it = document.begin(); it != document.end();) {
        if(grid.value(it.key()) == it.value() && !global.contains(it.key())) {
            global.insert(it.key(), it.value());
            grid.remove(it.key());
            it = document.erase(it);
        } else {
            ++it;
        }
    }

    auto collapseAgainstGlobal = [&global](QMap<QString, QString> &context) {
        for(auto it = context.begin(); it != context.end();) {
            if(global.value(it.key()) == it.value())
                it = context.erase(it);
            else
                ++it;
        }
    };
    collapseAgainstGlobal(document);
    collapseAgainstGlobal(grid);
    return shortcuts != before;
}

// { context: { action: [sorted keys] } }, or { "keys": [...], "primacy": key } when a primary is set
void writeShortcutsJson(const QString &path,
                        const QMap<ViewMode, QMap<QString, QString>> &shortcuts) {
    QJsonObject root;
    QMap<ViewMode, QMap<QString, QString>> primary;
    QFile existing(path);
    if(existing.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(existing.readAll()).object();
        existing.close();
        readEmbeddedShortcutPrimary(root, primary);
        const QJsonObject legacyPrimary = root.value("_primary").toObject();
        for(auto ctx = legacyPrimary.constBegin(); ctx != legacyPrimary.constEnd(); ++ctx) {
            if(!isShortcutContextToken(ctx.key()))
                continue;
            const ViewMode mode = shortcutContextFromToken(ctx.key());
            const QJsonObject contextValues = ctx.value().toObject();
            for(auto it = contextValues.constBegin(); it != contextValues.constEnd(); ++it)
                primary[mode].insert(it.key(), it.value().toString());
        }
    }
    for(const QString &key : root.keys()) {
        if(isShortcutContextToken(key))
            root.remove(key);
    }
    root.remove("_primary");
    // 2026.10.3's clip edit context (Esc / Enter while editing) is gone
    root.remove("clipEdit");
    for(auto ctx = shortcuts.cbegin(); ctx != shortcuts.cend(); ++ctx) {
        QJsonObject bindings;
        const QMap<QString, QStringList> byAction = actionToKeys(ctx.value());
        for(auto it = byAction.cbegin(); it != byAction.cend(); ++it)
            bindings.insert(it.key(), shortcutEntryToJson(it.value(), primary.value(ctx.key()).value(it.key())));
        root.insert(shortcutContextToken(ctx.key()), bindings);
    }
    // QSaveFile needs the parent directory
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if(file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
}

bool readShortcutsJson(const QString &path,
                       QMap<ViewMode, QMap<QString, QString>> &shortcuts) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    bool needsRewrite = root.contains("_primary");
    for(auto ctx = root.constBegin(); ctx != root.constEnd(); ++ctx) {
        if(!isShortcutContextToken(ctx.key()))
            continue;
        if(ctx.key() == QLatin1String("folderview"))
            needsRewrite = true;
        const ViewMode mode = shortcutContextFromToken(ctx.key());
        const QJsonObject bindings = ctx.value().toObject();
        const bool legacyShape = shortcutContextUsesLegacyKeyActionShape(bindings);
        if(legacyShape)
            needsRewrite = true;
        for(auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
            if(legacyShape) {
                shortcuts[mode].insert(it.key(), it.value().toString());  // keySequence -> action
            } else {
                insertActionKeys(shortcuts[mode], it.key(), shortcutKeysFromJson(it.value()));
            }
        }
    }
    return collapseGlobalShortcutDuplicates(shortcuts) || needsRewrite;
}

void writeShortcutStringMapJson(const QString &path, const QString &metadataKey,
                                const QMap<ViewMode, QMap<QString, QString>> &values) {
    QJsonObject root;
    QFile existing(path);
    if(existing.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(existing.readAll()).object();
        existing.close();
    }

    if(metadataKey == QLatin1String("_primary")) {
        root.remove("_primary");
        for(const QString &contextKey : root.keys()) {
            if(!isShortcutContextToken(contextKey))
                continue;
            const ViewMode mode = shortcutContextFromToken(contextKey);
            QJsonObject bindings = root.value(contextKey).toObject();
            if(shortcutContextUsesLegacyKeyActionShape(bindings))
                continue;
            for(const QString &action : bindings.keys()) {
                const QStringList keys = shortcutKeysFromJson(bindings.value(action));
                bindings.insert(action, shortcutEntryToJson(keys, values.value(mode).value(action)));
            }
            root.insert(contextKey, bindings);
        }

        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile file(path);
        if(file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.commit();
        }
        return;
    }

    QJsonObject metadata;
    for(auto ctx = values.cbegin(); ctx != values.cend(); ++ctx) {
        QJsonObject contextValues;
        for(auto it = ctx.value().cbegin(); it != ctx.value().cend(); ++it) {
            if(!it.value().isEmpty())
                contextValues.insert(it.key(), it.value());
        }
        metadata.insert(shortcutContextToken(ctx.key()), contextValues);
    }
    root.insert(metadataKey, metadata);

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if(file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
}

void readShortcutStringMapJson(const QString &path, const QString &metadataKey,
                               QMap<ViewMode, QMap<QString, QString>> &values) {
    values.clear();
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if(metadataKey == QLatin1String("_primary"))
        readEmbeddedShortcutPrimary(root, values);
    const QJsonObject metadata = root.value(metadataKey).toObject();
    for(auto ctx = metadata.constBegin(); ctx != metadata.constEnd(); ++ctx) {
        if(!isShortcutContextToken(ctx.key()))
            continue;
        const ViewMode mode = shortcutContextFromToken(ctx.key());
        const QJsonObject contextValues = ctx.value().toObject();
        for(auto it = contextValues.constBegin(); it != contextValues.constEnd(); ++it)
            values[mode].insert(it.key(), it.value().toString());
    }
}

void writeShortcutStringListJson(const QString &path, const QString &metadataKey,
                                 const QMap<ViewMode, QStringList> &values) {
    QJsonObject root;
    QFile existing(path);
    if(existing.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(existing.readAll()).object();
        existing.close();
    }

    QJsonObject metadata;
    for(auto ctx = values.cbegin(); ctx != values.cend(); ++ctx) {
        QJsonArray actions;
        QStringList names = ctx.value();
        names.removeDuplicates();
        names.sort(Qt::CaseInsensitive);
        for(const QString &name : names) {
            if(!name.isEmpty())
                actions.append(name);
        }
        metadata.insert(shortcutContextToken(ctx.key()), actions);
    }
    root.insert(metadataKey, metadata);

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if(file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
}

void readShortcutStringListJson(const QString &path, const QString &metadataKey,
                                QMap<ViewMode, QStringList> &values) {
    values.clear();
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object()
        .value(metadataKey).toObject();
    for(auto ctx = metadata.constBegin(); ctx != metadata.constEnd(); ++ctx) {
        if(!isShortcutContextToken(ctx.key()))
            continue;
        QStringList actions;
        const QJsonArray arr = ctx.value().toArray();
        for(const QJsonValue &value : arr) {
            const QString action = value.toString();
            if(!action.isEmpty())
                actions.append(action);
        }
        actions.removeDuplicates();
        values[shortcutContextFromToken(ctx.key())] = actions;
    }
}

// DEPRECATED (remove after next release): one-time "[Controls] shortcuts=" -> shortcuts.json migration.
// Context-less entries seed both contexts; "eq" decodes to "=".
void migrateLegacyShortcuts(QSettings *settingsConf, const QString &jsonPath) {
    if(QFile::exists(jsonPath))
        return;
    settingsConf->beginGroup("Controls");
    if(!settingsConf->contains("shortcuts")) {
        settingsConf->endGroup();
        return;
    }
    const QStringList in = settingsConf->value("shortcuts").toStringList();
    QMap<ViewMode, QMap<QString, QString>> shortcuts;
    for(int i = 0; i < in.count(); i++) {
        QString entry = in[i];
        bool legacy = !entry.contains('|');
        QString ctxName;
        if(!legacy) {
            ctxName = entry.section('|', 0, 0);
            entry = entry.section('|', 1);
        }
        QStringList pair = entry.split("=");
        if(pair.size() < 2 || pair[0].isEmpty() || pair[1].isEmpty())
            continue;
        if(pair[1].endsWith("eq"))
            pair[1] = pair[1].chopped(2) + "=";
        if(legacy) {
            shortcuts[MODE_GLOBAL].insert(pair[1], pair[0]);
        } else {
            shortcuts[shortcutContextFromToken(ctxName)].insert(pair[1], pair[0]);
        }
    }
    settingsConf->remove("shortcuts");
    settingsConf->endGroup();
    writeShortcutsJson(jsonPath, shortcuts);
}

}

Settings::Settings(QObject *parent)
    : QObject(parent),
      settingsConf(nullptr),
      themeConf(nullptr),
      mTmpDir(nullptr),
      mThumbCacheDir(nullptr) {
    settingsConf = PlatformDesktop::createSettingsConfig();
    const QString confDir = PlatformDesktop::settingsConfigDirectory(settingsConf);
    themeConf = PlatformDesktop::createThemeConfig(confDir);
    mShortcutsJsonPath = PlatformDesktop::shortcutsJsonPath(confDir);
    const QVersionNumber lastVer(
        settingsConf->value(groupedKey(QLatin1String("lastVerMajor")), 0).toInt(),
        settingsConf->value(groupedKey(QLatin1String("lastVerMinor")), 0).toInt(),
        settingsConf->value(groupedKey(QLatin1String("lastVerMicro")), 0).toInt());
    const bool isFirstRun = settingsConf->value(groupedKey(QLatin1String("firstRun")), true).toBool();
    const bool hasExistingSettings = !settingsConf->allKeys().isEmpty();
    runConfigRecoveryMigrations(confDir);
    if((!isFirstRun || hasExistingSettings) && lastVer < appVersion)
        runVersionedSettingsMigrations(lastVer);
    fillVideoFormats();
}
//------------------------------------------------------------------------------
Settings::~Settings() {
    saveTheme();
    delete mThumbCacheDir;
    delete mTmpDir;
    delete settingsConf;
    delete themeConf;
}
//------------------------------------------------------------------------------
Settings *Settings::getInstance() {
    if(!settings) {
        settings = new Settings();
        settings->setupCache();
        settings->loadTheme();
    }
    return settings;
}
//------------------------------------------------------------------------------
void Settings::setupCache() {
    const QString defaultCacheLocation = PlatformDesktop::defaultCacheDirectory();
    QString cacheLocation = defaultCacheLocation;
    if(PlatformDesktop::cacheDirectoryIsConfigurable())
        cacheLocation = settings->readSetting("cacheDir", defaultCacheLocation).toString();
    mTmpDir = new QDir(cacheLocation);
    mTmpDir->mkpath(mTmpDir->absolutePath());
    QFileInfo dirTest(mTmpDir->absolutePath());
    if(!dirTest.isDir() || !dirTest.isWritable() || !dirTest.exists()) {
        // fallback
        qCWarning(logSettings) << "Error: cache dir is not writable" << mTmpDir->absolutePath();
        qCWarning(logSettings) << "Trying to use" << defaultCacheLocation << "instead";
        mTmpDir->setPath(defaultCacheLocation);
        mTmpDir->mkpath(mTmpDir->absolutePath());
    }
    mThumbCacheDir = new QDir(PlatformDesktop::thumbnailCacheDirectory(mTmpDir->absolutePath()));
    mThumbCacheDir->mkpath(mThumbCacheDir->absolutePath());

    // Sweep "videothumb-*" / "export-*" dirs stranded by a crash. There is no single-instance guard, so
    // skip dirs younger than 5 minutes (frame grabs) or whose lock a live session holds (exports).
    QDir cacheRootDir(mTmpDir->absolutePath());
    const QStringList scratchDirs = cacheRootDir.entryList(
        {QStringLiteral("videothumb-*"), QStringLiteral("export-*")},
        QDir::Dirs | QDir::NoDotAndDotDot);
    const QDateTime staleBefore = QDateTime::currentDateTime().addSecs(-300);
    for(const QString& name : scratchDirs) {
        const QString dirPath = cacheRootDir.filePath(name);
        if(QFileInfo(dirPath).lastModified() > staleBefore)
            continue; // may still be in flight
        QLockFile lock(dirPath + QStringLiteral("/.tg-lock"));
        lock.setStaleLockTime(0);
        if(!lock.tryLock(0))
            continue; // a live session owns this one
        lock.unlock();
        QDir(dirPath).removeRecursively();
    }
}
//------------------------------------------------------------------------------
void Settings::sync() {
    settings->settingsConf->sync();
}
//------------------------------------------------------------------------------
QVariant Settings::readSetting(const QString &key, const QVariant &defaultValue) const {
    return settingsConf->value(groupedKey(key), defaultValue);
}

void Settings::writeSetting(const QString &key, const QVariant &value) {
    settingsConf->setValue(groupedKey(key), value);
}

void Settings::runVersionedSettingsMigrations(const QVersionNumber &lastVer) {
    if(lastVer < QVersionNumber(2026, 7, 7))
        migrateConfigGroups();
    // re-run whenever a key joins the retired list (pruning is idempotent)
    if(lastVer < QVersionNumber(2026, 10, 3))
        pruneRetiredSettings();
}

// drops retired keys, both grouped and flat spellings
void Settings::pruneRetiredSettings() {
    static const QHash<QString, QString> retiredKeys = {
        // the crop panel lost its "Crop & Save" button
        {QStringLiteral("defaultCropAction"), QStringLiteral("Document")},
        // the changelog window was removed
        {QStringLiteral("showChangelogs"), QStringLiteral("General")},
    };
    bool removed = false;
    for(auto it = retiredKeys.constBegin(); it != retiredKeys.constEnd(); ++it) {
        for(const QString &key : {it.value() + QLatin1Char('/') + it.key(), it.key()}) {
            if(!settingsConf->contains(key))
                continue;
            settingsConf->remove(key);
            removed = true;
        }
    }
    if(removed)
        settingsConf->sync();
}

void Settings::runConfigRecoveryMigrations(const QString &confDir) {
    const bool hasThemePointer = settingsConf->contains(QLatin1String("theme"));
    migrateLegacyThemeConf(confDir, themeConf);
    if(!hasThemePointer && themeConf->contains(QLatin1String("Theme/tid"))) {
        const int tid = normalizedThemeTid(themeConf->value(QLatin1String("Theme/tid")).toInt());
        settingsConf->setValue(QLatin1String("theme"), themeToken(tid));
    }
    themeConf->remove(QLatin1String("Theme"));

    const QString presetKey = groupedKey(QStringLiteral("preset"));
    if(!settingsConf->contains(presetKey))
        settingsConf->setValue(presetKey, QStringLiteral("qimgv"));

    // import the old savedState file into [State]; existing keys win
    QSettings *legacyState = PlatformDesktop::createLegacyStateConfig();
    const QString legacyStatePath = legacyState->fileName();
    if(QFile::exists(legacyStatePath)) {
        const QStringList stateKeys = legacyState->allKeys();
        for(const QString &key : stateKeys) {
            const QString target = QLatin1String("State/") + key;
            if(!settingsConf->contains(target))
                settingsConf->setValue(target, legacyState->value(key));
        }
    }
    delete legacyState;
    QFile::remove(legacyStatePath);

    // explode the old State/duplicateFinder blob into per-value keys; existing keys win
    const QString dupeBlobKey = QStringLiteral("State/duplicateFinder");
    if(settingsConf->contains(dupeBlobKey)) {
        static const QHash<QString, QString> blobKeyMap = {
            {QStringLiteral("geometry"),   QStringLiteral("State/duplicateFinderGeometry")},
            {QStringLiteral("header"),     QStringLiteral("State/duplicateFinderHeader")},
            {QStringLiteral("similarity"), QStringLiteral("State/duplicateFinderSimilarity")},
            {QStringLiteral("recursive"),  QStringLiteral("State/duplicateFinderRecursive")},
            {QStringLiteral("rotated"),    QStringLiteral("State/duplicateFinderRotated")},
            {QStringLiteral("mirrored"),   QStringLiteral("State/duplicateFinderMirrored")},
            {QStringLiteral("targets"),    QStringLiteral("State/duplicateFinderTargets")},
        };
        const QVariantMap blob = settingsConf->value(dupeBlobKey).toMap();
        for(auto it = blob.constBegin(); it != blob.constEnd(); ++it) {
            const QString target = blobKeyMap.value(it.key());
            if(!target.isEmpty() && !settingsConf->contains(target))
                settingsConf->setValue(target, it.value());
        }
        settingsConf->remove(dupeBlobKey);
    }
}
//------------------------------------------------------------------------------
int Settings::selectedThemeTid() {
    return themeTidFromToken(readSetting("theme", themeToken(COLORS_CUSTOM)).toString());
}

void Settings::setSelectedThemeTid(int tid) {
    writeSetting("theme", themeToken(tid));
}
//------------------------------------------------------------------------------
void Settings::migrateConfigGroups() {
    // old "useSystemColorScheme" / theme.ini tid -> [General]/theme
    if(!settingsConf->contains(QLatin1String("theme"))) {
        int tid = COLORS_CUSTOM;
        if(settingsConf->value(QLatin1String("useSystemColorScheme"), false).toBool())
            tid = COLORS_SYSTEM;
        else if(themeConf->contains(QLatin1String("Theme/tid")))
            tid = normalizedThemeTid(themeConf->value(QLatin1String("Theme/tid")).toInt());
        settingsConf->setValue(QLatin1String("theme"), themeToken(tid));
    }
    settingsConf->remove(QLatin1String("useSystemColorScheme"));
    // The selection pointer no longer lives in the theme file.
    themeConf->remove(QLatin1String("Theme"));

    bool moved = false;

    // move legacy folder-view colors into theme.ini and drop orphaned *Color keys from [General]
    static const QHash<QString, QString> legacyColorKeys = {
        {QStringLiteral("folderViewLabelBackgroundColor"),         QStringLiteral("fv_label_bg")},
        {QStringLiteral("folderViewSelectionColor"),               QStringLiteral("fv_selection")},
        {QStringLiteral("folderViewParentIconColor"),              QStringLiteral("fv_parent_icon")},
        {QStringLiteral("folderViewSelectedLabelBackgroundColor"), QStringLiteral("fv_sel_label_bg")},
        {QStringLiteral("folderViewCellBackgroundColor"),          QStringLiteral("fv_cell_bg")},
    };
    const QStringList rootKeys = settingsConf->childKeys();
    for(const QString &key : rootKeys) {
        if(!key.startsWith(QLatin1String("folderView")) || !key.endsWith(QLatin1String("Color")))
            continue;
        const QString target = legacyColorKeys.value(key);
        if(!target.isEmpty()) {
            const QString themeKey = QLatin1String("Colors/") + target;
            const QColor color(settingsConf->value(key).toString());
            if(color.isValid() && !themeConf->contains(themeKey))
                themeConf->setValue(themeKey, color.name());
        }
        settingsConf->remove(key);
        moved = true;
    }

    // move flat keys into their group (idempotent)
    const QStringList flatKeys = settingsConf->childKeys();
    for(const QString &key : flatKeys) {
        const QString group = settingGroupFor(key);
        if(group.isEmpty() || group == QLatin1String("General"))
            continue;
        const QVariant value = settingsConf->value(key);
        settingsConf->remove(key);
        settingsConf->setValue(group + QLatin1Char('/') + key, value);
        moved = true;
    }
    if(moved)
        settingsConf->sync();
}
//------------------------------------------------------------------------------
QString Settings::thumbnailCacheDir() {
    return mThumbCacheDir->path() + "/";
}
//------------------------------------------------------------------------------
QString Settings::tmpDir() {
    return mTmpDir->path() + "/";
}
//------------------------------------------------------------------------------
// fills the size placeholders, scaled from the application font
static void replaceStylesheetMetrics(QString &styleSheet) {
    // --- widget sizes ---------------------------------------------
    auto fnt = QGuiApplication::font();
    QFontMetrics fm(fnt);
    // todo: use precise values for ~9-11 point sizes
    int font_small = qMax((int)(fnt.pointSize() * 0.9f), 8);
    int font_large = (int)(fnt.pointSize() * 1.8f);
    int text_height = fm.height();
    int text_padding = (int)(text_height * 0.10f);
    int text_padding_small = (int)(text_height * 0.05f);
    int text_padding_large = (int)(text_height * 0.25f);

    // folderview top panel item sizes
    int top_panel_v_margin = 4;
    // ensure at least 4px so its not too thin
    int top_panel_text_padding = qMax(text_padding, 4);
    // scale with font, 38px base size
    int top_panel_height = qMax((text_height + top_panel_text_padding * 2 + top_panel_v_margin * 2), 38);

    // overlay headers
    int overlay_header_margin = 2;
    // 32px base size
    int overlay_header_size = qMax(text_height + text_padding * 2, 30);

    // todo
    int button_height = text_height + text_padding_large * 2;
    // tight ~1px margin around the text; the floor keeps descenders from clipping
    int combobox_height = qMax(text_height + 2, 18);

    // pseudo-dpi to scale some widget widths
    int text_height_base = 22;
    qreal pDpr = qMax( ((qreal)(text_height) / text_height_base), 1.0);
    int context_menu_width = 212 * pDpr;
    int context_menu_button_height = 32 * pDpr;
    int rename_overlay_width = 380 * pDpr;

    styleSheet.replace("%font_small%", QString::number(font_small)+"pt");
    styleSheet.replace("%font_large%", QString::number(font_large)+"pt");
    styleSheet.replace("%button_height%", QString::number(button_height)+"px");
    styleSheet.replace("%combobox_height%", QString::number(combobox_height)+"px");
    styleSheet.replace("%top_panel_height%", QString::number(top_panel_height)+"px");
    styleSheet.replace("%overlay_header_size%", QString::number(overlay_header_size)+"px");
    styleSheet.replace("%context_menu_width%", QString::number(context_menu_width)+"px");
    styleSheet.replace("%context_menu_button_height%", QString::number(context_menu_button_height)+"px");
    styleSheet.replace("%rename_overlay_width%", QString::number(rename_overlay_width)+"px");
    styleSheet.replace("%contextmenu_border_radius%",  PlatformDesktop::contextMenuBorderRadius());
}

// Fills the color placeholders from the active color scheme.
static void replaceStylesheetColors(QString &styleSheet, const ColorScheme &colors) {
    // panel tint from the active scheme so it follows the selected theme
    QColor sys_text = colors.text;
    QColor sys_window = colors.widget;
    QColor sys_window_tinted, sys_window_tinted_lc, sys_window_tinted_lc2, sys_window_tinted_hc, sys_window_tinted_hc2;
    if(sys_window.valueF() <= 0.45f) {
        // dark system theme
        sys_window_tinted_lc2.setHsv(sys_window.hue(), sys_window.saturation(), sys_window.value() + 6);
        sys_window_tinted_lc.setHsv(sys_window.hue(),  sys_window.saturation(), sys_window.value() + 14);
        sys_window_tinted.setHsv(sys_window.hue(),     sys_window.saturation(), sys_window.value() + 20);
        sys_window_tinted_hc.setHsv(sys_window.hue(),  sys_window.saturation(), sys_window.value() + 35);
        sys_window_tinted_hc2.setHsv(sys_window.hue(), sys_window.saturation(), sys_window.value() + 50);
    } else {
        // light system theme
        sys_window_tinted_lc2.setHsv(sys_window.hue(), sys_window.saturation(), sys_window.value() - 6);
        sys_window_tinted_lc.setHsv(sys_window.hue(),  sys_window.saturation(), sys_window.value() - 14);
        sys_window_tinted.setHsv(sys_window.hue(),     sys_window.saturation(), sys_window.value() - 20);
        sys_window_tinted_hc.setHsv(sys_window.hue(),  sys_window.saturation(), sys_window.value() - 35);
        sys_window_tinted_hc2.setHsv(sys_window.hue(), sys_window.saturation(), sys_window.value() - 50);
    }

    // icon variant follows the app theme's background, not the OS palette
    styleSheet.replace("%icontheme%",  colors.background.valueF() <= 0.45f ? "dark" : "light");
    styleSheet.replace("%sys_window%",    sys_window.name());
    styleSheet.replace("%sys_window_tinted%",    sys_window_tinted.name());
    styleSheet.replace("%sys_window_tinted_lc%", sys_window_tinted_lc.name());
    styleSheet.replace("%sys_window_tinted_lc2%", sys_window_tinted_lc2.name());
    styleSheet.replace("%sys_window_tinted_hc%", sys_window_tinted_hc.name());
    styleSheet.replace("%sys_window_tinted_hc2%", sys_window_tinted_hc2.name());
    styleSheet.replace("%sys_text_secondary_rgba%", "rgba(" + QString::number(sys_text.red())   + ","
                                                  + QString::number(sys_text.green()) + ","
                                                  + QString::number(sys_text.blue())  + ",50%)");

    // danger red: fixed hue, value per theme lightness for contrast
    QColor danger, danger_hover, danger_pressed, danger_text;
    if(colors.background.valueF() <= 0.45f) {
        danger.setHsv(357, 148, 132);
        danger_hover.setHsv(357, 152, 158);
        danger_pressed.setHsv(357, 158, 112);
        danger_text = QColor(240, 232, 232);
    } else {
        danger.setHsv(357, 172, 196);
        danger_hover.setHsv(357, 176, 216);
        danger_pressed.setHsv(357, 180, 170);
        danger_text = QColor(255, 255, 255);
    }
    styleSheet.replace("%danger%",               danger.name());
    styleSheet.replace("%danger_hover%",         danger_hover.name());
    styleSheet.replace("%danger_pressed%",       danger_pressed.name());
    styleSheet.replace("%danger_text%",          danger_text.name());
    // disabled state: keep the shape, drop the alarm
    styleSheet.replace("%danger_disabled_rgba%", "rgba(" + QString::number(danger.red())   + ","
                                                         + QString::number(danger.green()) + ","
                                                         + QString::number(danger.blue())  + ",30%)");

    styleSheet.replace("%button%",               colors.button.name());
    styleSheet.replace("%button_hover%",         colors.button_hover.name());
    styleSheet.replace("%button_pressed%",       colors.button_pressed.name());
    styleSheet.replace("%panel_button%",         colors.panel_button.name());
    styleSheet.replace("%panel_button_hover%",   colors.panel_button_hover.name());
    styleSheet.replace("%panel_button_pressed%", colors.panel_button_pressed.name());
    styleSheet.replace("%background%",           colors.background.name());
    styleSheet.replace("%widget%",               colors.widget.name());
    styleSheet.replace("%widget_border%",        colors.widget_border.name());
    styleSheet.replace("%folderview%",           colors.folderview.name());
    styleSheet.replace("%folderview_topbar%",    colors.folderview_topbar.name());
    styleSheet.replace("%folderview_hc%",        colors.folderview_hc.name());
    styleSheet.replace("%folderview_hc2%",       colors.folderview_hc2.name());
    styleSheet.replace("%accent%",               colors.accent.name());
    styleSheet.replace("%input_field_focus%",    colors.input_field_focus.name());
    styleSheet.replace("%overlay%",              colors.overlay.name());
    styleSheet.replace("%icons%",                colors.icons.name());
    styleSheet.replace("%text_hc2%",             colors.text_hc2.name());
    styleSheet.replace("%text_hc%",              colors.text_hc.name());
    styleSheet.replace("%text%",                 colors.text.name());
    styleSheet.replace("%overlay_text%",         colors.overlay_text.name());
    styleSheet.replace("%text_lc%",              colors.text_lc.name());
    styleSheet.replace("%text_lc2%",             colors.text_lc2.name());
    styleSheet.replace("%scrollbar%",            colors.scrollbar.name());
    styleSheet.replace("%scrollbar_hover%",      colors.scrollbar_hover.name());
    styleSheet.replace("%slider_groove%",        colors.text_lc2.name());
    styleSheet.replace("%slider_handle%",        colors.text_hc2.name());
    styleSheet.replace("%folderview_button_hover%",   colors.folderview_button_hover.name());
    styleSheet.replace("%folderview_button_pressed%", colors.folderview_button_pressed.name());
    styleSheet.replace("%table_bg%",             colors.table_bg.name());
    styleSheet.replace("%table_bg_alt%",         colors.table_bg_alt.name());
    styleSheet.replace("%table_header%",         colors.table_header.name());
    styleSheet.replace("%table_text%",           colors.table_text.name());
    styleSheet.replace("%table_border%",         colors.table_border.name());
    styleSheet.replace("%text_secondary_rgba%",  "rgba(" + QString::number(colors.text.red())   + ","
                                                         + QString::number(colors.text.green()) + ","
                                                         + QString::number(colors.text.blue())  + ",62%)");
    styleSheet.replace("%accent_hover_rgba%",    "rgba(" + QString::number(colors.accent.red())   + ","
                                                         + QString::number(colors.accent.green()) + ","
                                                         + QString::number(colors.accent.blue())  + ",65%)");
    styleSheet.replace("%overlay_rgba%",         "rgba(" + QString::number(colors.overlay.red())   + ","
                                                         + QString::number(colors.overlay.green()) + ","
                                                         + QString::number(colors.overlay.blue())  + ",90%)");
    styleSheet.replace("%fv_backdrop_rgba%",     "rgba(" + QString::number(colors.folderview_hc2.red())   + ","
                                                         + QString::number(colors.folderview_hc2.green()) + ","
                                                         + QString::number(colors.folderview_hc2.blue())  + ",80%)");
    // do not show separator line if topbar color matches folderview
    if(colors.folderview != colors.folderview_topbar)
        styleSheet.replace("%topbar_border_rgba%", "rgba(0,0,0,14%)");
    else
        styleSheet.replace("%topbar_border_rgba%", colors.folderview.name());
}

void Settings::loadStylesheet() {
    // stylesheet template file
    QFile file(":/res/styles/style-template.qss");
    if(!file.open(QFile::ReadOnly))
        return;
    QString styleSheet = QLatin1String(file.readAll());
    replaceStylesheetMetrics(styleSheet);
    replaceStylesheetColors(styleSheet, settings->colorScheme());
    qApp->setStyleSheet(styleSheet);
}
//------------------------------------------------------------------------------
void Settings::loadTheme() {
    // [General]/theme selects; colors come from the preset or the custom palette
    switch(selectedThemeTid()) {
        case COLORS_SYSTEM:
            setColorScheme(ThemeStore::colorScheme(ColorSchemes::COLORS_SYSTEM));
            break;
        case COLORS_BLACK:
        case COLORS_DARK:
        case COLORS_DARKBLUE:
        case COLORS_LIGHT:
        case COLORS_LIGHT_YELLOW:
            // presets are generated, never read from the stored custom palette
            setColorScheme(ThemeStore::colorScheme(static_cast<ColorSchemes>(selectedThemeTid())));
            break;
        default:
            setColorScheme(customColorScheme());
            break;
    }
}
//------------------------------------------------------------------------------
ColorScheme Settings::customColorScheme() {
    BaseColorScheme base;
    const int tid = COLORS_CUSTOM;
    themeConf->beginGroup("Colors");
        base.background            = QColor(themeConf->value("background",            "#1a1a1a").toString());
        base.background_fullscreen = QColor(themeConf->value("background_fullscreen", "#1a1a1a").toString());
        base.text                  = QColor(themeConf->value("text",                  "#b6b6b6").toString());
        base.icons                 = QColor(themeConf->value("icons",                 "#a4a4a4").toString());
        base.widget                = QColor(themeConf->value("widget",                "#252525").toString());
        base.widget_border         = QColor(themeConf->value("widget_border",         "#2c2c2c").toString());
        base.accent                = QColor(themeConf->value("accent",                "#8c9b81").toString());
        base.folderview            = QColor(themeConf->value("folderview",            "#242424").toString());
        base.folderview_topbar     = QColor(themeConf->value("folderview_topbar",     "#383838").toString());
        base.scrollbar             = QColor(themeConf->value("scrollbar",             "#5a5a5a").toString());
        base.overlay_text          = QColor(themeConf->value("overlay_text",          "#d2d2d2").toString());
        base.overlay               = QColor(themeConf->value("overlay",               "#1a1a1a").toString());
        base.tid                   = tid;
        ColorScheme defaults(base);
        base.folderview_label_bg = colorFromSettings(themeConf, "fv_label_bg",
                                                     settingsConf, "folderViewLabelBackgroundColor",
                                                     defaults.folderview_label_bg);
        base.folderview_selection = colorFromSettings(themeConf, "fv_selection",
                                                      settingsConf, "folderViewSelectionColor",
                                                      defaults.folderview_selection);
        base.folderview_parent_icon = colorFromSettings(themeConf, "fv_parent_icon",
                                                        settingsConf, "folderViewParentIconColor",
                                                        defaults.folderview_parent_icon);
        base.folderview_selected_label_bg = colorFromSettings(themeConf, "fv_sel_label_bg",
                                                              settingsConf, "folderViewSelectedLabelBackgroundColor",
                                                              base.folderview_label_bg);
        base.folderview_cell_bg = colorFromSettings(themeConf, "fv_cell_bg",
                                                    settingsConf, "folderViewCellBackgroundColor",
                                                    defaults.folderview_cell_bg);
    themeConf->endGroup();
    return ColorScheme(base);
}
void Settings::saveTheme() {
    setSelectedThemeTid(mColorScheme.tid);
    // only the custom theme persists colors; presets must not overwrite the custom palette
    if(mColorScheme.tid == COLORS_SYSTEM ||
       mColorScheme.tid == COLORS_BLACK || mColorScheme.tid == COLORS_DARK ||
       mColorScheme.tid == COLORS_DARKBLUE || mColorScheme.tid == COLORS_LIGHT ||
       mColorScheme.tid == COLORS_LIGHT_YELLOW) {
        return;
    }
    themeConf->beginGroup("Colors");
    themeConf->setValue("background",            mColorScheme.background.name());
    themeConf->setValue("background_fullscreen", mColorScheme.background_fullscreen.name());
    themeConf->setValue("text",                  mColorScheme.text.name());
    themeConf->setValue("icons",                 mColorScheme.icons.name());
    themeConf->setValue("widget",                mColorScheme.widget.name());
    themeConf->setValue("widget_border",         mColorScheme.widget_border.name());
    themeConf->setValue("accent",                mColorScheme.accent.name());
    themeConf->setValue("folderview",            mColorScheme.folderview.name());
    themeConf->setValue("folderview_topbar",     mColorScheme.folderview_topbar.name());
    themeConf->setValue("fv_label_bg",           mColorScheme.folderview_label_bg.name());
    themeConf->setValue("fv_selection",          mColorScheme.folderview_selection.name());
    themeConf->setValue("fv_parent_icon",        mColorScheme.folderview_parent_icon.name());
    themeConf->setValue("fv_sel_label_bg",       mColorScheme.folderview_selected_label_bg.name());
    themeConf->setValue("fv_cell_bg",            mColorScheme.folderview_cell_bg.name());
    themeConf->setValue("scrollbar",             mColorScheme.scrollbar.name());
    themeConf->setValue("overlay_text",          mColorScheme.overlay_text.name());
    themeConf->setValue("overlay",               mColorScheme.overlay.name());
    themeConf->endGroup();
}
//------------------------------------------------------------------------------
const ColorScheme& Settings::colorScheme() {
    return mColorScheme;
}
//------------------------------------------------------------------------------
void Settings::setColorScheme(ColorScheme scheme) {
    mColorScheme = scheme;
    loadStylesheet();
}
//------------------------------------------------------------------------------
void Settings::fillVideoFormats() {
    mVideoFormatsMap.insert("video/webm",       "webm");
    mVideoFormatsMap.insert("video/mp4",        "mp4");
    mVideoFormatsMap.insert("video/mp4",        "m4v");
    mVideoFormatsMap.insert("video/mpeg",       "mpg");
    mVideoFormatsMap.insert("video/mpeg",       "mpeg");
    mVideoFormatsMap.insert("video/x-matroska", "mkv");
    mVideoFormatsMap.insert("video/x-ms-wmv",   "wmv");
    mVideoFormatsMap.insert("video/x-msvideo",  "avi");
    mVideoFormatsMap.insert("video/quicktime",  "mov");
    mVideoFormatsMap.insert("video/x-flv",      "flv");
    // Audio played through the same mpv player (no video track, sound only).
    mVideoFormatsMap.insert("audio/mpeg",       "mp3");
}
//------------------------------------------------------------------------------
QString Settings::mpvBinary() {
    QString mpvPath = settings->readSetting("mpvBinary", "").toString();
    if(!QFile::exists(mpvPath)) {
        mpvPath = PlatformDesktop::defaultMpvBinary();
        if(!QFile::exists(mpvPath))
            mpvPath = "";
    }
    return mpvPath;
}

void Settings::setMpvBinary(const QString& path) {
    if(QFile::exists(path)) {
        settings->writeSetting("mpvBinary", path);
    }
}
//------------------------------------------------------------------------------
QList<QByteArray> Settings::supportedFormats() {
    auto formats = QImageReader::supportedImageFormats();
    formats << "jfif";
    if(videoPlayback())
        formats << mVideoFormatsMap.values();
    formats.removeAll("pdf");
    return formats;
}
//------------------------------------------------------------------------------
// for open/save dialogs: "Images (*.jpg, *.png)"
QString Settings::supportedFormatsFilter() {
    QString filters;
    auto formats = supportedFormats();
    filters.append("Supported files (");
    for(int i = 0; i < formats.count(); i++)
        filters.append("*." + QString(formats.at(i)) + " ");
    filters.append(")");
    return filters;
}
//------------------------------------------------------------------------------
QString Settings::supportedFormatsRegex() {
    QString filter;
    QList<QByteArray> formats = supportedFormats();
    filter.append(".*\\.(");
    for(int i = 0; i < formats.count(); i++)
        filter.append(QString(formats.at(i)) + "|");
    filter.chop(1);
    filter.append(")$");
    return filter;
}
//------------------------------------------------------------------------------
QStringList Settings::supportedMimeTypes() {
    QStringList filters;
    QList<QByteArray> mimeTypes = QImageReader::supportedMimeTypes();
    if(videoPlayback())
        mimeTypes << mVideoFormatsMap.keys();
    for(int i = 0; i < mimeTypes.count(); i++) {
        filters << QString(mimeTypes.at(i));
    }
    return filters;
}
//------------------------------------------------------------------------------
bool Settings::videoPlayback() {
#ifdef USE_MPV
    return settings->readSetting("videoPlayback", true).toBool();
#else
    return false;
#endif
}

void Settings::setVideoPlayback(bool mode) {
    settings->writeSetting("videoPlayback", mode);
}
//------------------------------------------------------------------------------
QVersionNumber Settings::lastVersion() {
    int vmajor = settings->readSetting("lastVerMajor", 0).toInt();
    int vminor = settings->readSetting("lastVerMinor", 0).toInt();
    int vmicro = settings->readSetting("lastVerMicro", 0).toInt();
    return QVersionNumber(vmajor, vminor, vmicro);
}

void Settings::setLastVersion(QVersionNumber &ver) {
    settings->writeSetting("lastVerMajor", ver.majorVersion());
    settings->writeSetting("lastVerMinor", ver.minorVersion());
    settings->writeSetting("lastVerMicro", ver.microVersion());
}
//------------------------------------------------------------------------------
qreal Settings::backgroundOpacity() {
    bool ok = false;
    qreal value = settings->readSetting("backgroundOpacity", 1.0).toReal(&ok);
    if(!ok)
        return 0.0;
    if(value > 1.0)
        return 1.0;
    if(value < 0.0)
        return 0.0;
    return value;
}

void Settings::setBackgroundOpacity(qreal value) {
    if(value > 1.0)
        value = 1.0;
    else if(value < 0.0)
        value = 0.0;
    settings->writeSetting("backgroundOpacity", value);
}
//------------------------------------------------------------------------------
bool Settings::blurBackground() {
#ifndef USE_KDE_BLUR
    return false;
#endif
    return settings->readSetting("blurBackground", false).toBool();
}

void Settings::setBlurBackground(bool mode) {
    settings->writeSetting("blurBackground", mode);
}
//------------------------------------------------------------------------------
void Settings::setSortingMode(SortingMode mode) {
    if(mode >= 6)
        mode = SortingMode::SORT_NAME;
    settings->writeSetting("sortingMode", mode);
}

SortingMode Settings::sortingMode() {
    int mode = settings->readSetting("sortingMode", 0).toInt();
    if(mode < 0 || mode >= 6)
        mode = 0;
    return static_cast<SortingMode>(mode);
}
//------------------------------------------------------------------------------
bool Settings::playVideoSounds() {
    return settings->readSetting("playVideoSounds", false).toBool();
}

void Settings::setPlayVideoSounds(bool mode) {
    settings->writeSetting("playVideoSounds", mode);
}
//------------------------------------------------------------------------------
bool Settings::showVideoControls() {
    return settings->readSetting("showVideoControls", true).toBool();
}

void Settings::setShowVideoControls(bool mode) {
    settings->writeSetting("showVideoControls", mode);
}
//------------------------------------------------------------------------------
void Settings::setVolume(int vol) {
    settings->writeSetting("volume", vol);
}

int Settings::volume() {
    return settings->readSetting("volume", 100).toInt();
}
//------------------------------------------------------------------------------
FolderViewMode Settings::folderViewMode() {
    int mode = settings->readSetting("folderViewMode", 2).toInt();
    if(mode < 0 || mode >= 3)
        mode = 2;
    return static_cast<FolderViewMode>(mode);
}

void Settings::setFolderViewMode(FolderViewMode mode) {
    settings->writeSetting("folderViewMode", mode);
}
//------------------------------------------------------------------------------
bool Settings::folderViewShowInfo() {
    return settings->readSetting("folderViewShowInfo", false).toBool();
}

void Settings::setFolderViewShowInfo(bool mode) {
    settings->writeSetting("folderViewShowInfo", mode);
}
//------------------------------------------------------------------------------
// false = cover (crop child previews), true = contain (fit whole image)
bool Settings::folderViewPreviewFit() {
    return settings->readSetting("folderViewPreviewFit", true).toBool();
}

void Settings::setFolderViewPreviewFit(bool mode) {
    settings->writeSetting("folderViewPreviewFit", mode);
}
//------------------------------------------------------------------------------
ThumbPanelStyle Settings::thumbPanelStyle() {
    int mode = settings->readSetting("thumbPanelStyle", 1).toInt();
    if(mode < 0 || mode > 1)
        mode = 1;
    return static_cast<ThumbPanelStyle>(mode);
}

void Settings::setThumbPanelStyle(ThumbPanelStyle mode) {
    settings->writeSetting("thumbPanelStyle", mode);
}
//------------------------------------------------------------------------------
const QMultiMap<QByteArray, QByteArray> Settings::videoFormats() const {
    return mVideoFormatsMap;
}
//------------------------------------------------------------------------------
int Settings::panelPreviewsSize() {
    bool ok = true;
    int size = settings->readSetting("panelPreviewsSize", 140).toInt(&ok);
    if(!ok)
        size = 140;
    size = qBound(100, size, 250);
    return size;
}

void Settings::setPanelPreviewsSize(int size) {
    settings->writeSetting("panelPreviewsSize", size);
}
//------------------------------------------------------------------------------
bool Settings::usePreloader() {
    return settings->readSetting("usePreloader", true).toBool();
}

void Settings::setUsePreloader(bool mode) {
    settings->writeSetting("usePreloader", mode);
}
//------------------------------------------------------------------------------
bool Settings::keepFitMode() {
    return settings->readSetting("keepFitMode", false).toBool();
}

void Settings::setKeepFitMode(bool mode) {
    settings->writeSetting("keepFitMode", mode);
}
//------------------------------------------------------------------------------
bool Settings::fullscreenMode() {
    return settings->readSetting("openInFullscreen", false).toBool();
}

void Settings::setFullscreenMode(bool mode) {
    settings->writeSetting("openInFullscreen", mode);
}
//------------------------------------------------------------------------------
bool Settings::maximizedWindow() {
    return settings->readSetting("maximizedWindow", false).toBool();
}

void Settings::setMaximizedWindow(bool mode) {
    settings->writeSetting("maximizedWindow", mode);
}
//------------------------------------------------------------------------------
bool Settings::panelEnabled() {
    return settings->readSetting("panelEnabled", true).toBool();
}

void Settings::setPanelEnabled(bool mode) {
    settings->writeSetting("panelEnabled", mode);
}
//------------------------------------------------------------------------------
bool Settings::panelFullscreenOnly() {
    return settings->readSetting("panelFullscreenOnly", true).toBool();
}

void Settings::setPanelFullscreenOnly(bool mode) {
    settings->writeSetting("panelFullscreenOnly", mode);
}
//------------------------------------------------------------------------------
int Settings::lastDisplay() {
    return settings->readSetting("lastDisplay", 0).toInt();
}

void Settings::setLastDisplay(int display) {
    settings->writeSetting("lastDisplay", display);
}
//------------------------------------------------------------------------------
PanelPosition Settings::panelPosition() {
    QString posString = settings->readSetting("panelPosition", "top").toString();
    if(posString == "top") {
        return PanelPosition::PANEL_TOP;
    } else if(posString == "bottom") {
        return PanelPosition::PANEL_BOTTOM;
    } else if(posString == "left") {
        return PanelPosition::PANEL_LEFT;
    } else {
        return PanelPosition::PANEL_RIGHT;
    }
}

void Settings::setPanelPosition(PanelPosition pos) {
    QString posString;
    switch(pos) {
        case PANEL_TOP:
            posString = "top";
            break;
        case PANEL_BOTTOM:
            posString = "bottom";
            break;
        case PANEL_LEFT:
            posString = "left";
            break;
        case PANEL_RIGHT:
            posString = "right";
            break;
    }
    settings->writeSetting("panelPosition", posString);
}
//------------------------------------------------------------------------------
bool Settings::panelPinned() {
    return settings->readSetting("panelPinned", false).toBool();
}

void Settings::setPanelPinned(bool mode) {
    settings->writeSetting("panelPinned", mode);
}
//------------------------------------------------------------------------------
/*
 * 0: fit window
 * 1: fit width
 * 2: orginal size
 * 3: fit window (stretch)
 */
ImageFitMode Settings::imageFitMode() {
    int mode = settings->readSetting("defaultFitMode", 0).toInt();
    if(mode < 0 || mode > 3) {
        qCWarning(logSettings) << "Settings: Invalid fit mode ( " + QString::number(mode) + " ). Resetting to default.";
        mode = 0;
    }
    return static_cast<ImageFitMode>(mode);
}

void Settings::setImageFitMode(ImageFitMode mode) {
    int modeInt = static_cast<ImageFitMode>(mode);
    if(modeInt < 0 || modeInt > 3) {
        qCWarning(logSettings) << "Settings: Invalid fit mode ( " + QString::number(modeInt) + " ). Resetting to default.";
        modeInt = 0;
    }
    settings->writeSetting("defaultFitMode", modeInt);
}
//------------------------------------------------------------------------------
QRect Settings::windowGeometry() {
    QRect savedRect = settings->readSetting("windowGeometry").toRect();
    if(savedRect.size().isEmpty())
        savedRect.setRect(100, 100, 900, 600);
    return savedRect;
}

void Settings::setWindowGeometry(QRect geometry) {
    settings->writeSetting("windowGeometry", geometry);
}
//------------------------------------------------------------------------------
bool Settings::loopSlideshow() {
    return settings->readSetting("loopSlideshow", false).toBool();
}

void Settings::setLoopSlideshow(bool mode) {
    settings->writeSetting("loopSlideshow", mode);
}
//------------------------------------------------------------------------------
void Settings::sendChangeNotification() {
    emit settingsChanged();
}
//------------------------------------------------------------------------------
// action -> sorted keys per context, in shortcuts.json
void Settings::readShortcuts(QMap<ViewMode, QMap<QString, QString>> &shortcuts) {
    migrateLegacyShortcuts(settings->settingsConf, settings->mShortcutsJsonPath);
    if(readShortcutsJson(settings->mShortcutsJsonPath, shortcuts))
        writeShortcutsJson(settings->mShortcutsJsonPath, shortcuts);
}

void Settings::saveShortcuts(const QMap<ViewMode, QMap<QString, QString>> &shortcuts) {
    writeShortcutsJson(settings->mShortcutsJsonPath, shortcuts);
}

bool Settings::collapseShortcutContexts(QMap<ViewMode, QMap<QString, QString>> &shortcuts) {
    return collapseGlobalShortcutDuplicates(shortcuts);
}
//------------------------------------------------------------------------------
QString Settings::selectedPreset() {
    return settings->readSetting("preset", QStringLiteral("qimgv")).toString();
}

void Settings::setSelectedPreset(const QString &id) {
    settings->writeSetting("preset", id);
}

bool Settings::shortcutsModified() {
    return settings->readSetting("modified", false).toBool();
}

void Settings::setShortcutsModified(bool modified) {
    settings->writeSetting("modified", modified);
}

bool Settings::shortcutsJsonExists() {
    return QFile::exists(settings->mShortcutsJsonPath);
}
//------------------------------------------------------------------------------
void Settings::readShortcutPrimary(QMap<ViewMode, QMap<QString, QString>> &primary) {
    readShortcutStringMapJson(settings->mShortcutsJsonPath, "_primary", primary);
}

void Settings::saveShortcutPrimary(const QMap<ViewMode, QMap<QString, QString>> &primary) {
    writeShortcutStringMapJson(settings->mShortcutsJsonPath, "_primary", primary);
}

// stored as "_fallback": { "document": { action: fallback } }
void Settings::readShortcutFallbacks(QMap<QString, QString> &fallbacks) {
    QMap<ViewMode, QMap<QString, QString>> values;
    readShortcutStringMapJson(settings->mShortcutsJsonPath, "_fallback", values);
    fallbacks = values.value(MODE_DOCUMENT);
}

void Settings::saveShortcutFallbacks(const QMap<QString, QString> &fallbacks) {
    QMap<ViewMode, QMap<QString, QString>> values;
    values.insert(MODE_DOCUMENT, fallbacks);
    writeShortcutStringMapJson(settings->mShortcutsJsonPath, "_fallback", values);
}

// "_videoFallback", shaped like "_fallback"
void Settings::readShortcutVideoFallbacks(QMap<QString, QString> &fallbacks) {
    QMap<ViewMode, QMap<QString, QString>> values;
    readShortcutStringMapJson(settings->mShortcutsJsonPath, "_videoFallback", values);
    fallbacks = values.value(MODE_DOCUMENT);
}

void Settings::saveShortcutVideoFallbacks(const QMap<QString, QString> &fallbacks) {
    QMap<ViewMode, QMap<QString, QString>> values;
    values.insert(MODE_DOCUMENT, fallbacks);
    writeShortcutStringMapJson(settings->mShortcutsJsonPath, "_videoFallback", values);
}

void Settings::readDisabledShortcuts(QMap<ViewMode, QStringList> &disabled) {
    readShortcutStringListJson(settings->mShortcutsJsonPath, "_disabled", disabled);
}

void Settings::saveDisabledShortcuts(const QMap<ViewMode, QStringList> &disabled) {
    writeShortcutStringListJson(settings->mShortcutsJsonPath, "_disabled", disabled);
}
//------------------------------------------------------------------------------
void Settings::readScripts(QMap<QString, Script> &scripts) {
    scripts.clear();
    settings->settingsConf->beginGroup("Scripts");
    int size = settings->settingsConf->beginReadArray("script");
    for(int i=0; i < size; i++) {
        settings->settingsConf->setArrayIndex(i);
        QString name = settings->settingsConf->value("name").toString();
        QVariant value = settings->settingsConf->value("value");
        Script scr = value.value<Script>();
        scripts.insert(name, scr);
    }
    settings->settingsConf->endArray();
    settings->settingsConf->endGroup();
}

void Settings::saveScripts(const QMap<QString, Script> &scripts) {
    settings->settingsConf->beginGroup("Scripts");
    settings->settingsConf->beginWriteArray("script");
    QMapIterator<QString, Script> i(scripts);
    int counter = 0;
    while(i.hasNext()) {
        i.next();
        settings->settingsConf->setArrayIndex(counter);
        settings->settingsConf->setValue("name", i.key());
        settings->settingsConf->setValue("value", QVariant::fromValue(i.value()));
        counter++;
    }
    settings->settingsConf->endArray();
    settings->settingsConf->endGroup();
}
//------------------------------------------------------------------------------
bool Settings::squareThumbnails() {
    return settings->readSetting("squareThumbnails", false).toBool();
}

void Settings::setSquareThumbnails(bool mode) {
    settings->writeSetting("squareThumbnails", mode);
}
//------------------------------------------------------------------------------
bool Settings::transparencyGrid() {
    return settings->readSetting("drawTransparencyGrid", false).toBool();
}

void Settings::setTransparencyGrid(bool mode) {
    settings->writeSetting("drawTransparencyGrid", mode);
}
//------------------------------------------------------------------------------
bool Settings::enableSmoothScroll() {
    return settings->readSetting("enableSmoothScroll", true).toBool();
}

void Settings::setEnableSmoothScroll(bool mode) {
    settings->writeSetting("enableSmoothScroll", mode);
}
//------------------------------------------------------------------------------
bool Settings::useThumbnailCache() {
    return settings->readSetting("thumbnailCache", true).toBool();
}

void Settings::setUseThumbnailCache(bool mode) {
    settings->writeSetting("thumbnailCache", mode);
}

// in-memory thumbnail cache limit in MB; 0 disables the memory cache
int Settings::thumbnailerMemCacheLimit() {
    int limit = settings->readSetting("thumbnailerMemCacheLimit", 64).toInt();
    if(limit < 0)
        limit = 0;
    return limit;
}

void Settings::setThumbnailerMemCacheLimit(int limitMB) {
    settings->writeSetting("thumbnailerMemCacheLimit", qMax(0, limitMB));
}
//------------------------------------------------------------------------------
QStringList Settings::savedPaths() {
    return settings->readSetting("savedPaths", QDir::homePath()).toStringList();
}

void Settings::setSavedPaths(const QStringList& paths) {
    settings->writeSetting("savedPaths", paths);
}

// savedPaths() falls back to home, so tell "nothing stored" apart here
bool Settings::savedPathsStored() {
    return settings->settingsConf->contains(groupedKey("savedPaths"));
}

void Settings::clearSavedPaths() {
    settings->settingsConf->remove(groupedKey("savedPaths"));
}
//------------------------------------------------------------------------------
QStringList Settings::bookmarks() {
    return settings->readSetting("bookmarks").toStringList();
}

void Settings::setBookmarks(const QStringList& paths) {
    settings->writeSetting("bookmarks", paths);
}

void Settings::clearBookmarks() {
    settings->settingsConf->remove(groupedKey("bookmarks"));
}
//------------------------------------------------------------------------------
QStringList Settings::clearOnExitStores() {
    return settings->readSetting("clearOnExit").toStringList();
}

void Settings::setClearOnExitStores(const QStringList &ids) {
    if(ids.isEmpty())
        settings->settingsConf->remove(groupedKey("clearOnExit"));
    else
        settings->writeSetting("clearOnExit", ids);
}
//------------------------------------------------------------------------------
bool Settings::placesPanel() {
    return settings->readSetting("placesPanel", true).toBool();
}

void Settings::setPlacesPanel(bool mode) {
    settings->writeSetting("placesPanel", mode);
}
//------------------------------------------------------------------------------
bool Settings::folderViewTopBar() {
    return settings->readSetting("folderViewTopBar", false).toBool();
}

void Settings::setFolderViewTopBar(bool mode) {
    settings->writeSetting("folderViewTopBar", mode);
}
//------------------------------------------------------------------------------
bool Settings::placesPanelBookmarksExpanded() {
    return settings->readSetting("placesPanelBookmarksExpanded", true).toBool();
}

void Settings::setPlacesPanelBookmarksExpanded(bool mode) {
    settings->writeSetting("placesPanelBookmarksExpanded", mode);
}
//------------------------------------------------------------------------------
bool Settings::placesPanelTreeExpanded() {
    return settings->readSetting("placesPanelTreeExpanded", true).toBool();
}

void Settings::setPlacesPanelTreeExpanded(bool mode) {
    settings->writeSetting("placesPanelTreeExpanded", mode);
}
//------------------------------------------------------------------------------
int Settings::placesPanelWidth() {
    return settings->readSetting("placesPanelWidth", 260).toInt();
}

void Settings::setPlacesPanelWidth(int width) {
    settings->writeSetting("placesPanelWidth", width);
}
//------------------------------------------------------------------------------
void Settings::setSlideshowInterval(int ms) {
    settings->writeSetting("slideshowInterval", ms);
}

int Settings::slideshowInterval() {
    int interval = settings->readSetting("slideshowInterval", 3000).toInt();
    if(interval <= 0)
        interval = 3000;
    return interval;
}
//------------------------------------------------------------------------------
// Thumbnailer and duplicate finder share idealThreadCount() cores; each ceiling is the budget minus
// the other's share (enforced here, not just in the UI). Thumbnailer wins ties; minimum 1 each.
static int totalCoreBudget() {
    return qMax(1, QThread::idealThreadCount());
}

static int defaultThreadsPerPool() {
    return qMax(1, qMin(2, totalCoreBudget() / 2));
}

int Settings::thumbnailerThreadCount() {
    int count = settings->readSetting("thumbnailerThreads", defaultThreadsPerPool()).toInt();
    return qBound(1, count, qMax(1, totalCoreBudget() - 1));
}

void Settings::setThumbnailerThreadCount(int count) {
    int maxAllowed = qMax(1, totalCoreBudget() - duplicateSearchThreadCount());
    settings->writeSetting("thumbnailerThreads", qBound(1, count, maxAllowed));
}

int Settings::duplicateSearchThreadCount() {
    int count = settings->readSetting("duplicateSearchThreads", defaultThreadsPerPool()).toInt();
    return qBound(1, count, qMax(1, totalCoreBudget() - thumbnailerThreadCount()));
}

void Settings::setDuplicateSearchThreadCount(int count) {
    int maxAllowed = qMax(1, totalCoreBudget() - thumbnailerThreadCount());
    settings->writeSetting("duplicateSearchThreads", qBound(1, count, maxAllowed));
}

// one [State] key per value; only the two widget-state blobs are binary
QByteArray Settings::duplicateFinderGeometry() {
    return settings->readSetting("duplicateFinderGeometry").toByteArray();
}

void Settings::setDuplicateFinderGeometry(const QByteArray &geometry) {
    settings->writeSetting("duplicateFinderGeometry", geometry);
}

QByteArray Settings::fileInfoDialogGeometry() {
    return settings->readSetting("fileInfoDialogGeometry").toByteArray();
}

void Settings::setFileInfoDialogGeometry(const QByteArray &geometry) {
    settings->writeSetting("fileInfoDialogGeometry", geometry);
}

QByteArray Settings::duplicateFinderHeader() {
    return settings->readSetting("duplicateFinderHeader").toByteArray();
}

void Settings::setDuplicateFinderHeader(const QByteArray &header) {
    settings->writeSetting("duplicateFinderHeader", header);
}

int Settings::duplicateFinderSimilarity() {
    return settings->readSetting("duplicateFinderSimilarity", 87).toInt();
}

void Settings::setDuplicateFinderSimilarity(int value) {
    settings->writeSetting("duplicateFinderSimilarity", value);
}

bool Settings::duplicateFinderRecursive() {
    return settings->readSetting("duplicateFinderRecursive", true).toBool();
}

void Settings::setDuplicateFinderRecursive(bool mode) {
    settings->writeSetting("duplicateFinderRecursive", mode);
}

bool Settings::duplicateFinderRotated() {
    return settings->readSetting("duplicateFinderRotated", false).toBool();
}

void Settings::setDuplicateFinderRotated(bool mode) {
    settings->writeSetting("duplicateFinderRotated", mode);
}

bool Settings::duplicateFinderMirrored() {
    return settings->readSetting("duplicateFinderMirrored", false).toBool();
}

void Settings::setDuplicateFinderMirrored(bool mode) {
    settings->writeSetting("duplicateFinderMirrored", mode);
}

QStringList Settings::duplicateFinderTargets() {
    return settings->readSetting("duplicateFinderTargets").toStringList();
}

void Settings::setDuplicateFinderTargets(const QStringList &targets) {
    settings->writeSetting("duplicateFinderTargets", targets);
}

void Settings::clearDuplicateFinderTargets() {
    settings->settingsConf->remove(groupedKey("duplicateFinderTargets"));
}

QString Settings::duplicateHashCachePath() {
    return tmpDir() + "duplicatehashes.dat";
}

QString Settings::settingsFilePath() {
    return settingsConf->fileName();
}
//------------------------------------------------------------------------------
bool Settings::smoothUpscaling() {
    return settings->readSetting("smoothUpscaling", true).toBool();
}

void Settings::setSmoothUpscaling(bool mode) {
    settings->writeSetting("smoothUpscaling", mode);
}
//------------------------------------------------------------------------------
int Settings::folderViewIconSize() {
    return settings->readSetting("folderViewIconSize", 120).toInt();
}

void Settings::setFolderViewIconSize(int value) {
    settings->writeSetting("folderViewIconSize", value);
}

int Settings::folderViewFontPointSize() {
    int defaultSize = qMax(QApplication::font().pointSize() - 1, 8);
    return qBound(6, settings->readSetting("folderViewFontPointSize", defaultSize).toInt(), 48);
}

void Settings::setFolderViewFontPointSize(int value) {
    settings->writeSetting("folderViewFontPointSize", qBound(6, value, 48));
}

QColor Settings::folderViewLabelBackgroundColor() {
    return settings->mColorScheme.folderview_label_bg;
}
//------------------------------------------------------------------------------
QColor Settings::folderViewSelectionColor() {
    return settings->mColorScheme.folderview_selection;
}
//------------------------------------------------------------------------------
QColor Settings::folderViewParentIconColor() {
    return settings->mColorScheme.folderview_parent_icon;
}
//------------------------------------------------------------------------------
QColor Settings::folderViewSelectedLabelBackgroundColor() {
    return settings->mColorScheme.folderview_selected_label_bg;
}
//------------------------------------------------------------------------------
QColor Settings::folderViewCellBackgroundColor() {
    return settings->mColorScheme.folderview_cell_bg;
}
//------------------------------------------------------------------------------
bool Settings::expandImage() {
    return settings->readSetting("expandImage", false).toBool();
}

void Settings::setExpandImage(bool mode) {
    settings->writeSetting("expandImage", mode);
}
//------------------------------------------------------------------------------
int Settings::expandLimit() {
    return settings->readSetting("expandLimit", 2).toInt();
}

void Settings::setExpandLimit(int value) {
    settings->writeSetting("expandLimit", value);
}
//------------------------------------------------------------------------------
int Settings::JPEGSaveQuality() {
    int quality = std::clamp(settings->readSetting("JPEGSaveQuality", 95).toInt(), 0, 100);
    return quality;
}

void Settings::setJPEGSaveQuality(int value) {
    settings->writeSetting("JPEGSaveQuality", value);
}
//------------------------------------------------------------------------------
ScalingFilter Settings::scalingFilter() {
    int defaultFilter = 1;
#ifdef USE_OPENCV
    // default to a nicer QI_FILTER_CV_CUBIC
    defaultFilter = 3;
#endif
    int mode = settings->readSetting("scalingFilter", defaultFilter).toInt();
#ifndef USE_OPENCV
    if(mode > 2)
        mode = 1;
#endif
    if(mode < 0 || mode > 4)
        mode = 1;
    return static_cast<ScalingFilter>(mode);
}

void Settings::setScalingFilter(ScalingFilter mode) {
    settings->writeSetting("scalingFilter", mode);
}

bool Settings::legacyScalingEngine() {
#ifdef USE_OPENCV
    return settings->readSetting("scalingEngine", "qt").toString() == QLatin1String("opencv");
#else
    return false;
#endif
}

void Settings::setLegacyScalingEngine(bool legacy) {
    settings->writeSetting("scalingEngine", legacy ? "opencv" : "qt");
}

bool Settings::scalingSharpen() {
    return settings->readSetting("scalingSharpen", false).toBool();
}

void Settings::setScalingSharpen(bool mode) {
    settings->writeSetting("scalingSharpen", mode);
}
//------------------------------------------------------------------------------
ScalingFilter Settings::resizeFilter() {
    int mode = settings->readSetting("resizeFilter", QI_FILTER_BILINEAR).toInt();
#ifdef USE_OPENCV
    if(mode < QI_FILTER_NEAREST || mode > QI_FILTER_CV_CUBIC_SHARPEN)
        mode = QI_FILTER_BILINEAR;
#else
    // the CV_* filters only exist in OpenCV builds
    if(mode != QI_FILTER_NEAREST && mode != QI_FILTER_BILINEAR)
        mode = QI_FILTER_BILINEAR;
#endif
    return static_cast<ScalingFilter>(mode);
}

void Settings::setResizeFilter(ScalingFilter mode) {
    settings->writeSetting("resizeFilter", mode);
}
//------------------------------------------------------------------------------
bool Settings::smoothAnimatedImages() {
    return settings->readSetting("smoothAnimatedImages", true).toBool();
}

void Settings::setSmoothAnimatedImages(bool mode) {
    settings->writeSetting("smoothAnimatedImages", mode);
}
//------------------------------------------------------------------------------
bool Settings::infoBarFullscreen() {
    return settings->readSetting("infoBarFullscreen", true).toBool();
}

void Settings::setInfoBarFullscreen(bool mode) {
    settings->writeSetting("infoBarFullscreen", mode);
}
//------------------------------------------------------------------------------
bool Settings::infoBarWindowed() {
    return settings->readSetting("infoBarWindowed", true).toBool();
}

void Settings::setInfoBarWindowed(bool mode) {
    settings->writeSetting("infoBarWindowed", mode);
}
//------------------------------------------------------------------------------
bool Settings::windowTitleExtendedInfo() {
    return settings->readSetting("windowTitleExtendedInfo", true).toBool();
}

void Settings::setWindowTitleExtendedInfo(bool mode) {
    settings->writeSetting("windowTitleExtendedInfo", mode);
}

//------------------------------------------------------------------------------
// full screen only by default; the old "cursorAutohiding" off maps to never
CursorAutohideMode Settings::cursorAutohide() {
    int mode = settings->readSetting("cursorAutohideMode", -1).toInt();
    if(mode == -1)
        mode = settings->readSetting("cursorAutohiding", true).toBool() ? CURSOR_AUTOHIDE_FULLSCREEN
                                                                        : CURSOR_AUTOHIDE_NEVER;
    if(mode < 0 || mode > 2)
        mode = CURSOR_AUTOHIDE_FULLSCREEN;
    return static_cast<CursorAutohideMode>(mode);
}

void Settings::setCursorAutohide(CursorAutohideMode mode) {
    settings->writeSetting("cursorAutohideMode", mode);
}
//------------------------------------------------------------------------------
bool Settings::firstRun() {
    return settings->readSetting("firstRun", true).toBool();
}

void Settings::setFirstRun(bool mode) {
    settings->writeSetting("firstRun", mode);
}
//------------------------------------------------------------------------------
bool Settings::showSaveOverlay() {
    return settings->readSetting("showSaveOverlay", true).toBool();
}

void Settings::setShowSaveOverlay(bool mode) {
    settings->writeSetting("showSaveOverlay", mode);
}
//------------------------------------------------------------------------------
bool Settings::confirmDelete() {
    return settings->readSetting("confirmDelete", true).toBool();
}

void Settings::setConfirmDelete(bool mode) {
    settings->writeSetting("confirmDelete", mode);
}
//------------------------------------------------------------------------------
bool Settings::confirmTrash() {
    return settings->readSetting("confirmTrash", true).toBool();
}

void Settings::setConfirmTrash(bool mode) {
    settings->writeSetting("confirmTrash", mode);
}
//------------------------------------------------------------------------------
bool Settings::unloadThumbs() {
    return settings->readSetting("unloadThumbs", true).toBool();
}

void Settings::setUnloadThumbs(bool mode) {
    settings->writeSetting("unloadThumbs", mode);
}
//------------------------------------------------------------------------------
float Settings::zoomStep() {
    bool ok = false;
    float value = settings->readSetting("zoomStep", 0.26f).toFloat(&ok);
    if(!ok)
        return 0.2f;
    value = qBound(0.01f, value, 0.5f);
    return value;
}

void Settings::setZoomStep(float value) {
    value = qBound(0.01f, value, 0.5f);
    settings->writeSetting("zoomStep", value);
}
//------------------------------------------------------------------------------
float Settings::mouseScrollingSpeed() {
    bool ok = false;
    float value = settings->readSetting("mouseScrollingSpeed", 1.0f).toFloat(&ok);
    if(!ok)
        return 1.0f;
    value = qBound(0.5f, value, 2.0f);
    return value;
}

void Settings::setMouseScrollingSpeed(float value) {
    value = qBound(0.5f, value, 2.0f);
    settings->writeSetting("mouseScrollingSpeed", value);
}
//------------------------------------------------------------------------------
void Settings::setZoomIndicatorMode(ZoomIndicatorMode mode) {
    settings->writeSetting("zoomIndicatorMode", mode);
}

ZoomIndicatorMode Settings::zoomIndicatorMode() {
    int mode = settings->readSetting("zoomIndicatorMode", 0).toInt();
    if(mode < 0 || mode > 2)
        mode = 0;
    return static_cast<ZoomIndicatorMode>(mode);
}
//------------------------------------------------------------------------------
void Settings::setFocusPointIn1to1Mode(ImageFocusPoint mode) {
    settings->writeSetting("focusPointIn1to1Mode", mode);
}

ImageFocusPoint Settings::focusPointIn1to1Mode() {
    int mode = settings->readSetting("focusPointIn1to1Mode", 1).toInt();
    if(mode < 0 || mode > 2)
        mode = 1;
    return static_cast<ImageFocusPoint>(mode);
}

ImageScrolling Settings::imageScrolling() {
    int mode = settings->readSetting("imageScrolling", 1).toInt();
    if(mode < 0 || mode > 2)
        mode = 0;
    return static_cast<ImageScrolling>(mode);
}

void Settings::setImageScrolling(ImageScrolling mode) {
    settings->writeSetting("imageScrolling", mode);
}
//------------------------------------------------------------------------------
ViewMode Settings::defaultViewMode() {
    int mode = settings->readSetting("defaultViewMode", 1).toInt();
    if(mode < 0 || mode > 1)
        mode = 0;
    return static_cast<ViewMode>(mode);
}

void Settings::setDefaultViewMode(ViewMode mode) {
    settings->writeSetting("defaultViewMode", mode);
}
//------------------------------------------------------------------------------
FolderEndAction Settings::folderEndAction() {
    int mode = settings->readSetting("folderEndAction", 0).toInt();
    if(mode < 0 || mode > 2)
        mode = 0;
    return static_cast<FolderEndAction>(mode);
}

void Settings::setFolderEndAction(FolderEndAction mode) {
    settings->writeSetting("folderEndAction", mode);
}
//------------------------------------------------------------------------------
bool Settings::printLandscape() {
    return readSetting("printLandscape", false).toBool();
}

void Settings::setPrintLandscape(bool mode) {
    writeSetting("printLandscape", mode);
}
//------------------------------------------------------------------------------
bool Settings::printPdfDefault() {
    return readSetting("printPdfDefault", false).toBool();
}

void Settings::setPrintPdfDefault(bool mode) {
    writeSetting("printPdfDefault", mode);
}
//------------------------------------------------------------------------------
bool Settings::printColor() {
    return readSetting("printColor", false).toBool();
}

void Settings::setPrintColor(bool mode) {
    writeSetting("printColor", mode);
}
//------------------------------------------------------------------------------
bool Settings::printFitToPage() {
    return readSetting("printFitToPage", true).toBool();
}

void Settings::setPrintFitToPage(bool mode) {
    writeSetting("printFitToPage", mode);
}
//------------------------------------------------------------------------------
QString Settings::lastPrinter() {
    return readSetting("lastPrinter", "").toString();
}

void Settings::setLastPrinter(const QString& name) {
    writeSetting("lastPrinter", name);
}
//------------------------------------------------------------------------------
int Settings::shortcutsSortColumn() {
    return readSetting("shortcutsSortColumn", 0).toInt();
}

void Settings::setShortcutsSortColumn(int column) {
    writeSetting("shortcutsSortColumn", column);
}
//------------------------------------------------------------------------------
Qt::SortOrder Settings::shortcutsSortOrder() {
    return static_cast<Qt::SortOrder>(readSetting("shortcutsSortOrder", Qt::AscendingOrder).toInt());
}

void Settings::setShortcutsSortOrder(Qt::SortOrder order) {
    writeSetting("shortcutsSortOrder", static_cast<int>(order));
}
//------------------------------------------------------------------------------
bool Settings::jxlAnimation() {
    return settings->readSetting("jxlAnimation", false).toBool();
}

void Settings::setJxlAnimation(bool mode) {
    settings->writeSetting("jxlAnimation", mode);
}
//------------------------------------------------------------------------------
bool Settings::showFullMetadata() {
    return settings->readSetting("showFullMetadata", false).toBool();
}

void Settings::setShowFullMetadata(bool mode) {
    settings->writeSetting("showFullMetadata", mode);
}
//------------------------------------------------------------------------------
bool Settings::autoResizeWindow() {
    return settings->readSetting("autoResizeWindow", false).toBool();
}

void Settings::setAutoResizeWindow(bool mode) {
    settings->writeSetting("autoResizeWindow", mode);
}
//------------------------------------------------------------------------------
int Settings::autoResizeLimit() {
    int limit = settings->readSetting("autoResizeLimit", 90).toInt();
    if(limit < 30 || limit > 100)
        limit = 90;
    return limit;
}

void Settings::setAutoResizeLimit(int percent) {
    settings->writeSetting("autoResizeLimit", percent);
}
//------------------------------------------------------------------------------
int Settings::memoryAllocationLimit() {
    int limit = settings->readSetting("memoryAllocationLimit", 1024).toInt();
    if(limit < 512)
        limit = 512;
    else if(limit > 8192)
        limit = 8192;
    return limit;
}

void Settings::setMemoryAllocationLimit(int limitMB) {
    settings->writeSetting("memoryAllocationLimit", limitMB);
}
//------------------------------------------------------------------------------
bool Settings::panelCenterSelection() {
    return settings->readSetting("panelCenterSelection", false).toBool();
}

void Settings::setPanelCenterSelection(bool mode) {
    settings->writeSetting("panelCenterSelection", mode);
}
//------------------------------------------------------------------------------
QString Settings::language() {
    return readSetting("language", "en_US").toString();
}

void Settings::setLanguage(const QString& lang) {
    writeSetting("language", lang);
}
//------------------------------------------------------------------------------
bool Settings::useFixedZoomLevels() {
    return settings->readSetting("useFixedZoomLevels", false).toBool();
}

void Settings::setUseFixedZoomLevels(bool mode) {
    settings->writeSetting("useFixedZoomLevels", mode);
}
//------------------------------------------------------------------------------
QString Settings::defaultZoomLevels() {
    return QString("0.05,0.1,0.125,0.166,0.25,0.333,0.5,0.66,1,1.5,2,3,4,5,6,7,8");
}
QString Settings::zoomLevels() {
    return readSetting("fixedZoomLevels", defaultZoomLevels()).toString();
}

void Settings::setZoomLevels(const QString& levels) {
    writeSetting("fixedZoomLevels", levels);
}
//------------------------------------------------------------------------------
bool Settings::unlockMinZoom() {
    return settings->readSetting("unlockMinZoom", true).toBool();
}

void Settings::setUnlockMinZoom(bool mode) {
    settings->writeSetting("unlockMinZoom", mode);
}
//------------------------------------------------------------------------------
bool Settings::sortFolders() {
    return settings->readSetting("sortFolders", true).toBool();
}

void Settings::setSortFolders(bool mode) {
    settings->writeSetting("sortFolders", mode);
}
//------------------------------------------------------------------------------
bool Settings::allowBrowseRoot() {
    return settings->readSetting("allowBrowseRoot", false).toBool();
}

void Settings::setAllowBrowseRoot(bool mode) {
    settings->writeSetting("allowBrowseRoot", mode);
}
//------------------------------------------------------------------------------
bool Settings::trackpadDetection() {
    return settings->readSetting("trackpadDetection", true).toBool();
}

void Settings::setTrackpadDetection(bool mode) {
    settings->writeSetting("trackpadDetection", mode);
}
//------------------------------------------------------------------------------
bool Settings::clickableEdges() {
    return settings->readSetting("clickableEdges", false).toBool();
}

void Settings::setClickableEdges(bool mode) {
    settings->writeSetting("clickableEdges", mode);
}
//------------------------------------------------------------------------------
bool Settings::clickableEdgesVisible() {
    return settings->readSetting("clickableEdgesVisible", true).toBool();
}

void Settings::setClickableEdgesVisible(bool mode) {
    settings->writeSetting("clickableEdgesVisible", mode);
}
//------------------------------------------------------------------------------
bool Settings::showHiddenFiles() {
    return settings->readSetting("showHiddenFiles", true).toBool();
}

void Settings::setShowHiddenFiles(bool mode) {
    settings->writeSetting("showHiddenFiles", mode);
}
//------------------------------------------------------------------------------
// show non-media files (text, configs, unknown types) in the folder view
bool Settings::showOtherFileTypes() {
    return settings->readSetting("showOtherFileTypes", false).toBool();
}

void Settings::setShowOtherFileTypes(bool mode) {
    settings->writeSetting("showOtherFileTypes", mode);
}
