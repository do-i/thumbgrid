#include "actionmanager.h"

#include <utility>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "shortcutpresetstore.h"
#include "utils/logging.h"

ActionManager *actionManager = nullptr;

namespace {

// selected preset's file (XDG, PRESETS_PATH, qrc), else the always-embedded qimgv
QString resolveSelectedPresetPath() {
    QString path = ShortcutPresetStore::resolvePath(settings->selectedPreset());
    if(path.isEmpty())
        path = ShortcutPresetStore::resolvePath(QStringLiteral("qimgv"));
    return path;
}

// $Ctrl/$Alt/$Shift -> platform modifier names (symbols on macOS)
QString expandModifierTokens(QString keys) {
    keys.replace(QStringLiteral("$Ctrl"),  InputMap::keyNameCtrl());
    keys.replace(QStringLiteral("$Shift"), InputMap::keyNameShift());
    keys.replace(QStringLiteral("$Alt"),   InputMap::keyNameAlt());
    return keys;
}

// the scroll actions' built-in fallbacks (empty: none)
const QMap<QString, QString> &scrollFallbackDefaults() {
    static const QMap<QString, QString> defaults{
        {"scrollUp", ""}, {"scrollDown", ""},
        {"scrollLeft", "prevImage"}, {"scrollRight", "nextImage"},
        {"scrollUpFine", ""}, {"scrollDownFine", ""},
        {"scrollLeftFine", "seekVideoBackward"}, {"scrollRightFine", "seekVideoForward"}};
    return defaults;
}

// in a video nothing scrolls: arrows and wheel seek instead (wheel up = forward, like mpv)
const QMap<QString, QString> &videoScrollFallbackDefaults() {
    static const QMap<QString, QString> defaults{
        {"scrollUp", ""}, {"scrollDown", ""},
        {"scrollLeft", "seekVideoBackward"}, {"scrollRight", "seekVideoForward"},
        {"scrollUpFine", ""}, {"scrollDownFine", ""},
        {"scrollLeftFine", "seekVideoBackward"}, {"scrollRightFine", "seekVideoForward"},
        {"scrollOrPrevImage", "seekVideoForward"}, {"scrollOrNextImage", "seekVideoBackward"}};
    return defaults;
}

// a fallback may not be a scroll action itself (which could loop back)
bool isScrollFamilyAction(const QString &action) {
    return scrollFallbackDefaults().contains(action) || action.startsWith(QLatin1String("scrollOr"));
}

QList<ViewMode> shortcutContexts() {
    return {MODE_GLOBAL, MODE_DOCUMENT, MODE_FOLDERVIEW, MODE_COPYMOVE};
}

QStringList shortcutKeysFromJson(const QJsonValue &value) {
    QStringList keys;
    const QJsonArray arr = value.isArray()
        ? value.toArray()
        : value.toObject().value("keys").toArray();
    for(const QJsonValue &key : arr) {
        const QString shortcut = expandModifierTokens(key.toString().trimmed());
        if(!shortcut.isEmpty() && !keys.contains(shortcut))
            keys.append(shortcut);
    }
    keys.sort(Qt::CaseInsensitive);
    return keys;
}

// accepts { action: [keys] } and the older { key: action } form
void insertShortcutObject(const QJsonObject &obj, ActionManager::ContextMap &out) {
    for(auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if(it.value().isString()) {
            out.insert(expandModifierTokens(it.key()), it.value().toString());
            continue;
        }
        for(const QString &key : shortcutKeysFromJson(it.value()))
            out.insert(key, it.key());
    }
}

void insertPlatformShortcutObject(const QJsonObject &obj,
                                  ActionManager::ContextMap &global,
                                  ActionManager::ContextMap &document,
                                  ActionManager::ContextMap &grid) {
    if(obj.contains("global") || obj.contains("document") || obj.contains("grid")) {
        insertShortcutObject(obj.value("global").toObject(), global);
        insertShortcutObject(obj.value("document").toObject(), document);
        insertShortcutObject(obj.value("grid").toObject(), grid);
        return;
    }
    insertShortcutObject(obj, global);
}

} // namespace

