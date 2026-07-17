#pragma once

#include "gui/folderview/folderview.h"
#include "gui/panels/infobar/infobarproxy.h"
#include <QMutexLocker>

struct FolderViewStateBuffer {
    QString directory;
    QList<int> selection;
    int itemCount = 0;
    SortingMode sortingMode;
    bool fullscreenMode;
};

class FolderViewProxy : public QWidget, public IDirectoryView {
    Q_OBJECT
    Q_INTERFACES(IDirectoryView)
public:
    FolderViewProxy(QWidget *parent = nullptr);
    void init();

public slots:
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
    void setStatusText(const QString& text);
    void setStatusFooterVisible(bool mode);

protected:
    void showEvent(QShowEvent *event) override;

signals:
    void itemActivated(int) override;
    void thumbnailsRequested(QList<int>, int, bool, bool) override;
    void draggedOut() override;
    void draggedToBookmarks(QList<int>) override;
    void sortingSelected(SortingMode);
    void showFoldersChanged(bool mode);
    void directorySelected(const QString&);
    void copyUrlsRequested(const QStringList&, const QString& path);
    void moveUrlsRequested(const QStringList&, const QString& path);
    void droppedInto(const QMimeData*, QObject*, int) override;
    void draggedOver(int) override;
    void selectionChanged();
    void convertFormatRequested(const QString& format);

private:
    std::shared_ptr<FolderView> folderView;
    std::shared_ptr<InfoBarProxy> statusFooter;
    QVBoxLayout layout;
    FolderViewStateBuffer stateBuf;
    std::function<SelectionInfo()> selectionInfoProvider;
    QMutex m;
    QString statusText;
    bool statusFooterVisible = false;
};
