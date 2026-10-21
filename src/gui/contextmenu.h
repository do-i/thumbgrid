#pragma once

#include <QScreen>
#include <QStyleOption>
#include <QTimer>
#include <QPainter>
#include <QPointer>
#include "components/actionmanager/actionmanager.h"
#include "components/scriptmanager/scriptmanager.h"
#include "gui/customwidgets/contextmenuitem.h"

namespace Ui {
class ContextMenu;
}

class ContextMenu : public QWidget {
    Q_OBJECT
public:
    explicit ContextMenu(QWidget *parent = nullptr);
    ~ContextMenu() override;
    void setImageEntriesEnabled(bool mode);

public slots:
    void showAt(QPoint pos);
    void setGeometry(QRect geom);

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
    void mousePressEvent(QMouseEvent*) override;
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
private slots:
    void switchToMainPage();
    void switchToScriptsPage();
};
