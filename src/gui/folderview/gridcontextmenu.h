// Right-click popup for the grid (folder) view.
// Reuses the ContextMenuItem component so it matches the document-view menu:
// each entry shows an icon, a short label and its primary key shortcut.

#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include "gui/customwidgets/contextmenuitem.h"
#include "gui/idirectoryview.h"

class GridContextMenu : public QWidget {
    Q_OBJECT
public:
    explicit GridContextMenu(QWidget *parent = nullptr);

    // Gates each file-op entry on what's selected: Convert needs every item to
    // be (or contain) a convertible image, Rename needs exactly one item,
    // Move/Trash/Delete need at least one. View toggles and Settings are
    // always enabled.
    void setSelectionInfo(const SelectionInfo &info);

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
    ContextMenuItem *renameItem;
    ContextMenuItem *moveItem;
    ContextMenuItem *trashItem;
    ContextMenuItem *deleteItem;

    ContextMenuItem *makeItem(const QString &text, const QString &iconPath);
    void addConvertFormat(QVBoxLayout *layout, const QString &label, const QString &format);
    void addSeparator(QWidget *page, QVBoxLayout *layout);
    void setCurrentPage(int index);
    void switchToMainPage();
    void switchToConvertPage();
    void clampToScreen(QRect &geom);
};
