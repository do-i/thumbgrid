#include "video.h"

#include <utility>
#include "utils/logging.h"

Video::Video(QString _path) : Image(std::move(_path)) {
    Video::load();
}

Video::Video(std::unique_ptr<DocumentInfo> _info) : Image(std::move(_info)) {
    Video::load();
}

void Video::load() {
    if(isLoaded())
        return;

    mLoaded = true;
}

bool Video::save(QString destPath) {
    Q_UNUSED(destPath)
    qCWarning(logVideo) << "Saving video is unsupported.";
    return false;
}

bool Video::save() {
    qCWarning(logVideo) << "Saving video is unsupported.";
    return false;
}

std::unique_ptr<QPixmap> Video::getPixmap() {
    qCWarning(logVideo) << "[Video] getPixmap() is not implemented.";
    //TODO: find out some easy way to get frames from video source
    return nullptr;
}

std::shared_ptr<const QImage> Video::getImage() {
    qCWarning(logVideo) << "[Video] getImage() is not implemented.";
    //TODO: find out some easy way to get frames from video source
    return nullptr;
}

int Video::height() {
    return srcHeight;
}

int Video::width() {
    return srcWidth;
}

QSize Video::size() {
    return QSize(srcWidth, srcHeight);
}
