#ifndef TREEVIEWCUSTOM_H
#define TREEVIEWCUSTOM_H

#include <QLineEdit>
#include <QPersistentModelIndex>
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

    // inline name editor over a row: Enter commits, Esc or losing focus cancels
    void startRename(const QModelIndex &index, const QString &name);
    // the editor sits where the first child of parent would be
    void startNewFolder(const QModelIndex &parent, const QString &name);
    bool isEditing() const { return mEditMode != EditMode::None; }

signals:
    void droppedIn(QStringList, QModelIndex);
    // right-click on a row, or the menu key on the current row; the row is not selected
    void renameCommitted(const QModelIndex &index, const QString &name);
    void newFolderCommitted(const QModelIndex &parent, const QString &name);
    void middleClicked(const QModelIndex &index);
    void contextMenuRequestedAt(const QModelIndex &index, const QPoint &globalPos);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void readSettings();
private:
    void updateScrollbarStyle(bool hovered);
    void requestMenuForCurrent();
    enum class EditMode { None, Rename, NewFolder };
    void beginEdit(EditMode mode, const QModelIndex &index, const QString &name);
    void positionEditor();
    void commitEdit();
    void cancelEdit();

    QLineEdit *mEditor = nullptr; // built on the first edit
    EditMode mEditMode = EditMode::None;
    QPersistentModelIndex mEditIndex; // the renamed row, or the parent of the new folder

    QScrollBar overlayScrollbar;
    QColor mHandleColor, mHandleHoverColor, mHandleIdleColor;
    // handle colour the overlay stylesheet was last built with
    QColor mAppliedHandleColor;

    const int SCROLLBAR_WIDTH = 12;
};

#endif // TREEVIEWCUSTOM_H
