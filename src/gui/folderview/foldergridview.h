#pragma once

#include <QGraphicsWidget>
#include <QContextMenuEvent>
#include <QLabel>
#include <QLineEdit>

#include "gui/customwidgets/thumbnailview.h"
#include "gui/customwidgets/thumbnailwidget.h"
#include "gui/flowlayout.h"
#include "gui/folderview/gridcontextmenu.h"
#include "components/actionmanager/actionmanager.h"

class FolderGridView : public ThumbnailView {
    Q_OBJECT
public:
    explicit FolderGridView(QWidget *parent = nullptr);

    const int THUMBNAIL_SIZE_MIN = 80;  // px
    const int THUMBNAIL_SIZE_MAX = 400;  // these should be divisible by ZOOM_STEP
    const int ZOOM_STEP = 20;
    void selectAll();

    // begins editing the current selection's name in place, over its cell
    void startRename(const QString& name);

    // type-ahead search: `/` starts it, typed characters extend the prefix.
    // While it is active every key belongs to the search, so no regular
    // shortcut fires. Escape (or backspacing over the leading `/`) leaves it.
    bool searchMode() const;
    QString searchQuery() const;
    void exitSearchMode();
    // feedback for the last emitted query, pushed back down once it is resolved
    void setSearchMatched(bool matched);

public slots:
    void show();
    void hide();
    void populate(int count) override;
    void setDirectoryPath(QString path) override;

    void selectFirst();
    void selectLast();
    void pageUp();
    void pageDown();
    void selectAbove();
    void selectBelow();
    void selectNext();
    void selectPrev();

    void zoomIn();
    void zoomOut();
    void setThumbnailSize(int newSize);
    void setShowLabels(bool mode);
    void setShowInfo(bool mode);
    void setLabelFontPointSize(int size);
    void setLabelBackgroundColor(const QColor &color);
    void setCellBackgroundColor(const QColor &color);
    void focusOn(int index) override;
    void focusOnSelection() override;
    void setDragHover(int index) override;

private:
    FlowLayout *flowLayout;
    GridContextMenu *contextMenu = nullptr;
    QGraphicsWidget holderWidget;
    int shiftedCol;
    void scrollToCurrent();
    int lastDragTarget = -1;
    bool mPreviewFit = false;
    QString mThumbColorSignature;

    QLineEdit *renameEditor = nullptr;
    int renameIndex = -1;
    void positionRenameEditor();

    bool mSearchMode = false;
    bool mSearchMatched = true;
    QString mSearchQuery;
    QLabel *searchIndicator = nullptr;
    void enterSearchMode();
    void updateSearchIndicator();
    void positionSearchIndicator();
    // returns true if the event was consumed by the active search
    bool handleSearchKey(QKeyEvent *event);

private slots:
    void onitemSelected();
    void commitRename();
    void cancelRename();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void updateScrollbarIndicator() override;
    void addItemToLayout(ThumbnailWidget *widget, int pos) override;
    void removeItemFromLayout(int pos) override;
    void removeAll() override;
    void setupLayout();
    ThumbnailWidget *createThumbnailWidget() override;
    void updateLayout() override;
    void fitSceneToContents() override;

    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    bool focusNextPrevChild(bool) override;
    bool eventFilter(QObject *o, QEvent *ev) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hideEvent(QHideEvent *event) override;

signals:
    void thumbnailSizeChanged(int);
    void convertFormatRequested(const QString& format);
    void renameRequested(const QString& name);
    void searchQueryChanged(const QString& prefix);
};
