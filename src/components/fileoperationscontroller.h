#pragma once

#include <QObject>
#include <QThread>
#include <atomic>
#include <functional>
#include <memory>
#include "components/directorymodel.h"
#include "gui/mainwindow.h"
#include "utils/resizecopy.h"

// Interactive file operations: the copy / move / remove / convert flows
// together with their confirmation and replace dialogs. Owned by Core.
// Knows nothing about viewer state - Core supplies the selection explicitly
// and a removeFile handler that closes playing media before deletion.
struct CopyMoveEnv;

class FileOperationsController : public QObject {
    Q_OBJECT

public:
    explicit FileOperationsController(MW *mw, QObject *parent = nullptr);
    ~FileOperationsController() override;

    void setModel(std::shared_ptr<DirectoryModel> newModel);
    void setRemoveFileHandler(std::function<FileOpResult(QString, bool)> handler);

    // true if destDirectory is a source directory or lives beneath one; static
    // and public so the containment rule can be tested without a live window
    static bool destinationIsInsideSource(const QStringList& paths, const QString& destDirectory);
    // Replaces each selected directory with its directly contained convertible
    // images (non-recursive); plain file paths pass through unchanged.
    static QStringList expandSelectedFolders(const QStringList& paths);

    bool isResizeRunning() const;
    // a move across filesystems copies and deletes on a worker thread; one at a time
    bool isMoveRunning() const;

public slots:
    bool copyPathsTo(const QStringList& paths, const QString& destDirectory);
    bool movePathsTo(const QStringList& paths, const QString& destDirectory);
    // false if the containment rule refused the operation and nothing was done;
    // callers must not report success without checking.
    bool interactiveCopy(const QStringList& paths, const QString& destDirectory);
    bool interactiveMove(const QStringList& paths, const QString& destDirectory);
    FileOpResult copyOrMoveFile(const QString &path, const QString &destDirectory, bool move);
    void removePaths(const QStringList& paths, bool trash);
    void convertToFormat(const QStringList& paths, const QString& format);
    // Writes a resized copy of each image next to its original. One image runs
    // inline; more run on a worker thread and resizeFinished fires when done.
    void resizeToCopies(const QStringList& paths, const ResizeSpec& spec);
    // stops a background move after the file it is on; every file is then in exactly one place
    void cancelMove();

signals:
    void resizeFinished(int resized, int skipped, int failed);
    // moved = top-level items that left their source
    void moveFinished(int moved, int failures, bool cancelled);

private:
    bool confirmFileOperation(const QString& action, QStringList paths, const QString& destDirectory);
    bool confirmRemovePossible(const QStringList& paths, bool trash);
    CopyMoveEnv guiEnv(bool move) const;
    // movedHere/selected: the instant same-filesystem part of the same request, for the summary
    void startBackgroundMove(const QStringList& paths, const QString& destDirectory, DialogResult overwriteFiles,
                             int movedHere, int selected);
    void finishBackgroundMove(const QStringList& paths, int failures, bool cancelled, int movedHere, int selected);
    DialogResult askReplaceFromWorker(const QString &src, const QString &dst, FileReplaceMode mode);
    void outputError(const FileOpResult &error) const;
    void reportResize(int resized, int skipped, int failed);

    MW *mw;
    std::shared_ptr<DirectoryModel> model;
    std::function<FileOpResult(QString, bool)> removeFileHandler;
    QThread *resizeThread = nullptr;
    std::atomic_bool cancelResize{false};
    QThread *moveThread = nullptr;
    std::atomic_bool cancelMoveRequested{false};
};
