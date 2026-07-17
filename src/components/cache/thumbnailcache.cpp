#include "thumbnailcache.h"

#include <utility>

ThumbnailCache::ThumbnailCache() {
    cacheDirPath = settings->thumbnailCacheDir();
}

QString ThumbnailCache::thumbnailPath(const QString& id) {
    return QString(cacheDirPath + id + ".png");
}

bool ThumbnailCache::exists(QString id) {
    QString filePath = thumbnailPath(std::move(id));
    QFileInfo file(filePath);
    return file.exists() && file.isReadable();
}

void ThumbnailCache::saveThumbnail(QImage *image, QString id) {
    if(image) {
        QString filePath = thumbnailPath(std::move(id));
        image->save(filePath, "PNG", 15);
    }
}

std::unique_ptr<QImage> ThumbnailCache::readThumbnail(QString id) {
    QString filePath = thumbnailPath(std::move(id));
    QFileInfo file(filePath);
    if(file.exists() && file.isReadable()) {
        auto thumb = std::make_unique<QImage>();
        if(thumb->load(filePath))
            return thumb;
    }
    return nullptr;
}
