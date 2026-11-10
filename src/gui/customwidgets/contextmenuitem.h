// A menu item that executes an action on press

#pragma once

#include "gui/customwidgets/menuitem.h"
#include "components/actionmanager/actionmanager.h"

class ContextMenuItem : public MenuItem {
    Q_OBJECT
public:
    ContextMenuItem(QWidget *parent = nullptr);
    ~ContextMenuItem() override;
    void setAction(QString mAction);
    // re-reads the bound shortcut for the active input context; true if the text changed
    bool refreshShortcut();
    // keeps the shortcut hint empty, also across refreshes
    void hideShortcut();
    // runs the item as a press would (keyboard activation)
    void trigger();

signals:
    void pressed();

protected:
    void onPress() override;

private:
    QString mAction;
    bool mShortcutHidden = false;
};
