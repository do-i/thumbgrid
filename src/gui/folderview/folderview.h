#pragma once

#include <QWidget>
#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include "gui/customwidgets/floatingwidgetcontainer.h"
#include "gui/idirectoryview.h"
#include "gui/folderview/foldergridview.h"
#include "gui/folderview/filesystemmodelcustom.h"
#include "gui/folderview/bookmarkswidget.h"
#include "gui/customwidgets/actionbutton.h"
#include "gui/customwidgets/styledcombobox.h"
#include "gui/folderview/fvoptionspopup.h"
#include "gui/folderview/treecontextmenu.h"
#include "gui/folderview/treefolderop.h"

namespace Ui {
    class FolderView;
}

class FolderView : public FloatingWidgetContainer, public IDirectoryView {
    Q_OBJECT
    Q_INTERFACES(IDirectoryView)
public:
    explicit FolderView(QWidget *parent = nullptr);
    ~FolderView() override;

public slots:
    void show();
    void hide();
    void populate(int) override;
    void setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) override;
    void select(QList<int>) override;
    void select(int) override;
    QList<int> selection() override;
    void focusOn(int) override;
    void focusOnSelection() override;
    void setDirectoryPath(QString path) override;
    void insertItem(int index) override;
    void removeItem(int index) override;
    void reloadItem(int index) override;
    void setDragHover(int) override;
    void setSelectionInfoProvider(std::function<SelectionInfo()> provider) override;
    void addItem();
    void onFullscreenModeChanged(bool mode);
    void onSortingChanged(SortingMode mode);
    void startRename(const QString& name);
    void startSearch();
    void navigate(GridNavigation direction);
    // bigger (steps > 0) or smaller thumbnails
    void zoomThumbnails(int steps);
    void setSearchMatched(bool matched);
    // stores the expanded tree folders for the next session
    void saveTreeState();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;

protected slots:
    void onThumbnailSizeChanged(int newSize);
    void onZoomSliderValueChanged(int value);

signals:
    void itemActivated(int) override;
    void thumbnailsRequested(QList<int>, int, bool, bool) override;
    void draggedOut() override;
    void sortingSelected(SortingMode);
    void directorySelected(const QString& path);
    void showFoldersChanged(bool mode);
    void copyUrlsRequested(const QStringList&, const QString& path);
    void moveUrlsRequested(const QStringList&, const QString& path);
    void droppedInto(const QMimeData*, QObject*, int) override;
    void draggedOver(int) override;
    void selectionChanged();
    void convertFormatRequested(const QString& format);
    void renameRequested(const QString& name);
    void searchQueryChanged(const QString& prefix);
    // a tree menu item that Core carries out; arg is e.g. a new name
    void folderOperationRequested(TreeFolderOp op, const QString& path, const QString& arg);

private slots:
    void onSortingSelected(int);
    void readSettings();

    void onTreeViewClicked(QModelIndex index);
    void onDroppedInByIndex(const QStringList &paths, const QModelIndex &index, bool copy);
    void onTreeMiddleClicked(const QModelIndex &index);
    void collapseTree();
    void onTreeRenameCommitted(const QModelIndex &index, const QString &name);
    void onTreeNewFolderCommitted(const QModelIndex &parent, const QString &name);
    void onTreeRowsInserted(const QModelIndex &parent, int first, int last);
    void showNewFolder();
    void onTreeContextMenuRequested(const QModelIndex &index, const QPoint &globalPos);
    void toggleBookmarks();
    void toggleFilesystemView();
    void setPlacesPanel(bool mode);
    void onPlacesPanelButtonChecked(bool mode);
    void onBookmarkClicked(QString dirPath);
    void newBookmark();
    void fsTreeScrollToCurrent();
    void onTreeDirectoryLoaded(const QString &dirPath);
    void onTreeExpanded(const QModelIndex &index);
    void onTreeCollapsed(const QModelIndex &index);

    void onOptionsPopupButtonToggled(bool mode);
    void onOptionsPopupDismissed();
    void onViewModeSelected(FolderViewMode mode);

    void onSplitterMoved();
    void onHomeBtn();
    void onRootBtn();

private:
    // expands the saved children of dir that are listed; once dir is fully listed the rest are dropped
    void restoreExpandedIn(const QString &dir, bool listed);

    Ui::FolderView *ui;
    FileSystemModelCustom *dirModel;
    FVOptionsPopup *optionsPopup;
    // built on the first right-click in the tree
    TreeContextMenu *mTreeMenu = nullptr;
    // a folder just asked for: its row is selected once the model lists it
    QString mPendingNewFolder;
    QMetaObject::Connection mPendingConnection;
    QElapsedTimer popupTimerClutch;
    QPersistentModelIndex mTreeScrollTarget;
    QStringList mTreeScrollWaits;
    // folders expanded in the tree, saved on close
    QSet<QString> mExpandedFolders;
    // last session's expanded folders not restored yet: parent path -> child names
    QHash<QString, QStringList> mRestoreFolders;
    // places panel state last applied from the settings
    // tree filter state last applied from the settings
    bool mShowHidden = false;
    bool mPanelApplied = false;
    bool mPlacesPanel = false, mBookmarksExpanded = false, mTreeExpanded = false;
    int mPanelWidth = 0;
};
