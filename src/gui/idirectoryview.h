#pragma once

#include <QtPlugin>
#include <QList>
#include <functional>
#include <memory>

class Thumbnail;
class QString;
class QMimeData;

// Cheap, extension-based summary of the current selection, used to gate
// context-menu entries. allConvertible means every selected item is a
// convertible image file or a folder containing at least one.
struct SelectionInfo {
    int fileCount = 0;
    int dirCount = 0;
    bool allConvertible = false;
    int total() const { return fileCount + dirCount; }
};

class IDirectoryView {
public:
    virtual ~IDirectoryView() {}

    virtual void populate(int) = 0;
    virtual void setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) = 0;
    virtual void select(QList<int>) = 0;
    virtual void select(int) = 0;
    virtual void focusOn(int) = 0;
    virtual void focusOnSelection() = 0;
    virtual QList<int> selection() = 0;
    virtual void setDirectoryPath(QString path) = 0;
    virtual void insertItem(int index) = 0;
    virtual void removeItem(int index) = 0;
    virtual void reloadItem(int index) = 0;
    virtual void setDragHover(int index) = 0;
    virtual void setSelectionInfoProvider(std::function<SelectionInfo()>) {}

//signals
    virtual void itemActivated(int) = 0;
    virtual void thumbnailsRequested(QList<int>, int, bool, bool) = 0;
    virtual void draggedOut() = 0;
    virtual void draggedToBookmarks(QList<int>) = 0;
    virtual void draggedOver(int) = 0;
    virtual void droppedInto(const QMimeData*, QObject*, int) = 0;
};

Q_DECLARE_INTERFACE(IDirectoryView, "IDirectoryView")
