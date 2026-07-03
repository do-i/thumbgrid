#include "actionmanager.h"

ActionManager *actionManager = nullptr;

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
    // Built once, then seeded into every context. Today every shortcut works in
    // every screen, so the two contexts start identical; users differentiate them
    // per-context from the shortcut editor. There is no global fallback.
    ContextMap d;
    d.insert("Right", "nextImage");
    d.insert("Left", "prevImage");
    d.insert("XButton2", "nextImage");
    d.insert("XButton1", "prevImage");
    d.insert("WheelDown", "nextImage");
    d.insert("WheelUp", "prevImage");
    d.insert("F", "toggleFullscreen");
    d.insert("F11", "toggleFullscreen");
    d.insert("LMB_DoubleClick", "toggleFullscreen");
    d.insert("Space", "toggleFitMode");
    d.insert("1", "fitWindow");
    d.insert("2", "fitWidth");
    d.insert("3", "fitNormal");
    d.insert("4", "fitWindowStretch");
    d.insert("R", "resize");
    d.insert("H", "flipH");
    d.insert("V", "flipV");
    d.insert(InputMap::keyNameCtrl() + "+R", "rotateRight");
    d.insert(InputMap::keyNameCtrl() + "+L", "rotateLeft");
    d.insert(InputMap::keyNameCtrl() + "+WheelUp", "zoomInCursor");
    d.insert(InputMap::keyNameCtrl() + "+WheelDown", "zoomOutCursor");
    d.insert("=", "zoomIn"); // [=+] key on the number row
    d.insert(InputMap::keyNameCtrl() + "+=", "zoomIn");
    d.insert("+", "zoomIn");
    d.insert(InputMap::keyNameCtrl() + "++", "zoomIn");
    d.insert("-", "zoomOut");
    d.insert(InputMap::keyNameCtrl() + "+-", "zoomOut");
    d.insert(InputMap::keyNameCtrl() + "+Down", "zoomOut");
    d.insert(InputMap::keyNameCtrl() + "+Up", "zoomIn");
    d.insert("Up", "scrollUp");
    d.insert("Down", "scrollDown");
    d.insert(InputMap::keyNameCtrl() + "+O", "open");
    d.insert(InputMap::keyNameCtrl() + "+S", "save");
    d.insert(InputMap::keyNameCtrl() + "+" + InputMap::keyNameShift() + "+S", "saveAs");
    d.insert(InputMap::keyNameCtrl() + "+W", "setWallpaper");
    d.insert("X", "crop");
    d.insert(InputMap::keyNameCtrl() + "+P", "print");
    d.insert(InputMap::keyNameAlt() + "+X", "exit");
    d.insert(InputMap::keyNameCtrl() + "+Q", "exit");
    d.insert("Esc", "closeFullScreenOrExit");
    d.insert("Del", "moveToTrash");
    d.insert(InputMap::keyNameShift() + "+Del", "removeFile");
    d.insert("C", "copyFile");
    d.insert("M", "moveFile");
    d.insert("Home", "jumpToFirst");
    d.insert("End", "jumpToLast");
    d.insert(InputMap::keyNameCtrl() + "+Right", "seekVideoForward");
    d.insert(InputMap::keyNameCtrl() + "+Left", "seekVideoBackward");
    d.insert(",", "frameStepBack");
    d.insert(".", "frameStep");
    d.insert("Enter", "folderView");
    d.insert("Backspace", "folderView");
    d.insert("F5", "reloadImage");
    d.insert(InputMap::keyNameCtrl() + "+C", "copyFileClipboard");
    d.insert(InputMap::keyNameCtrl() + "+" + InputMap::keyNameShift() + "+C", "copyPathClipboard");
    d.insert("F2", "renameFile");
    d.insert("F7", "createDirectory");
    d.insert("RMB", "contextMenu");
    d.insert("Menu", "contextMenu");
    d.insert("I", "toggleImageInfo");
    d.insert(InputMap::keyNameCtrl() + "+`", "toggleShuffle");
    d.insert(InputMap::keyNameCtrl() + "+D", "showInDirectory");
    d.insert("`", "toggleSlideshow");
    d.insert(InputMap::keyNameCtrl() + "+Z", "discardEdits");
    d.insert("U", "folderView");
    d.insert(InputMap::keyNameCtrl() + "+T", "toggleFolderViewTopBar");
    d.insert(InputMap::keyNameCtrl() + "+B", "toggleStatusFooter");
    d.insert(InputMap::keyNameShift() + "+Right", "nextDirectory");
    d.insert(InputMap::keyNameShift() + "+Left", "prevDirectory");
    d.insert(InputMap::keyNameShift() + "+F", "toggleFullscreenInfoBar");
    d.insert(InputMap::keyNameCtrl() + "+V", "pasteFile");
    d.insert(InputMap::keyNameCtrl() + "+X", "cutFile");
    d.insert("F12", "openSettings");

#ifdef __APPLE__
    d.insert(InputMap::keyNameAlt() + "+Up", "zoomIn");
    d.insert(InputMap::keyNameAlt() + "+Down", "zoomOut");
    d.insert(InputMap::keyNameCtrl() + "+Comma", "openSettings");
#else
    d.insert("P", "openSettings");
#endif

    //d.insert("Backspace", "goUp");

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
        for(ViewMode ctx : {MODE_DOCUMENT, MODE_FOLDERVIEW}) {
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
    return (context == MODE_FOLDERVIEW) ? QStringLiteral("folderview")
                                        : QStringLiteral("document");
}
//------------------------------------------------------------------------------
ViewMode ActionManager::contextFromString(const QString &name) {
    return (name == QStringLiteral("folderview")) ? MODE_FOLDERVIEW : MODE_DOCUMENT;
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
    actionManager->validateShortcuts();
}
//------------------------------------------------------------------------------
bool ActionManager::processEvent(QInputEvent *event) {
    return actionManager->invokeActionForShortcut(actionManager->context, ShortcutBuilder::fromEvent(event));
}
//------------------------------------------------------------------------------
