#include "fileoperationscontroller.h"

#include <utility>
#include <QDirIterator>
#include "utils/fileoperations.h"
#include "utils/logging.h"
#include "sourcecontainers/documentinfo.h"

FileOperationsController::FileOperationsController(MW *mw, QObject *parent)
    : QObject(parent),
      mw(mw)
{
}

FileOperationsController::~FileOperationsController() {
    // the worker's completion report targets this object: stop after the current file and drain
    if(resizeThread) {
        cancelResize = true;
        resizeThread->wait();
    }
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
    qCWarning(logCore) << FileOperations::decodeResult(error);
}

// canonicalFilePath() is empty for a nonexistent path: canonicalize the deepest existing
// ancestor and re-append the tail, so symlinks in the existing part still resolve
static QString canonicalizeAsFarAsPossible(const QString &path) {
    QFileInfo direct(path);
    QString canonical = direct.canonicalFilePath();
    if(!canonical.isEmpty())
        return canonical;

    QString clean = QDir::cleanPath(direct.absoluteFilePath());
    QStringList parts = clean.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList tail;
    while(!parts.isEmpty()) {
        tail.prepend(parts.takeLast());
        QString ancestor = QLatin1Char('/') + parts.join(QLatin1Char('/'));
        canonical = QFileInfo(ancestor).canonicalFilePath();
        if(!canonical.isEmpty()) {
            // root is "/": avoid doubling the separator
            if(!canonical.endsWith(QLatin1Char('/')))
                canonical += QLatin1Char('/');
            return QDir::cleanPath(canonical + tail.join(QLatin1Char('/')));
        }
    }
    return clean;
}

// A destination inside a source makes a recursive copy/move consume its own output.
// Checked in the interactive executors too, which pasteFile() calls directly.
bool FileOperationsController::destinationIsInsideSource(const QStringList& paths, const QString& destDirectory) {
    QString dest = canonicalizeAsFarAsPossible(destDirectory);
    for(const auto& path : paths) {
        QFileInfo fi(path);
        // only a real directory can contain the destination; links are recreated as links
        if(fi.isSymLink() || !fi.isDir())
            continue;
        QString src = fi.canonicalFilePath();
        if(src.isEmpty())
            continue;
        if(dest == src || dest.startsWith(src + QLatin1Char('/')))
            return true;
    }
    return false;
}

bool FileOperationsController::copyPathsTo(const QStringList& paths, const QString& destDirectory) {
    if(destinationIsInsideSource(paths, destDirectory)) {
        outputError(FileOpResult::DESTINATION_INSIDE_SOURCE);
        return false;
    }
    if(!confirmFileOperation(tr("Copy"), paths, destDirectory))
        return false;
    interactiveCopy(paths, destDirectory);
    return true;
}

