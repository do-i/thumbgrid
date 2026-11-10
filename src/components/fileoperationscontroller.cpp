#include "fileoperationscontroller.h"

#include <utility>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QSemaphore>
#include "utils/fileoperations.h"
#include "utils/logging.h"
#include "sourcecontainers/documentinfo.h"

FileOperationsController::FileOperationsController(MW *mw, QObject *parent)
    : QObject(parent),
      mw(mw)
{
    if(mw)
        connect(mw, &MW::moveCancelRequested, this, &FileOperationsController::cancelMove);
}

FileOperationsController::~FileOperationsController() {
    // the worker's completion report targets this object: stop after the current file and drain
    if(resizeThread) {
        cancelResize = true;
        resizeThread->wait();
    }
    // a background move stops after the file it is on; nothing is lost, the rest stays in the source
    if(moveThread) {
        cancelMoveRequested = true;
        moveThread->wait();
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
    // before the confirmation: confirming and then being refused is worse than not being asked
    if(isMoveRunning()) {
        mw->showMessage(tr("A move is already running"));
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

// What one recursive copy/move walk needs from its caller. The GUI-thread walk talks to the dialogs
// and the directory model directly; the background move only touches the filesystem and marshals
// prompts and errors to the GUI thread.
struct CopyMoveEnv {
    bool move = false;
    std::function<void(const QString &, const QString &, bool, FileOpResult &)> fileOp;
    std::function<void(const QString &, const QString &, bool, FileOpResult &)> symLinkOp;
    std::function<void(const QString &, const QString &, FileOpResult &)> dirRename;  // move only
    std::function<void(const QString &, FileOpResult &)> removeEmptyDir;              // move only
    std::function<DialogResult(const QString &, const QString &, FileReplaceMode)> askReplace;
    std::function<void(FileOpResult)> reportError;
    std::function<void(const QString &)> reportMkdirFailure;
    std::function<bool()> cancelled = [] { return false; };
    std::function<void()> entryDone = [] {};
};

namespace {

// on DESTINATION_FILE_EXISTS ask (unless an "all" answer is active) and retry with force
void runWithReplacePrompt(const CopyMoveEnv &env, const std::function<void(bool, FileOpResult &)> &op,
                          const QString &srcPath, const QString &dstPath, DialogResult &overwriteFiles) {
    FileOpResult result;
    op(overwriteFiles, result);
    if(result == FileOpResult::DESTINATION_FILE_EXISTS) {
        if(overwriteFiles.all) // skipping all
            return;
        overwriteFiles = env.askReplace(srcPath, dstPath, FILE_TO_FILE);
        if(!overwriteFiles || overwriteFiles.cancel)
            return;
        op(true, result);
    }
    if(!(result == FileOpResult::DESTINATION_FILE_EXISTS && !overwriteFiles))
        env.reportError(result);
    if(!overwriteFiles.all) // attempt done; reset temporary flag
        overwriteFiles.yes = false;
}

void copyMoveEntry(const CopyMoveEnv &env, const QString &path, const QString &destDirectory, DialogResult &overwriteFiles) {
    if(env.cancelled()) {
        overwriteFiles.cancel = true;
        return;
    }
    QFileInfo srcFi(path);
    QString dstPath = destDirectory + "/" + srcFi.fileName();
// SYMLINK (operate on the link itself, never dereference into the target) =====================
    if(srcFi.isSymLink()) {
        runWithReplacePrompt(env, [&](bool force, FileOpResult &result) {
            env.symLinkOp(path, destDirectory, force, result);
        }, srcFi.absoluteFilePath(), dstPath, overwriteFiles);
        env.entryDone();
        return;
    }
// SINGLE FILE ================================================================================
    if(!srcFi.isDir()) {
        runWithReplacePrompt(env, [&](bool force, FileOpResult &result) {
            env.fileOp(path, destDirectory, force, result);
        }, srcFi.absoluteFilePath(), dstPath, overwriteFiles);
        env.entryDone();
        return;
    }
// DIR (RECURSIVE) ============================================================================
    QDir srcDir(srcFi.absoluteFilePath());
    QFileInfo dstFi(dstPath);
    QDir dstDir(dstFi.absoluteFilePath());
    // a move to a free name is one rename on the same filesystem; walking the tree below instead copies
    // every byte (a ~100-folder, 12 GB move froze the app for minutes). Any failure (different
    // filesystem, permissions) falls through to the walk, which reports errors per entry.
    if(env.move && !dstFi.exists() && !dstFi.isSymLink()) {
        FileOpResult renameRes;
        env.dirRename(srcFi.absoluteFilePath(), destDirectory, renameRes);
        if(renameRes == FileOpResult::SUCCESS)
            return;
    }
    if(dstFi.exists() && !dstFi.isDir()) { // overwriting file with a folder
        if(overwriteFiles.all) {
            if(!overwriteFiles.yes) // "No to all": keep the file, leave the folder in the source
                return;
        } else {
            overwriteFiles = env.askReplace(srcFi.absoluteFilePath(), dstFi.absoluteFilePath(), DIR_TO_FILE);
            if(!overwriteFiles || overwriteFiles.cancel)
                return;
            if(!overwriteFiles.all) // reset temp flag right away
                overwriteFiles.yes = false;
        }
        // remove dst file; give up if not writable
        FileOpResult result;
        FileOperations::removeFile(dstFi.absoluteFilePath(), result);
        if(result != FileOpResult::SUCCESS) {
            env.reportError(result);
            return;
        }
    }
    // also after replacing a file: the folder must exist before its contents go in
    if(!dstDir.mkpath(".")) {
        env.reportMkdirFailure(dstDir.absolutePath());
        return;
    }
    // copy / move all contents
    QStringList entryList = srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for(const auto& entry : entryList) {
        copyMoveEntry(env, srcDir.absolutePath() + "/" + entry, dstDir.absolutePath(), overwriteFiles);
        if(overwriteFiles.cancel)
            return;
    }
    if(env.move) {
        FileOpResult dirRmRes;
        env.removeEmptyDir(srcDir.absolutePath(), dirRmRes);
    }
}

// files and links below (and including) path - what the walk calls entryDone() for
qint64 countWalkEntries(const QString &path, const std::atomic_bool &cancelled) {
    QFileInfo fi(path);
    if(fi.isSymLink() || !fi.isDir())
        return 1;
    qint64 count = 0;
    QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while(it.hasNext() && !cancelled) {
        it.next();
        if(it.fileInfo().isSymLink() || !it.fileInfo().isDir())
            count++;
    }
    return count;
}

} // namespace

CopyMoveEnv FileOperationsController::guiEnv(bool move) const {
    CopyMoveEnv env;
    env.move = move;
    if(move) {
        DirectoryModel *m = model.get();
        env.fileOp = [m](const QString &s, const QString &d, bool f, FileOpResult &r) { m->moveFileTo(s, d, f, r); };
        env.symLinkOp = [m](const QString &s, const QString &d, bool f, FileOpResult &r) { m->moveSymLinkTo(s, d, f, r); };
        env.dirRename = [m](const QString &s, const QString &d, FileOpResult &r) { m->moveDirTo(s, d, r); };
        env.removeEmptyDir = [m](const QString &p, FileOpResult &r) { m->removeDir(p, false, false, r); };
    } else {
        env.fileOp = &FileOperations::copyFileTo;
        env.symLinkOp = &FileOperations::copySymLinkTo;
    }
    env.askReplace = [this](const QString &s, const QString &d, FileReplaceMode mode) {
        return mw->fileReplaceDialog(s, d, mode, true);
    };
    env.reportError = [this](FileOpResult r) { outputError(r); };
    env.reportMkdirFailure = [this](const QString &dir) {
        mw->showError(tr("Could not create directory ") + dir);
        qCWarning(logCore) << "Could not create directory " << dir;
    };
    return env;
}

// repeated here: pasteFile() calls this directly, bypassing copyPathsTo()'s check
bool FileOperationsController::interactiveCopy(const QStringList& paths, const QString& destDirectory) {
    if(destinationIsInsideSource(paths, destDirectory)) {
        outputError(FileOpResult::DESTINATION_INSIDE_SOURCE);
        return false;
    }
    CopyMoveEnv env = guiEnv(false);
    DialogResult overwriteFiles;
    for(const auto& path : paths) {
        copyMoveEntry(env, path, destDirectory, overwriteFiles);
        if(overwriteFiles.cancel)
            return true;
    }
    return true;
}

// Same-filesystem entries are renamed right here (instant). Entries on another filesystem have to be
// copied and deleted byte by byte, so they go to one background worker instead of freezing the GUI.
bool FileOperationsController::interactiveMove(const QStringList& paths, const QString& destDirectory) {
    if(destinationIsInsideSource(paths, destDirectory)) {
        outputError(FileOpResult::DESTINATION_INSIDE_SOURCE);
        return false;
    }
    if(isMoveRunning()) {
        mw->showMessage(tr("A move is already running"));
        return false;
    }
    CopyMoveEnv env = guiEnv(true);
    DialogResult overwriteFiles;
    QStringList crossFilesystem;
    int movedHere = 0;
    for(const auto& path : paths) {
        if(!FileOperations::onSameFilesystem(path, destDirectory)) {
            crossFilesystem << path;
            continue;
        }
        copyMoveEntry(env, path, destDirectory, overwriteFiles);
        if(!QFileInfo::exists(path) && !QFileInfo(path).isSymLink())
            movedHere++;
        if(overwriteFiles.cancel)
            return true;
    }
    if(!crossFilesystem.isEmpty())
        startBackgroundMove(crossFilesystem, destDirectory, overwriteFiles, movedHere, paths.size());
    return true;
}

bool FileOperationsController::isMoveRunning() const {
    return moveThread != nullptr;
}

void FileOperationsController::cancelMove() {
    cancelMoveRequested = true;
}

// Runs on the worker: shows the replace dialog on the GUI thread and waits for the answer, polling the
// cancel flag so a quit (the destructor waits for the worker) can never deadlock on an open dialog.
DialogResult FileOperationsController::askReplaceFromWorker(const QString &src, const QString &dst, FileReplaceMode mode) {
    struct Pending {
        QSemaphore answered;
        DialogResult result;
    };
    auto pending = std::make_shared<Pending>();
    QMetaObject::invokeMethod(this, [this, pending, src, dst, mode]() {
        if(cancelMoveRequested)
            pending->result.cancel = true;
        else
            pending->result = mw->fileReplaceDialog(src, dst, mode, true);
        pending->answered.release();
    }, Qt::QueuedConnection);
    while(!pending->answered.tryAcquire(1, 100)) {
        if(cancelMoveRequested) {
            DialogResult cancelled;
            cancelled.cancel = true;
            return cancelled;
        }
    }
    return pending->result;
}

void FileOperationsController::startBackgroundMove(const QStringList& paths, const QString& destDirectory,
                                                   DialogResult overwriteFiles, int movedHere, int selected) {
    cancelMoveRequested = false;
    mw->beginMoveProgress();
    moveThread = QThread::create([this, paths, destDirectory, overwriteFiles, movedHere, selected]() mutable {
        qint64 total = 0;
        for(const auto &path : paths)
            total += countWalkEntries(path, cancelMoveRequested);
        QMetaObject::invokeMethod(this, [this, total]() {
            if(moveThread)
                mw->showMoveProgress(0, total);
        }, Qt::QueuedConnection);
        qint64 done = 0;
        int failures = 0;
        QElapsedTimer sinceProgress;
        sinceProgress.start();

        CopyMoveEnv env;
        env.move = true;
        env.fileOp = &FileOperations::moveFileTo;
        env.symLinkOp = &FileOperations::moveSymLinkTo;
        env.dirRename = &FileOperations::moveDirTo; // a folder that lives on dest's filesystem after all
        env.removeEmptyDir = [](const QString &p, FileOpResult &r) { FileOperations::removeDir(p, false, r); };
        env.askReplace = [this](const QString &s, const QString &d, FileReplaceMode mode) {
            return askReplaceFromWorker(s, d, mode);
        };
        env.reportError = [this, &failures](FileOpResult r) {
            if(r == FileOpResult::SUCCESS || r == FileOpResult::NOTHING_TO_DO)
                return;
            failures++;
            QMetaObject::invokeMethod(this, [this, r]() { outputError(r); }, Qt::QueuedConnection);
        };
        env.reportMkdirFailure = [this, &failures](const QString &dir) {
            failures++;
            QMetaObject::invokeMethod(this, [this, dir]() {
                mw->showError(tr("Could not create directory ") + dir);
                qCWarning(logCore) << "Could not create directory " << dir;
            }, Qt::QueuedConnection);
        };
        env.cancelled = [this]() { return cancelMoveRequested.load(); };
        env.entryDone = [this, &done, total, &sinceProgress]() {
            done++;
            if(sinceProgress.elapsed() < 250)
                return;
            sinceProgress.restart();
            QMetaObject::invokeMethod(this, [this, done, total]() {
                if(moveThread) // a late update must not reopen the overlay after the finish
                    mw->showMoveProgress(done, total);
            }, Qt::QueuedConnection);
        };

        for(const auto &path : paths) {
            copyMoveEntry(env, path, destDirectory, overwriteFiles);
            if(overwriteFiles.cancel)
                break;
        }
        const bool cancelled = overwriteFiles.cancel;
        QMetaObject::invokeMethod(this, [this, paths, failures, cancelled, movedHere, selected]() {
            finishBackgroundMove(paths, failures, cancelled, movedHere, selected);
        }, Qt::QueuedConnection);
    });
    connect(moveThread, &QThread::finished, moveThread, &QObject::deleteLater);
    moveThread->start();
}

void FileOperationsController::finishBackgroundMove(const QStringList& paths, int failures, bool cancelled,
                                                    int movedHere, int selected) {
    moveThread = nullptr;
    mw->hideMoveProgress();
    // the worker never touches the model; drop what left the listing (the watcher may already have)
    int moved = 0;
    for(const auto &path : paths) {
        QFileInfo fi(path);
        if(fi.exists() || fi.isSymLink())
            continue;
        moved++;
        if(model)
            model->forgetEntry(path);
    }
    const int movedTotal = movedHere + moved;
    if(cancelled)
        mw->showWarning(tr("Move cancelled - what was not moved yet is still in the source"));
    else if(failures)
        mw->showWarning(tr("Moved %1 of %2 item(s), %3 error(s)").arg(movedTotal).arg(selected).arg(failures));
    else if(movedTotal < selected) // replace prompts answered No
        mw->showWarning(tr("Moved %1 of %2 item(s), skipped files left in place").arg(movedTotal).arg(selected));
    else
        mw->showMessageSuccess(tr("Moved %n item(s)", "", movedTotal));
    emit moveFinished(moved, failures, cancelled);
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
