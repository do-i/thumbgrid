#pragma once

#include <QHash>
#include <QObject>
#include <QThreadPool>
#include "cache/cache.h"
#include "directorymanager/directorymanager.h"
#include "scaler/scaler.h"
#include "loader/loader.h"
#include "utils/fileoperations.h"
#include "utils/imagewriter.h"

class ImageStatic;

class DirectoryModel : public QObject {
    Q_OBJECT
public:
    explicit DirectoryModel(QObject *parent = nullptr);
    ~DirectoryModel() override;

    Scaler *scaler;

    void load(const QString& filePath, bool asyncHint);
    void preload(const QString& filePath);

    int fileCount() const;
    int dirCount() const;
    int indexOfFile(QString filePath) const;
    int indexOfDir(QString filePath) const;
    QString fileNameAt(int index) const;
    bool containsFile(QString filePath) const;
    bool isEmpty() const;
    QString nextOf(QString filePath) const;
    QString prevOf(QString filePath) const;
    QString firstFile() const;
    QString lastFile() const;
    QDateTime lastModified(QString filePath) const;

    bool forceInsert(const QString& filePath);
    void copyFileTo(const QString &srcFile, const QString &destDirPath, bool force, FileOpResult &result);
    void moveFileTo(const QString &srcFile, const QString &destDirPath, bool force, FileOpResult &result);
    void copySymLinkTo(const QString &srcLink, const QString &destDirPath, bool force, FileOpResult &result);
    void moveSymLinkTo(const QString &srcLink, const QString &destDirPath, bool force, FileOpResult &result);
    void renameEntry(const QString &oldFilePath, const QString &newName, bool force, FileOpResult &result);
    void removeFile(const QString &filePath, bool trash, FileOpResult &result);
    void removeDir(const QString &dirPath, bool trash, bool recursive, FileOpResult &result);
    void createDirectory(const QString &dirPath, FileOpResult &result);

    bool setDirectory(QString);

    void unload(int index);

    bool loaderBusy() const;

    std::shared_ptr<Image> getImage(const QString& filePath);

    void updateImage(const QString& filePath, const std::shared_ptr<Image>& img);

    void setSortingMode(SortingMode mode);
    SortingMode sortingMode() const;

    QString directoryPath() const;
    void unload(QString filePath);
    bool isLoaded(int index) const;
    bool isLoaded(QString filePath) const;
    void reload(const QString& filePath);
    QString filePathAt(int index) const;
    void unloadExcept(const QString& filePath, bool keepNearby);
    const FSEntry &fileEntryAt(int index) const;

    int totalCount() const;
    QString dirNameAt(int index) const;
    QString dirPathAt(int index) const;


    bool saveFile(const QString &filePath);
    bool saveFile(const QString &filePath, const QString &destPath);
    // Writes filePath's image to destPath on a worker thread; saveFinished()
    // follows. False when no write was started (unsupported image or format,
    // or filePath is already being saved).
    bool saveFileAsync(const QString &filePath, const QString &destPath);
    bool isSaving(const QString &filePath) const;

    bool containsDir(QString dirPath) const;
    FileListSource source();
signals:
    void fileRemoved(const QString& filePath, int index);
    void fileRenamed(const QString& fromPath, int indexFrom, const QString& toPath, int indexTo);
    void fileAdded(const QString& filePath);
    void fileModified(const QString& filePath);
    void dirRemoved(const QString& dirPath, int index);
    void dirRenamed(const QString& dirPath, int indexFrom, const QString& toPath, int indexTo);
    void dirAdded(const QString& dirPath);
    void loaded(const QString& filePath);
    void loadFailed(const QString &path);
    void sortingChanged(SortingMode);
    void imageReady(const std::shared_ptr<Image>& img, const QString&);
    void imageUpdated(const QString& filePath);
    void saveFinished(const QString& filePath, const QString& destPath, bool success);

private:
    DirectoryManager dirManager;
    Loader loader;
    Cache cache;
    FileListSource fileListSource;

    struct PendingSave {
        std::shared_ptr<ImageStatic> img;
        ImageWriteJob job;
    };
    // keyed by the saved file's path
    QHash<QString, PendingSave> pendingSaves;
    QThreadPool savePool;

    void onFileWritten(const QString &filePath, const QString &destPath);
    void finishSave(const QString &filePath, bool success);
    bool isSaveTarget(const QString &destPath) const;

private slots:
    void onImageReady(const std::shared_ptr<Image>& img, const QString &path);
    void onSortingChanged();
    void onFileAdded(QString filePath);
    void onFileRemoved(const QString& filePath, int index);
    void onFileRenamed(const QString& fromPath, int indexFrom, QString toPath, int indexTo);
    void onFileModified(const QString& filePath);
};
