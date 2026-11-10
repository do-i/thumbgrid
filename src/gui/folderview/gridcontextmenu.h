// Grid right-click popup, styled like the document-view menu.

#pragma once

#include <QStackedWidget>
#include "gui/popupmenubase.h"
#include "gui/idirectoryview.h"

class GridContextMenu : public PopupMenuBase {
    Q_OBJECT
public:
    explicit GridContextMenu(QWidget *parent = nullptr);

    // Convert needs convertible images, Rename exactly one item, Move/Trash/Delete at least one
    void setSelectionInfo(const SelectionInfo &info);

signals:
    void convertFormatRequested(QString format);

protected:
    void prepareShow() override;

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

    void addConvertFormat(QVBoxLayout *layout, const QString &label, const QString &format);
    void setCurrentPage(int index);
    void switchToMainPage();
    void switchToConvertPage();
};
