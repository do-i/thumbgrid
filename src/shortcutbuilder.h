#pragma once

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include "utils/inputmap.h"

class ShortcutBuilder {
public:
    static QString fromEvent(QInputEvent *event);
    // The binding name of a drag started by this press: LMB_Drag, RMB_Drag or
    // MiddleButton_Drag, after any held modifiers unless withModifiers is false.
    static QString fromDrag(QMouseEvent *press, bool withModifiers = true);

private:
    static QString processWheelEvent(QWheelEvent *event);
    static QString processMouseEvent(QMouseEvent *event);
    static QString processKeyEvent(QKeyEvent *event);
    static QString modifierKeys(QInputEvent *event);
    static QString buttonName(Qt::MouseButton button);
    static bool isModifier(Qt::Key key);
    static QString fromEventText(QKeyEvent *event);
    static QString fromEventNativeScanCode(QKeyEvent *event);
    static QString fromEventWithoutScanCode(QKeyEvent *event);
};