bool FileOperationsController::movePathsTo(const QStringList& paths, const QString& destDirectory) {
    if(destinationIsInsideSource(paths, destDirectory)) {
        outputError(FileOpResult::DESTINATION_INSIDE_SOURCE);
        return false;
    }
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

// repeated here: pasteFile() calls this directly, bypassing copyPathsTo()'s check
bool FileOperationsController::interactiveCopy(const QStringList& paths, const QString& destDirectory) {
    if(destinationIsInsideSource(paths, destDirectory)) {
        outputError(FileOpResult::DESTINATION_INSIDE_SOURCE);
        return false;
    }
    DialogResult overwriteFiles;
    for(const auto& path : paths) {
        doInteractiveCopyMove(path, destDirectory, false, overwriteFiles);
        if(overwriteFiles.cancel)
            return true;
    }
    return true;
}

bool FileOperationsController::interactiveMove(const QStringList& paths, const QString& destDirectory) {
    if(destinationIsInsideSource(paths, destDirectory)) {
        outputError(FileOpResult::DESTINATION_INSIDE_SOURCE);
        return false;
    }
    DialogResult overwriteFiles;
    for(const auto& path : paths) {
        doInteractiveCopyMove(path, destDirectory, true, overwriteFiles);
        if(overwriteFiles.cancel)
            return true;
    }
    return true;
}

// on DESTINATION_FILE_EXISTS ask (unless an "all" answer is active) and retry with force
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
    QFileInfo dstFi(destDirectory + "/" + srcFi.fileName());
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
        qCWarning(logCore) << "Could not create directory " << dstDir.absolutePath();
        return;
    }
    // copy / move all contents
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
        if(mw->showConfirmation(tr("File exists"), tr("Destination file exists. Overwrite?"), true))
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
        if(!mw->showConfirmation(trash ? tr("Move to trash") : tr("Delete permanently"), msg, true))
            return;
    }
    FileOpResult result;
    int successCount = 0;
    for(const auto& path : paths) {
        QFileInfo fi(path);
        // isDir() resolves links: remove just the link, never its target's contents
        if(!fi.isSymLink() && fi.isDir())
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

    // "jpeg"/"jpg" and "tiff"/"tif" count as the same format
    auto normalizeExt = [](const QString &e) {
        if(e == "jpeg") return QStringLiteral("jpg");
        if(e == "tiff") return QStringLiteral("tif");
        return e;
    };
    QString ext = normalizeExt(format.toLower());

    struct ConvertJob {
        QString src;
        QString dest;
        std::shared_ptr<Image> img;
    };
    // selected folders expand to their direct convertible images
    QStringList expandedPaths = expandSelectedFolders(paths);

    QList<ConvertJob> jobs;
    int skipped = 0;
    bool overwrites = false;

    for(const QString &path : expandedPaths) {
        QFileInfo fi(path);
        QString srcExt = normalizeExt(fi.suffix().toLower());
        // already in the target format
        if(srcExt == ext) {
            skipped++;
            continue;
        }
        auto img = model->getImage(path);
        if(!img || img->type() != STATIC) {
            skipped++;
            continue;
        }
        // skipped, not failed: the file is fine, the format just can't encode this size
        if(!DocumentInfo::canEncodeToFormat(ext, QSize(img->width(), img->height()))) {
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
            tr("Some files already exist and will be overwritten.\n\nContinue?"), true))
        return;

    int converted = 0, failed = 0;
    for(const ConvertJob &job : jobs) {
        bool ok;
        if(model->containsFile(job.src)) {
            // make sure the image is cached so DirectoryModel::saveFile can access it
            model->updateImage(job.src, job.img);
            ok = model->saveFile(job.src, job.dest);
        } else {
            // files from a subfolder aren't model entries: save directly
            ok = job.img->save(job.dest);
        }
        if(ok)
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

QStringList FileOperationsController::expandSelectedFolders(const QStringList& paths) {
    QStringList expandedPaths;
    for(const QString &path : paths) {
        if(QFileInfo(path).isDir()) {
            QDirIterator it(path, QDir::Files | QDir::Hidden);
            while(it.hasNext()) {
                QString entry = it.next();
                if(DocumentInfo::isConvertibleImageFile(entry))
                    expandedPaths << entry;
            }
        } else {
            expandedPaths << path;
        }
    }
    return expandedPaths;
}

bool FileOperationsController::isResizeRunning() const {
    return resizeThread != nullptr;
}

// Copies only: originals and cached images are never written; new files arrive via the watcher.
void FileOperationsController::resizeToCopies(const QStringList& paths, const ResizeSpec& spec) {
    if(isResizeRunning()) {
        mw->showMessage(tr("A resize is already running"));
        return;
    }
    QStringList files = expandSelectedFolders(paths);
    if(files.isEmpty()) {
        mw->showMessage(tr("Nothing to resize"));
        return;
    }
    auto tally = [](ResizeCopy::Outcome outcome, int &resized, int &skipped, int &failed) {
        switch(outcome) {
            case ResizeCopy::Outcome::Resized: resized++; break;
            case ResizeCopy::Outcome::Skipped: skipped++; break;
            case ResizeCopy::Outcome::Failed:  failed++;  break;
        }
    };
    if(files.size() == 1) {
        int resized = 0, skipped = 0, failed = 0;
        tally(ResizeCopy::resizeFile(files.first(), spec).outcome, resized, skipped, failed);
        reportResize(resized, skipped, failed);
        return;
    }

    mw->showMessage(tr("Resizing %1 images...").arg(files.size()));
    cancelResize = false;
    resizeThread = QThread::create([this, files, spec, tally]() {
        int resized = 0, skipped = 0, failed = 0;
        QSet<QString> reserved;
        for(const QString &path : files) {
            if(cancelResize)
                break;
            tally(ResizeCopy::resizeFile(path, spec, &reserved).outcome, resized, skipped, failed);
        }
        QMetaObject::invokeMethod(this, [this, resized, skipped, failed]() {
            resizeThread = nullptr;
            reportResize(resized, skipped, failed);
        }, Qt::QueuedConnection);
    });
    connect(resizeThread, &QThread::finished, resizeThread, &QObject::deleteLater);
    resizeThread->start(QThread::LowPriority);
}

void FileOperationsController::reportResize(int resized, int skipped, int failed) {
    if(resized && !skipped && !failed)
        mw->showMessageSuccess(tr("Resized %1 file(s)").arg(resized));
    else if(resized)
        mw->showWarning(tr("Resized %1, skipped %2, failed %3").arg(resized).arg(skipped).arg(failed));
    else if(failed)
        mw->showError(tr("Could not resize file(s)"));
    else
        mw->showMessage(tr("Nothing to resize - images are not larger than the target"));
    emit resizeFinished(resized, skipped, failed);
}
