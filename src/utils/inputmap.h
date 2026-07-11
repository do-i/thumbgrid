#pragma once

#include <QMap>
#include <QString>

class InputMap {
public:
    InputMap();
    static InputMap *getInstance();
    const QMap<quint32, QString> &keys();
    const QMap<QString, Qt::KeyboardModifier> &modifiers();
    static QString keyNameCtrl();
    static QString keyNameAlt();
    static QString keyNameShift();

private:
    // Loads the native scan-code map and modifier display names from the
    // per-OS keymap resource (src/res/keymaps/keymap_<os>.json) selected at
    // compile time. Replaces the old hardcoded initKeyMap()/initModMap().
    void loadFromResource();
    QMap<quint32, QString> keyMap;
    QMap<QString, Qt::KeyboardModifier> modMap;
    QString ctrlName = "Ctrl", altName = "Alt", shiftName = "Shift";
};

extern InputMap *inputMap;