ActionManager::ActionManager(QObject *parent) : QObject(parent) {
}
//------------------------------------------------------------------------------
ActionManager::~ActionManager() {
    delete actionManager;
}
//------------------------------------------------------------------------------
ActionManager *ActionManager::getInstance() {
    if(!actionManager) {
        actionManager = new ActionManager();
        initDefaults();
        initShortcuts();
    }
    return actionManager;
}
//------------------------------------------------------------------------------
void ActionManager::initDefaults() {
    // defaults come from the selected preset; global holds shared bindings, document/grid only differences
    const QString path = resolveSelectedPresetPath();
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ActionManager] could not open preset:" << path;
        return;
    }
    QJsonParseError err;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll(), &err).object();
    if(err.error != QJsonParseError::NoError) {
        qWarning() << "[ActionManager] default shortcuts JSON parse error:" << err.errorString();
        return;
    }

    ContextMap global;
    ContextMap document;
    ContextMap grid;
    ContextMap copyMove;
    insertShortcutObject(root.value("shortcuts").toObject(), global);
    insertShortcutObject(root.value("global").toObject(), global);
    insertShortcutObject(root.value("document").toObject(), document);
    insertShortcutObject(root.value("grid").toObject(), grid);
    insertShortcutObject(root.value("copyMove").toObject(), copyMove);
    // per-OS block, else "default" ("apple" is legacy macos)
    const QJsonObject platform = root.value("platform").toObject();
    const QString osToken = ShortcutPresetStore::currentOsToken();
    QJsonObject platformSpecific = platform.value(osToken).toObject();
    if(platformSpecific.isEmpty() && osToken == QStringLiteral("macos"))
        platformSpecific = platform.value("apple").toObject();
    if(platformSpecific.isEmpty())
        platformSpecific = platform.value("default").toObject();
    insertPlatformShortcutObject(platformSpecific, global, document, grid);

    ShortcutMap loaded;
    loaded.insert(MODE_GLOBAL, global);
    loaded.insert(MODE_DOCUMENT, document);
    loaded.insert(MODE_FOLDERVIEW, grid);
    loaded.insert(MODE_COPYMOVE, copyMove);
    // canonicalize like shortcuts.json (document+grid duplicates hoisted to global),
    // or every restart sees shortcuts != defaults and shows "Custom"
    Settings::collapseShortcutContexts(loaded);
    actionManager->defaults = loaded;
}

