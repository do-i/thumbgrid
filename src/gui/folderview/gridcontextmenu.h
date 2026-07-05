// Right-click popup for the grid (folder) view.
// Reuses the ContextMenuItem component so it matches the document-view menu:
// each entry shows an icon, a short label and its primary key shortcut.

#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include "gui/customwidgets/contextmenuitem.h"

class GridContextMenu : public QWidget {
    Q_OBJECT
public:
    explicit GridContextMenu(QWidget *parent = nullptr);

    // Convert entries only make sense with a selection.
    void setImageEntriesEnabled(bool mode);

public slots:
    void showAt(QPoint pos);

signals:
    void convertFormatRequested(QString format);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QStackedWidget *stack;
    ContextMenuItem *convertItem;

    ContextMenuItem *makeItem(const QString &text, const QString &iconPath);
    void addConvertFormat(QVBoxLayout *layout, const QString &label, const QString &format);
    void switchToMainPage();
    void switchToConvertPage();
    void clampToScreen(QRect &geom);
};
