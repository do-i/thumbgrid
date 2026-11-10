#ifndef TREEVIEWCUSTOM_H
#define TREEVIEWCUSTOM_H

#include <QTreeView>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QScrollBar>
#include <QDebug>
#include "settings.h"

class TreeViewCustom : public QTreeView
{
    Q_OBJECT
public:
    TreeViewCustom(QWidget *parent = nullptr);
    QSize minimumSizeHint() const override;

signals:
    void droppedIn(QStringList, QModelIndex);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void readSettings();
private:
    void updateScrollbarStyle(bool hovered);

    QScrollBar overlayScrollbar;
    QColor mHandleColor, mHandleHoverColor, mHandleIdleColor;
    // handle colour the overlay stylesheet was last built with
    QColor mAppliedHandleColor;

    const int SCROLLBAR_WIDTH = 12;
};

#endif // TREEVIEWCUSTOM_H
