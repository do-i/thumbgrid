#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QElapsedTimer>
#include <QTemporaryDir>

#include "components/cache/thumbnailcache.h"
#include "components/thumbnailer/thumbnailerrunnable.h"

// Verifies that generated thumbnails are served from the disk cache on a
// second request: a repeated directory-composite or file-thumbnail request
// must not rescan the folder or re-decode the image.
class DirThumbnailsAreCachedOnDiskTest : public QObject {
    Q_OBJECT
private slots:
    void secondGenerateDirHitsCache();
    void secondFileGenerateHitsCache();
};

static QStringList cacheFiles() {
    QDir d(settings->thumbnailCacheDir());
    return d.entryList(QDir::Files, QDir::Name);
}

static QImage folderIconBase(int size) {
    QImage src(":/res/icons/common/other/folder.png");
    if(src.isNull())
        src = QImage(":/res/icons/common/other/folder96.png");
    return src.scaled(qRound(size * 1.10),
                      qRound(size * 1.10 * src.height() / (qreal)src.width()),
                      Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void DirThumbnailsAreCachedOnDiskTest::secondGenerateDirHitsCache() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());
    QVERIFY(root.mkpath("sub"));
    for(int i = 0; i < 30; i++)
        QVERIFY(tgtest::writeImage(root.filePath(QString("sub/img%1.png").arg(i, 3, 10, QChar('0'))), Qt::red));

    ThumbnailCache cache;
    const QString subPath = root.filePath("sub");
    const int size = 200;
    const QImage iconBase = folderIconBase(size);
    const QString colorId = "#808080";

    QElapsedTimer t;
    t.start();
    auto first = ThumbnailerRunnable::generateDir(&cache, subPath, size, false, false, false, false, iconBase, colorId);
    qint64 firstMs = t.elapsed();
    QVERIFY(first != nullptr);

    QStringList filesAfterFirst = cacheFiles();
    qInfo() << "cache files after first call:" << filesAfterFirst;
    QCOMPARE(filesAfterFirst.count(), 1);
    QFileInfo cacheFile(settings->thumbnailCacheDir() + filesAfterFirst.first());
    QDateTime writeTimeFirst = cacheFile.lastModified();

    QTest::qWait(1100); // ensure a rewrite would be visible in mtime

    t.restart();
    auto second = ThumbnailerRunnable::generateDir(&cache, subPath, size, false, false, false, false, iconBase, colorId);
    qint64 secondMs = t.elapsed();
    QVERIFY(second != nullptr);

    cacheFile.refresh();
    qInfo() << "first(ms):" << firstMs << "second(ms):" << secondMs
            << "rewritten:" << (cacheFile.lastModified() != writeTimeFirst);
    QCOMPARE(cacheFile.lastModified(), writeTimeFirst); // fails if regenerated
}

void DirThumbnailsAreCachedOnDiskTest::secondFileGenerateHitsCache() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());

    // image larger than the thumbnail size, so it qualifies for disk caching
    QImage big(800, 600, QImage::Format_RGB32);
    big.fill(Qt::blue);
    const QString imgPath = root.filePath("big.png");
    QVERIFY(big.save(imgPath, "PNG"));

    ThumbnailCache cache;
    QStringList before = cacheFiles();

    auto first = ThumbnailerRunnable::generate(&cache, imgPath, 200, false, false);
    QVERIFY(first != nullptr);
    QStringList afterFirst = cacheFiles();
    QCOMPARE(afterFirst.count(), before.count() + 1);

    QString newFile;
    for(const QString &f : afterFirst)
        if(!before.contains(f))
            newFile = f;
    QFileInfo cacheFile(settings->thumbnailCacheDir() + newFile);
    QDateTime writeTimeFirst = cacheFile.lastModified();

    QTest::qWait(1100);

    auto second = ThumbnailerRunnable::generate(&cache, imgPath, 200, false, false);
    QVERIFY(second != nullptr);
    cacheFile.refresh();
    qInfo() << "file thumb rewritten:" << (cacheFile.lastModified() != writeTimeFirst);
    QCOMPARE(cacheFile.lastModified(), writeTimeFirst);
}

TG_BEHAVIOR_TEST_MAIN(DirThumbnailsAreCachedOnDiskTest)

#include "test_dir_thumbnails_are_cached_on_disk.moc"
