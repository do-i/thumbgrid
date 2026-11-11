#ifndef BOOKMARKSITEM_H
#define BOOKMARKSITEM_H

#include <QObject>
#include <QWidget>
#include <QStyleOption>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QMimeData>
#include <QLabel>
#include <QLineEdit>
#include "gui/customwidgets/iconbutton.h"
#include "gui/customwidgets/clickablelabel.h"
#include "settings.h"
#include <QMouseEvent>

class BookmarksItem : public QWidget {
    Q_OBJECT
public:
    explicit BookmarksItem(QString _dirName, QString _dirPath, QWidget *parent = nullptr);
    QString path();
    QString label() const { return dirName; }
    void setLabel(const QString& name);
    // inline label editor: Enter commits (empty resets the label), Esc or losing focus cancels
    void startRename();
    bool isRenaming() const { return mEditor && !mEditor->isHidden(); }

public slots:
    void setHighlighted(bool mode);

signals:
    void clicked(const QString& dirPath);
    void removeClicked(const QString& dirPath);
    void contextMenuRequested(const QString& dirPath, const QPoint& globalPos);
    void renameCommitted(const QString& dirPath, const QString& label);
    // the pointer moved past the drag distance with the button down
    void reorderDragRequested(const QString& dirPath);
    // a bookmark drag over this row or dropped on it; pos is in the parent's coordinates
    void reorderDragMoved(const QPoint& pos);
    void reorderDragLeft();
    void reorderDropped(const QString& dirPath, const QPoint& pos);
    // copy: Ctrl was held, otherwise the files are to be moved
    void droppedIn(const QStringList& paths, const QString& dirPath, bool copy);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    void paintEvent(QPaintEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onRemoveClicked();

private:
    void endRename(bool commit);
    QString dirName, dirPath;

    IconWidget folderIconWidget;
    IconButton removeItemButton;
    QLabel dirNameLabel;
    QLineEdit *mEditor = nullptr; // built on the first rename
    QSpacerItem *spacer;
    QHBoxLayout layout;
    bool mHighlighted;
    bool mPressed = false, mDragged = false;
    QPoint mPressPos;
};

#endif // BOOKMARKSITEM_H
