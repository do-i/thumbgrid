#pragma once

#include <QObject>
#include <memory>
#include "gui/idirectoryview.h"
#include "components/thumbnailer/thumbnailer.h"
#include "directorymodel.h"
#include "sharedresources.h"
#include <QMimeData>

class DirectoryPresenter : public QObject {
    Q_OBJECT
public:
    explicit DirectoryPresenter(QObject *parent = nullptr);

    void setView(std::shared_ptr<IDirectoryView>);
    void setModel(const std::shared_ptr<DirectoryModel>& newModel);
    void unsetModel();

    void selectAndFocus(int index);
    void selectAndFocus(const QString& path);

    void onFileRemoved(const QString& filePath, int index);
    void onFileRenamed(const QString& fromPath, int indexFrom, const QString& toPath, int indexTo);
    void onFileAdded(QString filePath);
    void onFileModified(QString filePath);

    void onDirRemoved(const QString& dirPath, int index);
    void onDirRenamed(const QString& fromPath, int indexFrom, const QString& toPath, int indexTo);
    void onDirAdded(QString dirPath);

    bool showDirs();
    void setShowDirs(bool mode);
    void setShowParentDir(bool mode);

    QStringList selectedPaths() const;
    SelectionInfo selectionInfo() const;


signals:
    void dirActivated(const QString& dirPath);
    void parentDirActivated();
    void fileActivated(const QString& filePath);
    void draggedOut(const QStringList&);
    void droppedInto(const QStringList&, const QString&);
    void statusTextChanged(const QString& text);

public slots:
    void disconnectView();
    void reloadModel();

private slots:
    void generateThumbnails(const QList<int>&, int, bool, bool);
    void onThumbnailReady(std::shared_ptr<Thumbnail> thumb, QString filePath);
    void onDirThumbnailReady(std::shared_ptr<Thumbnail> thumb, QString dirPath);
    void populateView();
    void onItemActivated(int absoluteIndex);
    void onDraggedOut();
    void onDraggedOver(int index);
    void onSelectionChanged();

    void onDroppedInto(const QMimeData *data, QObject *source, int targetIndex);
private:
    std::shared_ptr<IDirectoryView> view = nullptr;
    std::shared_ptr<DirectoryModel> model = nullptr;
    Thumbnailer thumbnailer;
    bool mShowDirs;
    bool mShowParentDir = false;

    bool hasParentDir() const;
    int parentOffset() const;
    QString parentDirPath() const;
    std::shared_ptr<Thumbnail> createParentDirThumbnail(int size);
    int realObjectCount() const;
    QString statusText() const;
    void emitStatusText();
};
