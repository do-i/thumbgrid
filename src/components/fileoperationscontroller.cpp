#include "fileoperationscontroller.h"

#include <utility>
#include "utils/fileoperations.h"

FileOperationsController::FileOperationsController(MW *mw, QObject *parent)
    : QObject(parent),
      mw(mw)
{
}

void FileOperationsController::setModel(std::shared_ptr<DirectoryModel> newModel) {
    model = std::move(newModel);
}

void FileOperationsController::setRemoveFileHandler(std::function<FileOpResult(QString, bool)> handler) {
    removeFileHandler = std::move(handler);
}

void FileOperationsController::outputError(const FileOpResult &error) const {
    if(error == FileOpResult::SUCCESS || error == FileOpResult::NOTHING_TO_DO)
        return;
    mw->showError(FileOperations::decodeResult(error));
    qDebug() << FileOperations::decodeResult(error);
}

bool FileOperationsController::copyPathsTo(const QStringList& paths, const QString& destDirectory) {
    if(!confirmFileOperation(tr("Copy"), paths, destDirectory))
        return false;
    interactiveCopy(paths, destDirectory);
    return true;
}

bool FileOperationsController::movePathsTo(const QStringList& paths, const QString& destDirectory) {
    if(!confirmFileOperation(tr("Move"), paths, destDirectory))
        return false;
    interactiveMove(paths, destDirectory);
    return true;
}

bool FileOperationsController::confirmFileOperation(const QString& action, QStringList paths, const QString& destDirectory) {
    if(paths.isEmpty())
        return false;

    QString destination = QDir::toNativeSeparators(destDirectory);
    QString msg;
    if(paths.count() == 1) {
        QFileInfo fi(paths.first());
        QString itemName = fi.fileName();
        if(itemName.isEmpty())
            itemName = paths.first();
        msg = tr("%1 \"%2\" to \"%3\"?").arg(action, itemName, destination);
    } else {
        msg = tr("%1 %2 items to \"%3\"?").arg(action).arg(paths.count()).arg(destination);
    }

    return mw->showConfirmation(action, msg);
}

void FileOperationsController::interactiveCopy(const QStringList& paths, const QString& destDirectory) {
    DialogResult overwriteFiles;
    for(const auto& path : paths) {
        doInteractiveCopyMove(path, destDirectory, false, overwriteFiles);
        if(overwriteFiles.cancel)
            return;
    }
}

void FileOperationsController::interactiveMove(const QStringList& paths, const QString& destDirectory) {
    DialogResult overwriteFiles;
    for(const auto& path : paths) {
        doInteractiveCopyMove(path, destDirectory, true, overwriteFiles);
        if(overwriteFiles.cancel)
            return;
    }
}

// Single copy/move attempt; on DESTINATION_FILE_EXISTS asks via the replace
// dialog (unless an "all" answer is active) and retries with force.
void FileOperationsController::doInteractiveOp(const std::function<void(bool, FileOpResult &)> &op,
                                               const QString &srcPath, const QString &dstPath,
                                               DialogResult &overwriteFiles) {
    FileOpResult result;
    op(overwriteFiles, result);
    if(result == FileOpResult::DESTINATION_FILE_EXISTS) {
        if(overwriteFiles.all) // skipping all
            return;
        overwriteFiles = mw->fileReplaceDialog(srcPath, dstPath, FILE_TO_FILE, true);
        if(!overwriteFiles || overwriteFiles.cancel)
            return;
        op(true, result);
    }
    if(!(result == FileOpResult::DESTINATION_FILE_EXISTS && !overwriteFiles))
        outputError(result);
    if(!overwriteFiles.all) // attempt done; reset temporary flag
        overwriteFiles.yes = false;
}

// todo: replacing DIR with a FILE?
void FileOperationsController::doInteractiveCopyMove(QString path, QString destDirectory, bool move, DialogResult &overwriteFiles) {
    QFileInfo srcFi(path);
    QString dstPath = destDirectory + "/" + srcFi.fileName();
// SYMLINK (operate on the link itself, never dereference into the target) =====================
    if(srcFi.isSymLink()) {
        doInteractiveOp([&](bool force, FileOpResult &result) {
            if(move)
                model->moveSymLinkTo(path, destDirectory, force, result);
            else
                FileOperations::copySymLinkTo(path, destDirectory, force, result);
        }, srcFi.absoluteFilePath(), dstPath, overwriteFiles);
        return;
    }
// SINGLE FILE ================================================================================
    if(!srcFi.isDir()) {
        doInteractiveOp([&](bool force, FileOpResult &result) {
            if(move)
                model->moveFileTo(path, destDirectory, force, result);
            else
                FileOperations::copyFileTo(path, destDirectory, force, result);
        }, srcFi.absoluteFilePath(), dstPath, overwriteFiles);
        return;
    }
// DIR (RECURSIVE) ============================================================================
    QDir srcDir(srcFi.absoluteFilePath());
    QFileInfo dstFi(destDirectory + "/" + srcFi.baseName());
    QDir dstDir(dstFi.absoluteFilePath());
    if(dstFi.exists() && !dstFi.isDir()) { // overwriting file with a folder
        if(!overwriteFiles && !overwriteFiles.all) {
            overwriteFiles = mw->fileReplaceDialog(srcFi.absoluteFilePath(), dstFi.absoluteFilePath(), DIR_TO_FILE, true);
            if(!overwriteFiles || overwriteFiles.cancel)
                return;
            if(!overwriteFiles.all) // reset temp flag right away
                overwriteFiles.yes = false;
        }
        // remove dst file; give up if not writable
        FileOpResult result;
        FileOperations::removeFile(dstFi.absoluteFilePath(), result);
        if(result != FileOpResult::SUCCESS) {
            outputError(result);
            return;
        }
    } else if(!dstDir.mkpath(".")) {
        mw->showError(tr("Could not create directory ") + dstDir.absolutePath());
        qDebug() << "Could not create directory " << dstDir.absolutePath();
        return;
    }
    // copy / move all contents
    // TODO: skip symlinks? test
    QStringList entryList = srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for(const auto& entry : entryList) {
        doInteractiveCopyMove(srcDir.absolutePath() + "/" + entry, dstDir.absolutePath(), move, overwriteFiles);
        if(overwriteFiles.cancel)
            return;
    }
    if(move) {
        FileOpResult dirRmRes;
        model->removeDir(srcDir.absolutePath(), false, false, dirRmRes);
    }
}

