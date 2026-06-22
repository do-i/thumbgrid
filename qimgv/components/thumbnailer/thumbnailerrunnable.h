#pragma once

#include <QRunnable>
#include <QProcess>
#include <QThread>
#include <QCryptographicHash>
#include <ctime>
#include "sourcecontainers/thumbnail.h"
#include "components/cache/thumbnailcache.h"
#include "utils/imagefactory.h"
#include "utils/imagelib.h"
#include "settings.h"
#include <memory>
#include <QImageWriter>

class ThumbnailerRunnable : public QObject, public QRunnable {
    Q_OBJECT
public:
    ThumbnailerRunnable(ThumbnailCache* _cache, QString _path, int _size, bool _crop, bool _force);
    // directory thumbnail variant
    ThumbnailerRunnable(ThumbnailCache* _cache, QString _path, int _size, bool _crop, bool _force,
                        bool _previewFit, bool _showHidden, QImage _iconBase, QString _colorId);
    ~ThumbnailerRunnable();
    void run();
    static std::shared_ptr<Thumbnail> generate(ThumbnailCache *cache, QString path, int size, bool crop, bool force);
    static std::shared_ptr<Thumbnail> generateDir(ThumbnailCache *cache, QString path, int size, bool crop, bool force,
                                                  bool previewFit, bool showHidden, const QImage &iconBase, const QString &colorId);
private:
    static QString generateIdString(QString path, int size, bool crop);
    static QString generateDirIdString(QString path, int size, bool previewFit, const QString &colorId);
    static std::pair<QImage*, QSize> createThumbnail(QString path, const char* format, int size, bool crop);
    static std::pair<QImage*, QSize> createVideoThumbnail(QString path, int size, bool crop);
    static QList<QImage> dirPreviewImages(ThumbnailCache *cache, const QString &path, int targetSize, bool crop, bool showHidden);
    static void drawDirPreview(QImage &base, const QList<QImage> &images, bool previewFit);
    QString path;
    int size;
    bool crop, force;
    ThumbnailCache* cache = nullptr;
    bool isDir = false;
    bool previewFit = false;
    bool showHidden = false;
    QImage iconBase;
    QString colorId;

signals:
    void taskStart(QString, int);
    void taskEnd(std::shared_ptr<Thumbnail>, QString);
    void dirTaskEnd(std::shared_ptr<Thumbnail>, QString);
};
