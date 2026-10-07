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

signals:
    void resizeFinished(int resized, int skipped, int failed);

private:
    bool confirmFileOperation(const QString& action, QStringList paths, const QString& destDirectory);
    bool confirmRemovePossible(const QStringList& paths, bool trash);
    void doInteractiveCopyMove(QString path, QString destDirectory, bool move, DialogResult &overwriteFiles);
    void doInteractiveOp(const std::function<void(bool, FileOpResult &)> &op,
                         const QString &srcPath, const QString &dstPath, DialogResult &overwriteFiles);
    void outputError(const FileOpResult &error) const;
    void reportResize(int resized, int skipped, int failed);

    MW *mw;
    std::shared_ptr<DirectoryModel> model;
    std::function<FileOpResult(QString, bool)> removeFileHandler;
    QThread *resizeThread = nullptr;
    std::atomic_bool cancelResize{false};
};
