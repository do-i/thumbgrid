#pragma once

#include <QObject>
#include <functional>
#include <memory>
#include "components/directorymodel.h"
#include "gui/mainwindow.h"

// Interactive file operations: the copy / move / remove / convert flows
// together with their confirmation and replace dialogs. Owned by Core.
// Knows nothing about viewer state - Core supplies the selection explicitly
// and a removeFile handler that closes playing media before deletion.
class FileOperationsController : public QObject {
    Q_OBJECT

public:
    explicit FileOperationsController(MW *mw, QObject *parent = nullptr);

    void setModel(std::shared_ptr<DirectoryModel> newModel);
    void setRemoveFileHandler(std::function<FileOpResult(QString, bool)> handler);

public slots:
    bool copyPathsTo(QStringList paths, QString destDirectory);
    bool movePathsTo(QStringList paths, QString destDirectory);
    void interactiveCopy(QStringList paths, QString destDirectory);
    void interactiveMove(QStringList paths, QString destDirectory);
    FileOpResult copyOrMoveFile(const QString &path, const QString &destDirectory, bool move);
    void removePaths(QStringList paths, bool trash);
    void convertToFormat(QStringList paths, QString format);

private:
    bool confirmFileOperation(QString action, QStringList paths, QString destDirectory);
    bool confirmRemovePossible(QStringList paths, bool trash);
    void doInteractiveCopyMove(QString path, QString destDirectory, bool move, DialogResult &overwriteFiles);
    void doInteractiveOp(const std::function<void(bool, FileOpResult &)> &op,
                         const QString &srcPath, const QString &dstPath, DialogResult &overwriteFiles);
    void outputError(const FileOpResult &error) const;

    MW *mw;
    std::shared_ptr<DirectoryModel> model;
    std::function<FileOpResult(QString, bool)> removeFileHandler;
};
