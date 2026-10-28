#include "shortcutbuilder.h"

QString ShortcutBuilder::fromEvent(QInputEvent *event) {
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
    QWheelEvent *wheelEvent = dynamic_cast<QWheelEvent *>(event);
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(event);
    if(keyEvent) {
        return processKeyEvent(keyEvent);
    } else if(wheelEvent) {
        return processWheelEvent(wheelEvent);
    } else if(mouseEvent) {
        return processMouseEvent(mouseEvent);
    }
    return "";
}
//------------------------------------------------------------------------------
QString ShortcutBuilder::processWheelEvent(QWheelEvent *event) {
    QString sequence;
    if(event->angleDelta() == QPoint(0,0))
        return "";
    if(event->angleDelta().ry() < 0)
        sequence = "WheelDown";
    else if(event->angleDelta().ry() > 0)
        sequence = "WheelUp";
    if(event->angleDelta().rx() < 0) // fallback to horizontal
        sequence = "WheelDown";
    else if(event->angleDelta().rx() > 0)
        sequence = "WheelUp";
    // turned with the right button held (zooms the picture by default)
    if(event->buttons() & Qt::RightButton)
        sequence.prepend("RMB+");
    sequence.prepend(modifierKeys(event));
    return sequence;
}
//------------------------------------------------------------------------------
// double click is LMB only; others count as a click
QString ShortcutBuilder::buttonName(Qt::MouseButton button) {
    if(button == Qt::LeftButton)
        return "LMB";
    if(button == Qt::RightButton)
        return "RMB";
    if(button == Qt::MiddleButton)
        return "MiddleButton";
    if(button == Qt::XButton1)
        return "XButton1";
    if(button == Qt::XButton2)
        return "XButton2";
    return "";
}
//------------------------------------------------------------------------------
QString ShortcutBuilder::fromDrag(QMouseEvent *press, bool withModifiers) {
    const QString button = buttonName(press->button());
    if(button.isEmpty())
        return "";
    return (withModifiers ? modifierKeys(press) : QString()) + button + "_Drag";
}
//------------------------------------------------------------------------------
QString ShortcutBuilder::processMouseEvent(QMouseEvent *event) {
    QString sequence = buttonName(event->button());

    sequence.prepend(modifierKeys(event));

    if(event->type() == QEvent::MouseButtonDblClick) {
        sequence.append("_DoubleClick");
        return sequence;
    }
    if((event->type() == QEvent::MouseButtonPress   && event->button() != Qt::RightButton) ||
       (event->type() == QEvent::MouseButtonRelease && event->button() == Qt::RightButton))
    {
        return sequence;
    }

    return "";
}
//------------------------------------------------------------------------------
QString ShortcutBuilder::processKeyEvent(QKeyEvent *event) {
    if(event->type() != QEvent::KeyPress || isModifier(Qt::Key(event->key())))
        return "";
#if defined(__linux__) || defined(__FreeBSD__) || defined(_WIN32)
    // synthetic events (tests, accessibility tools) carry no scancode
    if(event->nativeScanCode() == 0)
        return fromEventWithoutScanCode(event);
    return fromEventNativeScanCode(event);
#else
    return fromEventText(event);
#endif
}
//------------------------------------------------------------------------------
QString ShortcutBuilder::modifierKeys(QInputEvent *event){
    QString mods;
    QMapIterator<QString, Qt::KeyboardModifier> i(inputMap->modifiers());
    while(i.hasNext()) {
        i.next();
        if(event->modifiers().testFlag(i.value()))
            mods.append(i.key() + "+");
    }
    return mods;
}
//------------------------------------------------------------------------------
bool ShortcutBuilder::isModifier(Qt::Key key) {
    if(key == Qt::Key_Control ||
       key == Qt::Key_Super_L ||
       key == Qt::Key_Super_R ||
       key == Qt::Key_AltGr   ||
       key == Qt::Key_Shift   ||
       key == Qt::Key_Meta    ||
       key == Qt::Key_Alt     )
    {
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
QString ShortcutBuilder::fromEventNativeScanCode(QKeyEvent *event) {
    // layout-independent (mostly): a key with a letter on one layout and a symbol on another may not match
    QString sequence = inputMap->keys().value(event->nativeScanCode());

    if(sequence.isEmpty())
        return sequence;

    QString eventText = event->text();

    QChar keyChr = eventText.isEmpty() ? QChar() : eventText.at(0);

    // use alt characters accessed by shift (punctuation on the numbers row etc.)
    bool useAltChr = (event->modifiers() == Qt::ShiftModifier) && !(keyChr.isLetter() || !keyChr.isPrint() || keyChr.isSpace());
    if(useAltChr) {
        sequence = eventText;
    } else if(!sequence.isEmpty()) {
        sequence.prepend(modifierKeys(event));
    }

    return sequence;
}
//------------------------------------------------------------------------------
// punctuation is named by its character; shifted characters stand alone
QString ShortcutBuilder::fromEventWithoutScanCode(QKeyEvent *event) {
    const QString text = event->text();
    const QChar chr = text.size() == 1 ? text.at(0) : QChar();
    if(chr.isPrint() && !chr.isLetterOrNumber() && !chr.isSpace())
        return (event->modifiers() == Qt::ShiftModifier) ? text : modifierKeys(event) + text;
    return fromEventText(event);
}
//------------------------------------------------------------------------------
QString ShortcutBuilder::fromEventText(QKeyEvent *event) {
    // layout-dependent fallback without native scancodes: binds work on the layout they were made on
    QString sequence = QVariant::fromValue(Qt::Key(event->key())).toString();
    if(!sequence.isEmpty()) {
        // remove "Key_" at the beginning
        sequence.remove(0,4);
        // rename some keys to match the ones from inputmap
        // just a bandaid
        if(sequence == "Return")
            sequence = "Enter";
        else if(sequence == "Escape")
            sequence = "Esc";
    } else {
        // got an unknown key (usually something from non-eng layout)
        // use it's text value instead
        sequence = QKeySequence(event->key()).toString();
    }
    if(!sequence.isEmpty())
        sequence.prepend(modifierKeys(event));
    return sequence;
}