//------------------------------------------------------------------------------
void ActionManager::initShortcuts() {
    // first run: materialize shortcuts.json from the selected preset
    const bool firstRun = !settings->shortcutsJsonExists();
    actionManager->readShortcuts();
    bool empty = true;
    for(auto it = actionManager->shortcuts.cbegin(); it != actionManager->shortcuts.cend(); ++it)
        if(!it.value().isEmpty()) { empty = false; break; }
    if(empty)
        actionManager->resetDefaults();
    if(firstRun) {
        settings->setSelectedPreset(settings->selectedPreset()); // persist default id
        actionManager->saveShortcuts();                          // write shortcuts.json
    }
    actionManager->rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
void ActionManager::addShortcut(ViewMode context, const QString &keys, const QString &action) {
    ActionType type = validateAction(action);
    if(type != ActionType::ACTION_INVALID) {
        actionManager->shortcuts[context].insert(keys, action);
        actionManager->rebuildShortcutLookup();
    }
}
//------------------------------------------------------------------------------
void ActionManager::removeShortcut(ViewMode context, const QString &keys) {
    actionManager->shortcuts[context].remove(keys);
    actionManager->rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
void ActionManager::setContext(ViewMode context) {
    actionManager->context = context;
}
//------------------------------------------------------------------------------
bool ActionManager::isOverlayContext(ViewMode context) {
    return context == MODE_COPYMOVE;
}
//------------------------------------------------------------------------------
ViewMode ActionManager::currentContext() const {
    return context;
}
//------------------------------------------------------------------------------
QString ActionManager::contextToString(ViewMode context) {
    if(context == MODE_GLOBAL)
        return QStringLiteral("global");
    if(context == MODE_COPYMOVE)
        return QStringLiteral("copyMove");
    return (context == MODE_FOLDERVIEW) ? QStringLiteral("grid")
                                        : QStringLiteral("document");
}
//------------------------------------------------------------------------------
ViewMode ActionManager::contextFromString(const QString &name) {
    if(name == QStringLiteral("global"))
        return MODE_GLOBAL;
    if(name == QStringLiteral("copyMove"))
        return MODE_COPYMOVE;
    return (name == QStringLiteral("grid") || name == QStringLiteral("folderview"))
        ? MODE_FOLDERVIEW
        : MODE_DOCUMENT;
}
//------------------------------------------------------------------------------
QStringList ActionManager::actionList() {
    return appActions->getList();
}
//------------------------------------------------------------------------------
const ActionManager::ShortcutMap &ActionManager::allShortcuts() {
    return actionManager->shortcuts;
}
//------------------------------------------------------------------------------
const ActionManager::ShortcutMap &ActionManager::allDefaultShortcuts() {
    return actionManager->defaults;
}
//------------------------------------------------------------------------------
void ActionManager::removeAllShortcuts() {
    shortcuts.clear();
    // seed every context, or an empty preset section makes shortcuts != defaults
    for(ViewMode ctx : shortcutContexts())
        shortcuts.insert(ctx, ContextMap());
    rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
void ActionManager::removeAllShortcuts(const QString& actionName) {
    if(validateAction(actionName) == ActionType::ACTION_INVALID)
        return;

    for(auto ctx = shortcuts.begin(); ctx != shortcuts.end(); ++ctx) {
        ContextMap &m = ctx.value();
        for(auto i = m.begin(); i != m.end();) {
            if(i.value() == actionName)
                i = m.erase(i);
            else
                ++i;
        }
    }
    rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
QString ActionManager::keyForNativeScancode(quint32 scanCode) {
    return inputMap->keys().value(scanCode);
}
//------------------------------------------------------------------------------
void ActionManager::resetDefaults() {
    actionManager->shortcuts = actionManager->defaults;
    actionManager->rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
void ActionManager::resetDefaults(ViewMode context) {
    actionManager->shortcuts[context] = actionManager->defaults.value(context);
    actionManager->rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
void ActionManager::resetDefaults(const QString& action) {
    actionManager->removeAllShortcuts(action);
    for(ViewMode ctx : shortcutContexts()) {
        QMapIterator<QString, QString> i(defaults[ctx]);
        while(i.hasNext()) {
            i.next();
            if(i.value() == action) {
                shortcuts[ctx].insert(i.key(), i.value());
                qCDebug(logSettings) << "[ActionManager] new action " << i.value() << " - assigning as [" << i.key() << "] in" << contextToString(ctx);
            }
        }
    }
    rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
void ActionManager::adjustFromVersion(const QVersionNumber& lastVer) {
    // swap Ctrl-P & P
    if(lastVer < QVersionNumber(0,9,2)) {
        actionManager->resetDefaults("print");
        actionManager->resetDefaults("openSettings");
    }
    // swap WheelUp/WheelDown. derp
    if(lastVer < QVersionNumber(1,0,1)) {
        qCDebug(logSettings) << "[actionManager]: swapping WheelUp/WheelDown";
        for(auto ctx = shortcuts.begin(); ctx != shortcuts.end(); ++ctx) {
            ContextMap swapped;
            QMapIterator<QString, QString> i(ctx.value());
            while(i.hasNext()) {
                i.next();
                QString key = i.key();
                if(key.contains("WheelUp"))
                    key.replace("WheelUp", "WheelDown");
                else if(key.contains("WheelDown"))
                    key.replace("WheelDown", "WheelUp");
                swapped.insert(key, i.value());
            }
            ctx.value() = swapped;
        }
    }
    // drop the old MiddleButton=exit default (only if still bound to exit)
    if(lastVer < QVersionNumber(2026,7,3)) {
        for(ViewMode ctx : shortcutContexts()) {
            if(shortcuts[ctx].value("MiddleButton") == "exit") {
                shortcuts[ctx].remove("MiddleButton");
                qCDebug(logSettings) << "[actionManager]: removed MiddleButton=exit in" << contextToString(ctx);
            }
        }
    }
    // backfill File info with the preset's key (preset-specific); skip if bound or the key is taken
    if(lastVer < QVersionNumber(2026,7,25)) {
        // any existing binding counts: defaults are canonicalized, so seeding global could double a key
        bool alreadyBound = false;
        for(ViewMode ctx : shortcutContexts())
            alreadyBound = alreadyBound || !shortcuts[ctx].key("toggleImageInfo").isEmpty();
        for(ViewMode ctx : shortcutContexts()) {
            if(alreadyBound)
                break;
            const QString key = defaults[ctx].key("toggleImageInfo");
            if(key.isEmpty() || shortcuts[ctx].contains(key))
                continue;
            shortcuts[ctx].insert(key, "toggleImageInfo");
            qCDebug(logSettings) << "[actionManager]: bound" << key << "=toggleImageInfo in" << contextToString(ctx);
        }
    }
    // resize gained a grid flow: mirror the user's document key there unless it is taken
    if(lastVer < QVersionNumber(2026,10,1)) {
        const QString key = shortcuts[MODE_DOCUMENT].key("resize");
        const bool gridHasResize = !shortcuts[MODE_FOLDERVIEW].key("resize").isEmpty() ||
                                   !shortcuts[MODE_GLOBAL].key("resize").isEmpty();
        if(!key.isEmpty() && !gridHasResize &&
           !shortcuts[MODE_FOLDERVIEW].contains(key) && !shortcuts[MODE_GLOBAL].contains(key)) {
            shortcuts[MODE_FOLDERVIEW].insert(key, "resize");
            // store hoisted to global like defaults, so an untouched preset compares equal
            Settings::collapseShortcutContexts(shortcuts);
            qCDebug(logSettings) << "[actionManager]: bound" << key << "=resize in grid";
        }
    }
    // arrows became scroll actions and the wheel scrollOr*: follow the preset where the user kept the old default
    if(lastVer < QVersionNumber(2026,10,3)) {
        const QMap<QString, QString> oldDefaults{
            {"Left", "prevImage"}, {"Right", "nextImage"},
            {"WheelUp", "prevImage"}, {"WheelDown", "nextImage"},
            {expandModifierTokens("$Ctrl+Up"), "zoomIn"},
            {expandModifierTokens("$Ctrl+Down"), "zoomOut"},
            {expandModifierTokens("$Ctrl+Left"), "seekVideoBackward"},
            {expandModifierTokens("$Ctrl+Right"), "seekVideoForward"}};
        ContextMap &doc = shortcuts[MODE_DOCUMENT];
        QMapIterator<QString, QString> i(defaults[MODE_DOCUMENT]);
        while(i.hasNext()) {
            i.next();
            if(!i.value().startsWith("scroll"))
                continue;
            const QString cur = doc.value(i.key());
            const QString global = shortcuts[MODE_GLOBAL].value(i.key());
            const bool free = cur.isEmpty() ? (global.isEmpty() || global == oldDefaults.value(i.key()))
                                            : cur == oldDefaults.value(i.key());
            if(free && cur != i.value()) {
                doc.insert(i.key(), i.value());
                qCDebug(logSettings) << "[actionManager]: bound" << i.key() << "=" << i.value() << "in document";
            }
        }
        // grid Enter/Backspace/Ctrl+A/"/" are now actions; give them the preset's key unless rebound
        const QStringList gridActions{"openSelected", "goUp", "selectAll", "startSearch"};
        ContextMap &grid = shortcuts[MODE_FOLDERVIEW];
        QMapIterator<QString, QString> g(defaults[MODE_FOLDERVIEW]);
        while(g.hasNext()) {
            g.next();
            if(!gridActions.contains(g.value()))
                continue;
            const QString cur = grid.value(g.key());
            const bool free = cur.isEmpty() || (g.key() == "Enter" && cur == "documentView");
            if(free && cur != g.value()) {
                grid.insert(g.key(), g.value());
                qCDebug(logSettings) << "[actionManager]: bound" << g.key() << "=" << g.value() << "in grid";
            }
        }
        // Space now runs its binding: hand the preset's Space to configs that left it unbound or nextImage
        const bool oldNextImageDefault =
            QStringList{"gwenview", "xnviewmp"}.contains(settings->selectedPreset()) &&
            shortcuts[MODE_DOCUMENT].value("Space") == "nextImage";
        if(oldNextImageDefault)
            shortcuts[MODE_DOCUMENT].remove("Space");
        const bool spaceBound = shortcuts[MODE_GLOBAL].contains("Space") ||
                                shortcuts[MODE_DOCUMENT].contains("Space");
        for(ViewMode ctx : {MODE_GLOBAL, MODE_DOCUMENT}) {
            const QString action = defaults[ctx].value("Space");
            if(spaceBound || action.isEmpty())
                continue;
            shortcuts[ctx].insert("Space", action);
            qCDebug(logSettings) << "[actionManager]: bound Space =" << action << "in" << contextToString(ctx);
            break;
        }
        // mouse gestures that were hard-coded are now bindings: bind the preset's action unless the user bound the gesture
        const auto bindPresetKey = [this](ViewMode view, const QString &key) {
            if(shortcuts[MODE_GLOBAL].contains(key) || shortcuts[view].contains(key))
                return;
            for(ViewMode ctx : {MODE_GLOBAL, view}) {
                const QString action = defaults[ctx].value(key);
                if(action.isEmpty())
                    continue;
                shortcuts[ctx].insert(key, action);
                qCDebug(logSettings) << "[actionManager]: bound" << key << "=" << action << "in" << contextToString(ctx);
                return;
            }
        };
        bindPresetKey(MODE_DOCUMENT, "RMB+WheelUp");
        bindPresetKey(MODE_DOCUMENT, "RMB+WheelDown");
        bindPresetKey(MODE_FOLDERVIEW, expandModifierTokens("$Ctrl+WheelUp"));
        bindPresetKey(MODE_FOLDERVIEW, expandModifierTokens("$Ctrl+WheelDown"));
        bindPresetKey(MODE_FOLDERVIEW, "LMB_TileDoubleClick");
        bindPresetKey(MODE_FOLDERVIEW, "LMB_Drag");
        bindPresetKey(MODE_DOCUMENT, "LMB_Drag");
        bindPresetKey(MODE_DOCUMENT, "RMB_Drag");
        bindPresetKey(MODE_DOCUMENT, "LMB_LeftEdge");
        bindPresetKey(MODE_DOCUMENT, "LMB_RightEdge");
        // grid selection keys are now actions; a grid binding on them never ran
        const QMap<QString, QString> gridMoves{
            {"Left", "selectPrevious"}, {"Right", "selectNext"}, {"Up", "selectAbove"},
            {"Down", "selectBelow"}, {"PgUp", "selectPageUp"}, {"PgDown", "selectPageDown"},
            {"Home", "selectFirst"}, {"End", "selectLast"}};
        for(auto move = gridMoves.cbegin(); move != gridMoves.cend(); ++move) {
            grid.insert(move.key(), move.value());
            qCDebug(logSettings) << "[actionManager]: bound" << move.key() << "=" << move.value() << "in grid";
        }
    }
    // qimgv/leftie dropped double-click full screen; remove it from configs still on that default
    if(lastVer < QVersionNumber(2026,10,4)) {
        for(ViewMode ctx : shortcutContexts()) {
            if(shortcuts[ctx].value("LMB_DoubleClick") == "toggleFullscreen" &&
               defaults[ctx].value("LMB_DoubleClick") != "toggleFullscreen") {
                shortcuts[ctx].remove("LMB_DoubleClick");
                qCDebug(logSettings) << "[actionManager]: removed LMB_DoubleClick=toggleFullscreen in" << contextToString(ctx);
            }
        }
    }
    for(ViewMode ctx : shortcutContexts()) {
        ContextMap &cur = shortcuts[ctx];
        QMapIterator<QString, QString> i(defaults[ctx]);
        while(i.hasNext()) {
            i.next();
            if(appActions->getMap().value(i.value()) > lastVer) {
                if(!cur.contains(i.key())) {
                    cur.insert(i.key(), i.value());
                    qCDebug(logSettings) << "[ActionManager] new action " << i.value() << " - assigning as [" << i.key() << "] in" << contextToString(ctx);
                } else if(i.value() != cur.value(i.key())) {
                    qCDebug(logSettings) << "[ActionManager] new action " << i.value() << " - shortcut [" << i.key() << "] already assigned to " << cur.value(i.key());
                }
            }
        }
    }
    // apply
    rebuildShortcutLookup();
    saveShortcuts();
}
//------------------------------------------------------------------------------
void ActionManager::saveShortcuts() {
    settings->saveShortcuts(actionManager->shortcuts);
    // modified iff the mapping differs from the selected preset
    settings->setShortcutsModified(actionManager->shortcuts != actionManager->defaults);
}

// Destructive (UI confirms). Also resets per-action disabled/primary metadata.
void ActionManager::applyPreset(const QString &id) {
    settings->setSelectedPreset(id);
    initDefaults();                                   // reload defaults from `id`
    actionManager->shortcuts = actionManager->defaults;
    actionManager->rebuildShortcutLookup();
    settings->saveDisabledShortcuts(QMap<ViewMode, QStringList>());
    settings->saveShortcutPrimary(QMap<ViewMode, QMap<QString, QString>>());
    actionManager->setScrollFallbackOverrides(QMap<QString, QString>());
    actionManager->setVideoScrollFallbackOverrides(QMap<QString, QString>());
    actionManager->saveShortcuts();
    // a preset selection is never modified; don't rely on the JSON round-trip comparison
    settings->setShortcutsModified(false);
}

QList<PresetInfo> ActionManager::availablePresets() {
    return ShortcutPresetStore::available(true);
}

QString ActionManager::selectedPreset() {
    return settings->selectedPreset();
}
//------------------------------------------------------------------------------
QString ActionManager::actionForShortcut(ViewMode context, const QString &keys) {
    return actionManager->lookupActionForShortcut(context, keys);
}

bool ActionManager::viewerHandlesWheel(QWheelEvent *event) {
    const QString action = lookupActionForShortcut(MODE_DOCUMENT, ShortcutBuilder::fromEvent(event));
    if(action.isEmpty())
        return true;
    return event->modifiers() == Qt::NoModifier &&
           (action == "scrollOrPrevImage" || action == "scrollOrNextImage");
}

// returns first shortcut that is found in the given context
const QString ActionManager::shortcutForAction(ViewMode context, const QString& action) {
    const QString contextKey = shortcuts[context].key(action, "");
    if(!contextKey.isEmpty() || context == MODE_GLOBAL || isOverlayContext(context))
        return contextKey;
    return shortcuts[MODE_GLOBAL].key(action, "");
}

// returns every shortcut bound to action across all contexts
const QStringList ActionManager::shortcutsForAction(const QString& action) {
    QStringList keys;
    for(auto it = shortcuts.cbegin(); it != shortcuts.cend(); ++it) {
        const QStringList ctxKeys = it.value().keys(action);
        for(const QString &k : ctxKeys)
            if(!keys.contains(k))
                keys.append(k);
    }
    return keys;
}

//------------------------------------------------------------------------------
bool ActionManager::invokeAction(const QString &actionName) {
    ActionType type = validateAction(actionName);
    if(type == ActionType::ACTION_NORMAL) {
        QMetaObject::invokeMethod(this, actionName.toLatin1().constData(), Qt::DirectConnection);
        return true;
    } else if(type == ActionType::ACTION_SCRIPT) {
        QString scriptName = actionName;
        scriptName.remove(0, 2); // remove the "s:" prefix
        emit runScript(scriptName);
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
bool ActionManager::invokeActionForShortcut(ViewMode context, const QString &shortcut) {
    const QString action = lookupActionForShortcut(context, shortcut);
    return action.isEmpty() ? false : invokeAction(action);
}
//------------------------------------------------------------------------------
void ActionManager::rebuildShortcutLookup() {
    shortcutLookup.clear();
    for(auto ctx = shortcuts.cbegin(); ctx != shortcuts.cend(); ++ctx) {
        QHash<QString, QString> lookup;
        for(auto it = ctx.value().cbegin(); it != ctx.value().cend(); ++it)
            lookup.insert(it.key(), it.value());
        shortcutLookup.insert(ctx.key(), lookup);
    }
}
//------------------------------------------------------------------------------
QString ActionManager::lookupActionForShortcut(ViewMode context, const QString &shortcut) const {
    if(shortcut.isEmpty())
        return QString();
    const QString contextAction = shortcutLookup.value(context).value(shortcut);
    // Global bindings don't reach the Copy / Move overlay's or clip editing's own keys
    if(!contextAction.isEmpty() || context == MODE_GLOBAL || isOverlayContext(context))
        return contextAction;
    return shortcutLookup.value(MODE_GLOBAL).value(shortcut);
}
//------------------------------------------------------------------------------
void ActionManager::validateShortcuts() {
    for(auto ctx = shortcuts.begin(); ctx != shortcuts.end(); ++ctx) {
        ContextMap &m = ctx.value();
        for(auto i = m.begin(); i != m.end();) {
            if(validateAction(i.value()) == ActionType::ACTION_INVALID)
                i = m.erase(i);
            else
                ++i;
        }
    }
    rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
inline
ActionType ActionManager::validateAction(const QString &actionName) {
    if(appActions->getMap().contains(actionName))
        return ActionType::ACTION_NORMAL;
    if(actionName.startsWith("s:")) {
        QString scriptName = actionName;
        scriptName.remove(0, 2);
        if(scriptManager->scriptExists(scriptName))
            return ActionType::ACTION_SCRIPT;
    }
    return ActionType::ACTION_INVALID;
}
//------------------------------------------------------------------------------
bool ActionManager::hasScrollFallback(const QString &action) {
    return scrollFallbackDefaults().contains(action);
}

QString ActionManager::defaultScrollFallback(const QString &action) {
    return scrollFallbackDefaults().value(action);
}

QString ActionManager::scrollFallback(const QString &action) const {
    return resolvedFallback(scrollFallbacks.value(action, defaultScrollFallback(action)));
}

const QMap<QString, QString> &ActionManager::scrollFallbackOverrides() const {
    return scrollFallbacks;
}

void ActionManager::setScrollFallbackOverrides(const QMap<QString, QString> &overrides) {
    scrollFallbacks = validFallbackOverrides(overrides, scrollFallbackDefaults());
    settings->saveShortcutFallbacks(scrollFallbacks);
}

bool ActionManager::hasVideoScrollFallback(const QString &action) {
    return videoScrollFallbackDefaults().contains(action);
}

QString ActionManager::defaultVideoScrollFallback(const QString &action) {
    return videoScrollFallbackDefaults().value(action);
}

QString ActionManager::videoScrollFallback(const QString &action) const {
    return resolvedFallback(videoScrollFallbacks.value(action, defaultVideoScrollFallback(action)));
}

const QMap<QString, QString> &ActionManager::videoScrollFallbackOverrides() const {
    return videoScrollFallbacks;
}

void ActionManager::setVideoScrollFallbackOverrides(const QMap<QString, QString> &overrides) {
    videoScrollFallbacks = validFallbackOverrides(overrides, videoScrollFallbackDefaults());
    settings->saveShortcutVideoFallbacks(videoScrollFallbacks);
}

QString ActionManager::resolvedFallback(const QString &fallback) {
    if(fallback == QLatin1String("none") || isScrollFamilyAction(fallback))
        return QString();
    return fallback;
}

// keeps overrides of actions with a fallback: "none" or a valid non-scroll, non-default action
QMap<QString, QString> ActionManager::validFallbackOverrides(const QMap<QString, QString> &overrides,
                                                             const QMap<QString, QString> &defaults) {
    QMap<QString, QString> valid;
    for(auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        const bool usable = it.value() == QLatin1String("none") ||
                            (!isScrollFamilyAction(it.value()) && validateAction(it.value()) != ACTION_INVALID);
        if(defaults.contains(it.key()) && usable && it.value() != defaults.value(it.key()))
            valid.insert(it.key(), it.value());
    }
    return valid;
}
//------------------------------------------------------------------------------
void ActionManager::readShortcuts() {
    settings->readShortcuts(shortcuts);
    // 2026.10.3's clip button action; the clip is no longer edited in a mode
    for(ContextMap &m : shortcuts) {
        for(QString &action : m) {
            if(action == QLatin1String("toggleClipEdit"))
                action = QStringLiteral("toggleClip");
        }
    }
    QMap<QString, QString> fallbacks;
    settings->readShortcutFallbacks(fallbacks);
    scrollFallbacks = validFallbackOverrides(fallbacks, scrollFallbackDefaults());
    settings->readShortcutVideoFallbacks(fallbacks);
    videoScrollFallbacks = validFallbackOverrides(fallbacks, videoScrollFallbackDefaults());
    // seed contexts missing from an older shortcuts.json
    for(ViewMode ctx : shortcutContexts())
        shortcuts[ctx];
    QMap<ViewMode, QStringList> disabled;
    settings->readDisabledShortcuts(disabled);
    for(auto ctx = shortcuts.begin(); ctx != shortcuts.end(); ++ctx) {
        const QStringList disabledActions = disabled.value(ctx.key());
        ContextMap &m = ctx.value();
        for(auto it = m.begin(); it != m.end();) {
            if(disabledActions.contains(it.value()))
                it = m.erase(it);
            else
                ++it;
        }
    }
    actionManager->validateShortcuts();
}
//------------------------------------------------------------------------------
bool ActionManager::processEvent(QInputEvent *event) {
    // releasing the right button after RMB+wheel is not a right click
    if(auto wheel = dynamic_cast<QWheelEvent *>(event)) {
        if(wheel->buttons() & Qt::RightButton)
            actionManager->rightButtonWheel = true;
    } else if(auto mouse = dynamic_cast<QMouseEvent *>(event); mouse && mouse->button() == Qt::RightButton) {
        if(mouse->type() == QEvent::MouseButtonPress)
            actionManager->rightButtonWheel = false;
        else if(mouse->type() == QEvent::MouseButtonRelease && std::exchange(actionManager->rightButtonWheel, false))
            return false;
    }
    return actionManager->invokeActionForShortcut(actionManager->context, ShortcutBuilder::fromEvent(event));
}
//------------------------------------------------------------------------------
