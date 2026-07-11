#include "actionmanager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "shortcutpresetstore.h"

ActionManager *actionManager = nullptr;

namespace {

// Resolve the selected preset to a file path (system PRESETS_PATH dir, else the
// embedded qrc copy), falling back to qimgv (always embedded) if the selected
// preset resolves nowhere — e.g. a config naming a preset not compiled into this
// build. The active mapping in shortcuts.json is never touched by this.
QString resolveSelectedPresetPath() {
    QString path = ShortcutPresetStore::resolvePath(settings->selectedPreset());
    if(path.isEmpty())
        path = ShortcutPresetStore::resolvePath(QStringLiteral("qimgv"));
    return path;
}

// Replace the platform-neutral $Ctrl/$Alt/$Shift tokens in a stored key
// sequence with the actual platform modifier names (Ctrl/Alt/Shift, or the
// Command/Option/Shift symbols on macOS).
QString expandModifierTokens(QString keys) {
    keys.replace(QStringLiteral("$Ctrl"),  InputMap::keyNameCtrl());
    keys.replace(QStringLiteral("$Shift"), InputMap::keyNameShift());
    keys.replace(QStringLiteral("$Alt"),   InputMap::keyNameAlt());
    return keys;
}

QList<ViewMode> shortcutContexts() {
    return {MODE_GLOBAL, MODE_DOCUMENT, MODE_FOLDERVIEW};
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

// Read either the new { action: [keySequence...] } default object or the
// previous { keySequence: action } object, expanding modifier tokens as it goes.
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
    // Defaults are loaded from the selected preset (system PRESETS_PATH dir or
    // embedded qrc, resolved by ShortcutPresetStore). Shared bindings live in
    // MODE_GLOBAL; document/grid entries are only context-specific differences.
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
    insertShortcutObject(root.value("shortcuts").toObject(), global);
    insertShortcutObject(root.value("global").toObject(), global);
    insertShortcutObject(root.value("document").toObject(), document);
    insertShortcutObject(root.value("grid").toObject(), grid);
    // Per-OS overrides: pick the current-OS block (windows/macos/linux), falling
    // back to "default". (Legacy preset files used "apple"; treat it as macos.)
    const QJsonObject platform = root.value("platform").toObject();
    const QString osToken = ShortcutPresetStore::currentOsToken();
    QJsonObject platformSpecific = platform.value(osToken).toObject();
    if(platformSpecific.isEmpty() && osToken == QStringLiteral("macos"))
        platformSpecific = platform.value("apple").toObject();
    if(platformSpecific.isEmpty())
        platformSpecific = platform.value("default").toObject();
    insertPlatformShortcutObject(platformSpecific, global, document, grid);

    actionManager->defaults.insert(MODE_GLOBAL, global);
    actionManager->defaults.insert(MODE_DOCUMENT, document);
    actionManager->defaults.insert(MODE_FOLDERVIEW, grid);
}

//------------------------------------------------------------------------------
void ActionManager::initShortcuts() {
    // First run: no shortcuts.json yet. Materialize it from the selected preset
    // (default qimgv) and record the preset pointer + clean modified flag.
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

    // Merge newer default actions into pre-existing configs, per context, so
    // upgrading users get them without resetting all shortcuts.
    auto mergeMissing = [](const QString &action, const QString &shortcut) {
        QMap<ViewMode, QStringList> disabled;
        settings->readDisabledShortcuts(disabled);
        for(ViewMode ctx : shortcutContexts()) {
            if(actionManager->defaults.value(ctx).key(action).isEmpty())
                continue;
            if(disabled.value(ctx).contains(action))
                continue;
            ContextMap &m = actionManager->shortcuts[ctx];
            if(!m.values().contains(action) && !m.contains(shortcut))
                m.insert(shortcut, action);
        }
    };
    mergeMissing("toggleStatusFooter", InputMap::keyNameCtrl() + "+B");
    mergeMissing("cutFile", InputMap::keyNameCtrl() + "+X");
    mergeMissing("togglePlacesPanel", InputMap::keyNameCtrl() + "+E");
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
ViewMode ActionManager::currentContext() const {
    return context;
}
//------------------------------------------------------------------------------
QString ActionManager::contextToString(ViewMode context) {
    if(context == MODE_GLOBAL)
        return QStringLiteral("global");
    return (context == MODE_FOLDERVIEW) ? QStringLiteral("grid")
                                        : QStringLiteral("document");
}
//------------------------------------------------------------------------------
ViewMode ActionManager::contextFromString(const QString &name) {
    if(name == QStringLiteral("global"))
        return MODE_GLOBAL;
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
    rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
// Removes all shortcuts for the specified action.
void ActionManager::removeAllShortcuts(QString actionName) {
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
    if(inputMap->keys().contains(scanCode)) {
        return inputMap->keys()[scanCode];
    }
    return "";
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
void ActionManager::resetDefaults(QString action) {
    actionManager->removeAllShortcuts(action);
    for(ViewMode ctx : shortcutContexts()) {
        QMapIterator<QString, QString> i(defaults[ctx]);
        while(i.hasNext()) {
            i.next();
            if(i.value() == action) {
                shortcuts[ctx].insert(i.key(), i.value());
                qDebug() << "[ActionManager] new action " << i.value() << " - assigning as [" << i.key() << "] in" << contextToString(ctx);
            }
        }
    }
    rebuildShortcutLookup();
}
//------------------------------------------------------------------------------
void ActionManager::adjustFromVersion(QVersionNumber lastVer) {
    // swap Ctrl-P & P
    if(lastVer < QVersionNumber(0,9,2)) {
        actionManager->resetDefaults("print");
        actionManager->resetDefaults("openSettings");
    }
    // swap WheelUp/WheelDown. derp
    if(lastVer < QVersionNumber(1,0,1)) {
        qDebug() << "[actionManager]: swapping WheelUp/WheelDown";
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
    // drop the old MiddleButton=exit default: a single stray middle-click quit
    // the whole app with no confirmation. Only strip it if still bound to exit,
    // so anyone who deliberately rebound MiddleButton keeps their choice.
    if(lastVer < QVersionNumber(2026,7,3)) {
        for(ViewMode ctx : shortcutContexts()) {
            if(shortcuts[ctx].value("MiddleButton") == "exit") {
                shortcuts[ctx].remove("MiddleButton");
                qDebug() << "[actionManager]: removed MiddleButton=exit in" << contextToString(ctx);
            }
        }
    }
    // add new default actions, per context
    for(ViewMode ctx : shortcutContexts()) {
        ContextMap &cur = shortcuts[ctx];
        QMapIterator<QString, QString> i(defaults[ctx]);
        while(i.hasNext()) {
            i.next();
            if(appActions->getMap().value(i.value()) > lastVer) {
                if(!cur.contains(i.key())) {
                    cur.insert(i.key(), i.value());
                    qDebug() << "[ActionManager] new action " << i.value() << " - assigning as [" << i.key() << "] in" << contextToString(ctx);
                } else if(i.value() != cur.value(i.key())) {
                    qDebug() << "[ActionManager] new action " << i.value() << " - shortcut [" << i.key() << "] already assigned to " << cur.value(i.key());
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
    // Self-correcting dirty flag: modified iff the active mapping differs from the
    // selected preset (defaults holds the loaded selected preset). Reverting to
    // the preset scheme clears it; there is no sticky one-way bit.
    settings->setShortcutsModified(actionManager->shortcuts != actionManager->defaults);
}

// Overwrite the active mapping with a preset and persist. Destructive — the UI
// gates this behind a confirm dialog. Also clears per-action _disabled/_primary
// metadata: those are customizations layered on top of a mapping, same as the
// bindings themselves, so a preset switch resets them too (otherwise a
// previously-disabled action would silently stay disabled under the new preset).
void ActionManager::applyPreset(const QString &id) {
    settings->setSelectedPreset(id);
    initDefaults();                                   // reload defaults from `id`
    actionManager->shortcuts = actionManager->defaults;
    actionManager->rebuildShortcutLookup();
    settings->saveDisabledShortcuts(QMap<ViewMode, QStringList>());
    settings->saveShortcutPrimary(QMap<ViewMode, QMap<QString, QString>>());
    actionManager->saveShortcuts();                   // recomputes modified -> false
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

// returns first shortcut that is found in the given context
const QString ActionManager::shortcutForAction(ViewMode context, QString action) {
    const QString contextKey = shortcuts[context].key(action, "");
    if(!contextKey.isEmpty() || context == MODE_GLOBAL)
        return contextKey;
    return shortcuts[MODE_GLOBAL].key(action, "");
}

// returns every shortcut bound to action across all contexts
const QList<QString> ActionManager::shortcutsForAction(QString action) {
    QList<QString> keys;
    for(auto it = shortcuts.cbegin(); it != shortcuts.cend(); ++it) {
        const QList<QString> ctxKeys = it.value().keys(action);
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
    if(!contextAction.isEmpty() || context == MODE_GLOBAL)
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
void ActionManager::readShortcuts() {
    settings->readShortcuts(shortcuts);
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
    return actionManager->invokeActionForShortcut(actionManager->context, ShortcutBuilder::fromEvent(event));
}
//------------------------------------------------------------------------------
