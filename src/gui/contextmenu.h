#pragma once

#include <QTimer>
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
    // the zoom and transform rows are icon-only; name each with its shortcut
    void updateToolTips();

signals:
    void showScriptSettings();

protected:
    void prepareShow() override;
private slots:
    void switchToMainPage();
    void switchToScriptsPage();
};
