#include "actionmanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

ActionManager *actionManager = nullptr;

namespace {

// Prefer the installed system copy (admin-editable), fall back to the copy
// embedded in the binary via resources.qrc so dev/non-Linux builds work.
// Mirrors ThemeStore::resolveThemePath().
QString resolveDefaultShortcutsPath() {
#ifdef SHORTCUTS_PATH
    const QString systemPath = QStringLiteral(SHORTCUTS_PATH) + "/default-shortcuts.json";
    if(QFile::exists(systemPath))
        return systemPath;
#endif
    return QStringLiteral(":/res/default-shortcuts.json");
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

// Read a { keySequence: action } object into the shared default map, expanding
// modifier tokens as it goes.
void insertShortcutObject(const QJsonObject &obj, ActionManager::ContextMap &out) {
    for(auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        out.insert(expandModifierTokens(it.key()), it.value().toString());
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
    // Defaults are loaded from default-shortcuts.json (installed system copy,
    // qrc fallback), then seeded into every context. Today every shortcut works
    // in every screen, so the two contexts start identical; users differentiate
    // them per-context from the shortcut editor. There is no global fallback.
    const QString path = resolveDefaultShortcutsPath();
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ActionManager] could not open default shortcuts:" << path;
        return;
    }
    QJsonParseError err;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll(), &err).object();
    if(err.error != QJsonParseError::NoError) {
        qWarning() << "[ActionManager] default shortcuts JSON parse error:" << err.errorString();
        return;
    }

    ContextMap d;
    insertShortcutObject(root.value("shortcuts").toObject(), d);
    const QJsonObject platform = root.value("platform").toObject();
#ifdef __APPLE__
    insertShortcutObject(platform.value("apple").toObject(), d);
#else
    insertShortcutObject(platform.value("default").toObject(), d);
#endif

    // Seed each context with the full default set.
    actionManager->defaults.insert(MODE_DOCUMENT, d);
    actionManager->defaults.insert(MODE_FOLDERVIEW, d);
}

//------------------------------------------------------------------------------
void ActionManager::initShortcuts() {
    actionManager->readShortcuts();
    bool empty = true;
    for(auto it = actionManager->shortcuts.cbegin(); it != actionManager->shortcuts.cend(); ++it)
        if(!it.value().isEmpty()) { empty = false; break; }
    if(empty)
        actionManager->resetDefaults();

    // Merge newer default actions into pre-existing configs, per context, so
    // upgrading users get them without resetting all shortcuts.
    auto mergeMissing = [](const QString &action, const QString &shortcut) {
        QMap<ViewMode, QStringList> disabled;
        settings->readDisabledShortcuts(disabled);
        for(ViewMode ctx : {MODE_DOCUMENT, MODE_FOLDERVIEW}) {
            if(disabled.value(ctx).contains(action))
                continue;
            ContextMap &m = actionManager->shortcuts[ctx];
            if(!m.values().contains(action) && !m.contains(shortcut))
                m.insert(shortcut, action);
        }
    };
    mergeMissing("toggleStatusFooter", InputMap::keyNameCtrl() + "+B");
    mergeMissing("cutFile", InputMap::keyNameCtrl() + "+X");
}
//------------------------------------------------------------------------------
void ActionManager::addShortcut(ViewMode context, const QString &keys, const QString &action) {
    ActionType type = validateAction(action);
    if(type != ActionType::ACTION_INVALID) {
        actionManager->shortcuts[context].insert(keys, action);
    }
}
//------------------------------------------------------------------------------
void ActionManager::removeShortcut(ViewMode context, const QString &keys) {
    actionManager->shortcuts[context].remove(keys);
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
    return (context == MODE_FOLDERVIEW) ? QStringLiteral("grid")
                                        : QStringLiteral("document");
}
//------------------------------------------------------------------------------
ViewMode ActionManager::contextFromString(const QString &name) {
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
}
//------------------------------------------------------------------------------
// Removes all shortcuts for specified action. Slow (reverse map lookup).
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
}
//------------------------------------------------------------------------------
void ActionManager::resetDefaults(ViewMode context) {
    actionManager->shortcuts[context] = actionManager->defaults.value(context);
}
//------------------------------------------------------------------------------
void ActionManager::resetDefaults(QString action) {
    actionManager->removeAllShortcuts(action);
    for(ViewMode ctx : {MODE_DOCUMENT, MODE_FOLDERVIEW}) {
        QMapIterator<QString, QString> i(defaults[ctx]);
        while(i.hasNext()) {
            i.next();
            if(i.value() == action) {
                shortcuts[ctx].insert(i.key(), i.value());
                qDebug() << "[ActionManager] new action " << i.value() << " - assigning as [" << i.key() << "] in" << contextToString(ctx);
            }
        }
    }
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
        for(ViewMode ctx : {MODE_DOCUMENT, MODE_FOLDERVIEW}) {
            if(shortcuts[ctx].value("MiddleButton") == "exit") {
                shortcuts[ctx].remove("MiddleButton");
                qDebug() << "[actionManager]: removed MiddleButton=exit in" << contextToString(ctx);
            }
        }
    }
    // add new default actions, per context
    for(ViewMode ctx : {MODE_DOCUMENT, MODE_FOLDERVIEW}) {
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
    saveShortcuts();
}
//------------------------------------------------------------------------------
void ActionManager::saveShortcuts() {
    settings->saveShortcuts(actionManager->shortcuts);
}
//------------------------------------------------------------------------------
QString ActionManager::actionForShortcut(ViewMode context, const QString &keys) {
    return actionManager->shortcuts[context].value(keys);
}

// returns first shortcut that is found in the given context
const QString ActionManager::shortcutForAction(ViewMode context, QString action) {
    return shortcuts[context].key(action, "");
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
    const ContextMap &m = shortcuts[context];
    if(!shortcut.isEmpty() && m.contains(shortcut)) {
        return invokeAction(m.value(shortcut));
    }
    return false;
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