// Copies or moves a single file, asking about an existing destination.
FileOpResult FileOperationsController::copyOrMoveFile(const QString &path, const QString &destDirectory, bool move) {
    FileOpResult result;
    auto op = [&](bool force) {
        if(move)
            model->moveFileTo(path, destDirectory, force, result);
        else
            model->copyFileTo(path, destDirectory, force, result);
    };
    op(false);
    if(result == FileOpResult::SUCCESS) {
        mw->showMessageSuccess(move ? tr("File moved.") : tr("File copied."));
    } else if(result == FileOpResult::DESTINATION_FILE_EXISTS) {
        if(mw->showConfirmation(tr("File exists"), tr("Destination file exists. Overwrite?")))
            op(true);
    }
    return result;
}

bool FileOperationsController::confirmRemovePossible(const QStringList& paths, bool trash) {
    FileOpResult result;
    for(const auto &path : paths) {
        FileOperations::checkCanRemove(path, result);
        if(result == FileOpResult::SUCCESS)
            continue;

        const QString title = trash ? tr("Cannot move to trash") : tr("Cannot delete");
        QString msg = FileOperations::decodeResult(result);
        msg += "\n\n" + QDir::toNativeSeparators(path);
        mw->showErrorDialog(title, msg);
        return false;
    }
    return true;
}

void FileOperationsController::removePaths(const QStringList& paths, bool trash) {
    if(!paths.count())
        return;
    if(!confirmRemovePossible(paths, trash))
        return;
    if(trash ? settings->confirmTrash() : settings->confirmDelete()) {
        QString msg;
        if(trash)
            msg = (paths.count() > 1) ? tr("Move ") + QString::number(paths.count()) + tr(" items to trash?")
                                      : tr("Move item to trash?");
        else
            msg = (paths.count() > 1) ? tr("Delete ") + QString::number(paths.count()) + tr(" items permanently?")
                                      : tr("Delete item permanently?");
        if(!mw->showConfirmation(trash ? tr("Move to trash") : tr("Delete permanently"), msg))
            return;
    }
    FileOpResult result;
    int successCount = 0;
    for(const auto& path : paths) {
        QFileInfo fi(path);
        if(fi.isDir())
            model->removeDir(path, trash, true, result);
        else
            result = removeFileHandler(path, trash);
        if(result == FileOpResult::SUCCESS)
            successCount++;
    }
    if(paths.count() == 1) {
        if(result == FileOpResult::SUCCESS)
            mw->showMessageSuccess(trash ? tr("Moved to trash") : tr("File removed"));
        else
            outputError(result);
    } else if(paths.count() > 1) {
        if(trash)
            mw->showMessageSuccess(tr("Moved to trash: ") + QString::number(successCount) + tr(" files"));
        else
            mw->showMessageSuccess(tr("Removed: ") + QString::number(successCount) + tr(" files"));
    }
}

void FileOperationsController::convertToFormat(const QStringList& paths, const QString& format) {
    if(!model || paths.isEmpty())
        return;

    // normalize the target extension
    QString ext = format.toLower();
    if(ext == "jpeg")
        ext = "jpg";

    struct ConvertJob {
        QString src;
        QString dest;
        std::shared_ptr<Image> img;
    };
    QList<ConvertJob> jobs;
    int skipped = 0;
    bool overwrites = false;

    for(const QString &path : paths) {
        QFileInfo fi(path);
        QString srcExt = fi.suffix().toLower();
        // already in the target format
        if(srcExt == ext || (ext == "jpg" && srcExt == "jpeg")) {
            skipped++;
            continue;
        }
        auto img = model->getImage(path);
        if(!img || img->type() != STATIC) {
            skipped++;
            continue;
        }
        QString dest = fi.absolutePath() + "/" + fi.completeBaseName() + "." + ext;
        if(QFileInfo::exists(dest))
            overwrites = true;
        jobs.append({path, dest, img});
    }

    if(jobs.isEmpty()) {
        mw->showMessage(tr("Nothing to convert"));
        return;
    }
    if(overwrites && !mw->showConfirmation(tr("Convert"),
            tr("Some files already exist and will be overwritten.\n\nContinue?")))
        return;

    int converted = 0, failed = 0;
    for(const ConvertJob &job : jobs) {
        // make sure the image is cached so DirectoryModel::saveFile can access it
        model->updateImage(job.src, job.img);
        if(model->saveFile(job.src, job.dest))
            converted++;
        else
            failed++;
    }

    if(converted && !failed)
        mw->showMessageSuccess(tr("Converted %1 file(s)").arg(converted));
    else if(converted && failed)
        mw->showWarning(tr("Converted %1, failed %2").arg(converted).arg(failed));
    else
        mw->showError(tr("Could not convert file(s)"));
}
