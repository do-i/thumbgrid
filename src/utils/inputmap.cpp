#include "inputmap.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

InputMap *inputMap = nullptr;

namespace {
// Per-OS keymap resource, selected at compile time. macOS ships modifiers only
// (it uses the text-event path, not native scan codes); unknown platforms use an
// empty fallback so ShortcutBuilder::fromEventText() handles them.
QString keymapResourcePath() {
#if defined(_WIN32)
    return QStringLiteral(":/res/keymaps/keymap_windows.json");
#elif defined(__linux__) || defined(__FreeBSD__)
    return QStringLiteral(":/res/keymaps/keymap_linux.json");
#elif defined(__APPLE__)
    return QStringLiteral(":/res/keymaps/keymap_macos.json");
#else
    return QStringLiteral(":/res/keymaps/keymap_fallback.json");
#endif
}
} // namespace

InputMap::InputMap() {
    loadFromResource();
}

InputMap *InputMap::getInstance() {
    if(!inputMap) {
        inputMap = new InputMap();
    }
    return inputMap;
}

const QMap<quint32, QString> &InputMap::keys() {
    return keyMap;
}

const QMap<QString, Qt::KeyboardModifier> &InputMap::modifiers() {
    return modMap;
}

void InputMap::loadFromResource() {
    keyMap.clear();
    modMap.clear();

    const QString path = keymapResourcePath();
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[InputMap] could not open keymap:" << path;
    } else {
        QJsonParseError err;
        const QJsonObject root = QJsonDocument::fromJson(file.readAll(), &err).object();
        if(err.error != QJsonParseError::NoError) {
            qWarning() << "[InputMap] keymap JSON parse error:" << err.errorString();
        } else {
            // native QKeyEvent::nativeScanCode() -> canonical key name
            const QJsonObject keys = root.value("keys").toObject();
            for(auto it = keys.constBegin(); it != keys.constEnd(); ++it)
                keyMap.insert(it.key().toUInt(), it.value().toString());

            // optional per-OS modifier display names (Ctrl/Alt/Shift, or the
            // Command/Option/Shift glyphs on macOS)
            const QJsonObject mods = root.value("modifiers").toObject();
            if(mods.contains("ctrl"))  ctrlName  = mods.value("ctrl").toString();
            if(mods.contains("alt"))   altName   = mods.value("alt").toString();
            if(mods.contains("shift")) shiftName = mods.value("shift").toString();
        }
    }

    modMap.insert(ctrlName,  Qt::ControlModifier);
    modMap.insert(altName,   Qt::AltModifier);
    modMap.insert(shiftName, Qt::ShiftModifier);
}

QString InputMap::keyNameCtrl() {
    return getInstance()->ctrlName;
}

QString InputMap::keyNameAlt() {
    return getInstance()->altName;
}

QString InputMap::keyNameShift() {
    return getInstance()->shiftName;
}
