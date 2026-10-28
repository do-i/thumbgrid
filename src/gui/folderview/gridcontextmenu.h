// Grid right-click popup, styled like the document-view menu.

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

    // Convert needs convertible images, Rename exactly one item, Move/Trash/Delete at least one
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
    ContextMenuItem *resizeItem;
    ContextMenuItem *renameItem;
    ContextMenuItem *copyPathItem;
    ContextMenuItem *fileInfoItem;
    ContextMenuItem *moveItem;
    ContextMenuItem *trashItem;
    ContextMenuItem *deleteItem;
    ContextMenuItem *findDuplicatesItem;
    ContextMenuItem *slideshowItem;

    ContextMenuItem *makeItem(const QString &text, const QString &iconPath);
    void addConvertFormat(QVBoxLayout *layout, const QString &label, const QString &format);
    void addSeparator(QWidget *page, QVBoxLayout *layout);
    void setCurrentPage(int index);
    void switchToMainPage();
    void switchToConvertPage();
    void clampToScreen(QRect &geom);
};
