#include "contextmenuitem.h"

#include <utility>

ContextMenuItem::ContextMenuItem(QWidget *parent)
    : MenuItem(parent),
      mAction("")
{
}

ContextMenuItem::~ContextMenuItem() {
}

void ContextMenuItem::setAction(QString text) {
    this->mAction = std::move(text);
    setShortcutText(actionManager->shortcutForAction(actionManager->currentContext(), mAction));
}

bool ContextMenuItem::refreshShortcut() {
    if(mAction.isEmpty() || mShortcutHidden)
        return false;
    const QString text = actionManager->shortcutForAction(actionManager->currentContext(), mAction);
    if(text == shortcut())
        return false;
    setShortcutText(text);
    return true;
}

void ContextMenuItem::hideShortcut() {
    mShortcutHidden = true;
    setShortcutText("");
}

void ContextMenuItem::trigger() {
    onPress();
}

void ContextMenuItem::onPress() {
    emit pressed();
    if(!mAction.isEmpty())
        actionManager->invokeAction(mAction);
}
