#include "cmdoptionsrunner.h"
#include "utils/logging.h"

void CmdOptionsRunner::generateThumbs(const QString& dirPath, int size) {
    if(size <= 50 || size > 400) {
        qCWarning(logCore) << "Error: Invalid thumbnail size.";
        qCWarning(logCore) << "Please specify a value between [50, 400].";
        qCWarning(logCore) << "Example:  thumbgrid --gen-thumbs=/home/user/Pictures/ --gen-thumbs-size=120";
        QCoreApplication::exit(1);
        return;
    }

    Thumbnailer th;
    DirectoryManager dm;
    if(!dm.setDirectoryRecursive(dirPath)) {
        qCWarning(logCore) << "Error: Invalid path.";
        QCoreApplication::exit(1);
        return;
    }

    auto list = dm.fileList();

    qCDebug(logCore) << "\nDirectory:" << dirPath;
    qCDebug(logCore) << "File count:" << list.size();
    qCDebug(logCore) << "Size limit:" << size << "x" << size << "px";
    qCDebug(logCore) << "Generating thumbnails...";

    for(const auto& path : list)
        th.getThumbnailAsync(path, size, false, false);

    th.waitForDone();
    qCDebug(logCore) << "\nDone.";
    QCoreApplication::quit();
}

void CmdOptionsRunner::showBuildOptions() {
    QStringList features;
#ifdef USE_MPV
    features << "USE_MPV";
#endif
#ifdef USE_EXIV2
    features << "USE_EXIV2";
#endif
#ifdef USE_KDE_BLUR
    features << "USE_KDE_BLUR";
#endif
#ifdef USE_OPENCV
    features << "USE_OPENCV";
#endif
    qCDebug(logCore) << "\nEnabled build options:";
    if(!features.count())
        qCDebug(logCore) << "   --";
    for(int i = 0; i < features.count(); i++)
        qCDebug(logCore) << "   " << features.at(i);
    QCoreApplication::quit();
}
