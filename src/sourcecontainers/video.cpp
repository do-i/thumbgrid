#include "video.h"

Video::Video(QString _path) : Image(_path) {
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
    qDebug() << "Saving video is unsupported.";
    return false;
}

bool Video::save() {
    qDebug() << "Saving video is unsupported.";
    return false;
}

std::unique_ptr<QPixmap> Video::getPixmap() {
    qDebug() << "[Video] getPixmap() is not implemented.";
    //TODO: find out some easy way to get frames from video source
    return nullptr;
}

std::shared_ptr<const QImage> Video::getImage() {
    qDebug() << "[Video] getImage() is not implemented.";
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
