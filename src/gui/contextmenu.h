#pragma once

#include <QTimer>
#include <QPointer>
#include "components/scriptmanager/scriptmanager.h"
#include "gui/popupmenubase.h"

namespace Ui {
class ContextMenu;
}

class ContextMenu : public PopupMenuBase {
    Q_OBJECT
public:
    explicit ContextMenu(QWidget *parent = nullptr);
    ~ContextMenu() override;
    void setImageEntriesEnabled(bool mode);

private:
    Ui::ContextMenu *ui;
    QTimer mTimer;

    void fillOpenWithMenu();
    // keyboard navigation over the rows of the current page
    QPointer<ContextMenuItem> mHighlighted;
    QList<ContextMenuItem*> navigableItems();
    void setHighlighted(ContextMenuItem *item);
    void moveHighlight(int step);
    void activateHighlighted();
    // the zoom and transform rows are icon-only; name each with its shortcut
    void updateToolTips();

signals:
    void showScriptSettings();

protected:
    void prepareShow() override;
    void keyPressEvent(QKeyEvent *event) override;
private slots:
    void switchToMainPage();
    void switchToScriptsPage();
};
