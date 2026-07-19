#include "duplicatefinder.h"

#include <QDateTime>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QMutex>
#include <QRunnable>
#include <QSet>
#include "imagehasher.h"
#include "settings.h"

namespace {

constexpr int DECODE_SIZE = 256;

QStringList imageNameFilters() {
    QStringList filters;
    for(const QByteArray &fmt : QImageReader::supportedImageFormats()) {
        if(fmt == "pdf")
            continue;
        filters << "*." + QString::fromLatin1(fmt);
    }
    filters << "*.jfif";
    return filters;
}

QStringList dedupePaths(const QStringList &paths) {
    QStringList result;
    QSet<QString> seen;
    for(const QString &path : paths) {
        if(!seen.contains(path)) {
            seen.insert(path);
            result << path;
        }
    }
    return result;
}

} // namespace

DuplicateFinder::DuplicateFinder(QObject *parent) : QObject(parent) {
    qRegisterMetaType<DuplicateMatch>();
    mCache = std::make_unique<HashCache>(settings->tmpDir() + "duplicatehashes.dat");
}

DuplicateFinder::~DuplicateFinder() {
    cancel();
    joinThread();
}

bool DuplicateFinder::isRunning() const {
    return mRunning;
}

QStringList DuplicateFinder::enumerateFolder(const QString &path, bool recursive,
                                             const std::atomic<bool> *cancelFlag) {
    QStringList result;
    QDirIterator it(path, imageNameFilters(), QDir::Files,
                    recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    while(it.hasNext()) {
        if(cancelFlag && *cancelFlag)
            break;
        result << it.next();
    }
    return result;
}

void DuplicateFinder::start(const DuplicateSearchRequest &request) {
    if(mRunning)
        return;
    joinThread();
    mCancel = false;
    mRunning = true;
    mMatchCount = 0;
    mPool.setMaxThreadCount(settings->duplicateSearchThreadCount());
    mThread = std::thread([this, request] { run(request); });
}

void DuplicateFinder::cancel() {
    mCancel = true;
    mPool.clear();
}

void DuplicateFinder::joinThread() {
    if(mThread.joinable())
        mThread.join();
}

void DuplicateFinder::run(DuplicateSearchRequest request) {
    if(!mCacheLoaded) {
        mCache->load();
        mCacheLoaded = true;
    }
    QStringList sources, targets;
    bool within = (request.mode == DuplicateSearchRequest::WITHIN_FOLDERS);
    switch(request.mode) {
    case DuplicateSearchRequest::SINGLE_IMAGE:
        sources << request.sourceImage;
        break;
    case DuplicateSearchRequest::COMPARE_FOLDERS:
        for(const QString &dir : request.sourceFolders)
            sources << enumerateFolder(dir, request.recursive, &mCancel);
        break;
    case DuplicateSearchRequest::WITHIN_FOLDERS:
        break;
    }
    for(const QString &dir : request.targetFolders)
        targets << enumerateFolder(dir, request.recursive, &mCancel);
    sources = dedupePaths(sources);
    targets = dedupePaths(targets);
    if(within)
        sources = targets;

    QStringList toHash = sources;
    for(const QString &path : targets)
        if(!sources.contains(path))
            toHash << path;
    int total = toHash.count();

    if(mCancel) {
        mRunning = false;
        emit finished(true);
        return;
    }
    emit progress(0, total, 0);

    QHash<QString, HashEntry> entries;
    entries.reserve(total);
    std::atomic<int> hashed{0};
    hashFiles(sources, true, request, entries, total, hashed);
    hashFiles(targets, false, request, entries, total, hashed);
    mCache->save();

    if(!mCancel) {
        int maxDistance = ImageHasher::maxDistanceForSimilarity(request.similarityPercent);
        QHash<QString, QByteArray> contentHashes;
        DuplicateMatch match;
        auto reportMatch = [&](const QString &src, const QString &tgt) {
            if(!comparePair(entries[src], entries[tgt], maxDistance, request.matchRotated,
                            contentHashes, match))
                return;
            match.sourcePath = src;
            match.matchPath = tgt;
            match.sourceDimensions = entries[src].dimensions;
            match.matchDimensions = entries[tgt].dimensions;
            match.sourceFileSize = entries[src].fileSize;
            match.matchFileSize = entries[tgt].fileSize;
            ++mMatchCount;
            emit matchFound(match);
        };
        if(within) {
            for(int i = 0; i < sources.count() && !mCancel; i++)
                for(int j = i + 1; j < sources.count(); j++)
                    reportMatch(sources[i], sources[j]);
        } else {
            for(const QString &src : sources) {
                if(mCancel)
                    break;
                for(const QString &tgt : targets)
                    reportMatch(src, tgt);
            }
        }
    }
    emit progress(hashed, total, mMatchCount);
    bool cancelled = mCancel;
    mRunning = false;
    emit finished(cancelled);
}

void DuplicateFinder::hashFiles(const QStringList &paths, bool sourceSet,
                                const DuplicateSearchRequest &request,
                                QHash<QString, HashEntry> &entries, int totalCount,
                                std::atomic<int> &hashedCount) {
    QMutex mutex;
    bool variants = sourceSet && (request.matchRotated || request.matchMirrored);
    for(const QString &path : paths) {
        {
            QMutexLocker lock(&mutex);
            if(entries.contains(path))
                continue;
            entries.insert(path, HashEntry()); // reserve so the target pass skips it
        }
        mPool.start(QRunnable::create([this, path, variants, &request, &entries, &mutex,
                                       totalCount, &hashedCount] {
            HashEntry entry;
            if(!mCancel) {
                QFileInfo info(path);
                qint64 mtime = info.lastModified().toSecsSinceEpoch();
                quint64 cachedHash = 0;
                QSize cachedDims;
                // variant hashing needs the decoded image, so only the
                // plain-hash path (all targets, most sources) hits the cache
                if(!variants && mCache->lookup(path, mtime, info.size(), cachedHash, cachedDims)) {
                    entry.hashes = QList<quint64>{cachedHash};
                    entry.dimensions = cachedDims;
                    entry.fileSize = info.size();
                    entry.canonicalPath = info.canonicalFilePath();
                    entry.valid = true;
                } else {
                    QImageReader reader(path);
                    reader.setAutoTransform(true);
                    QSize size = reader.size();
                    if(size.isValid() && (size.width() > DECODE_SIZE || size.height() > DECODE_SIZE))
                        reader.setScaledSize(QSize(DECODE_SIZE, DECODE_SIZE));
                    QImage image = reader.read();
                    if(!image.isNull()) {
                        if(!size.isValid())
                            size = image.size();
                        if(variants)
                            entry.hashes = ImageHasher::phashVariants(image, request.matchRotated,
                                                                      request.matchMirrored);
                        else
                            entry.hashes = QList<quint64>{ImageHasher::phash(image)};
                        entry.dimensions = size;
                        entry.fileSize = info.size();
                        entry.canonicalPath = info.canonicalFilePath();
                        entry.valid = !entry.hashes.isEmpty();
                        if(!variants && entry.valid)
                            mCache->insert(path, mtime, info.size(), entry.hashes.first(), size);
                    }
                }
            }
            {
                QMutexLocker lock(&mutex);
                entries[path] = entry;
            }
            int done = ++hashedCount;
            if(done % 8 == 0 || done == totalCount)
                emit progress(done, totalCount, mMatchCount);
        }));
    }
    mPool.waitForDone();
}

bool DuplicateFinder::comparePair(const HashEntry &source, const HashEntry &target, int maxDistance,
                                  bool allowRotated, QHash<QString, QByteArray> &contentHashes,
                                  DuplicateMatch &out) const {
    if(!source.valid || !target.valid)
        return false;
    if(source.canonicalPath == target.canonicalPath)
        return false;
    // Aspect gate: pHash squashes everything to a square, so images with very
    // different shapes can collide at loose thresholds (seen in user testing:
    // a 2.1:1 graphic "matching" a 2.9:1 sprite sheet at 65%). Require the
    // shapes to be within 35% of each other; with rotation matching the
    // 90-degree-swapped shape also qualifies.
    if(!source.dimensions.isEmpty() && !target.dimensions.isEmpty()) {
        qreal sourceRatio = qreal(source.dimensions.width()) / source.dimensions.height();
        qreal targetRatio = qreal(target.dimensions.width()) / target.dimensions.height();
        auto compatible = [](qreal a, qreal b) {
            qreal q = a / b;
            if(q < 1)
                q = 1 / q;
            return q <= 1.35;
        };
        bool aspectOk = compatible(sourceRatio, targetRatio);
        if(!aspectOk && allowRotated)
            aspectOk = compatible(sourceRatio, 1.0 / targetRatio);
        if(!aspectOk)
            return false;
    }
    int best = ImageHasher::HASH_BITS + 1;
    quint64 targetHash = target.hashes.first();
    for(quint64 sourceHash : source.hashes)
        best = qMin(best, ImageHasher::hammingDistance(sourceHash, targetHash));
    if(best > maxDistance)
        return false;
    out = DuplicateMatch();
    out.similarity = (ImageHasher::HASH_BITS - best) * 100.0 / ImageHasher::HASH_BITS;
    if(source.fileSize > 0 && source.fileSize == target.fileSize) {
        for(const auto &entry : {source, target})
            if(!contentHashes.contains(entry.canonicalPath))
                contentHashes.insert(entry.canonicalPath, ImageHasher::contentHash(entry.canonicalPath));
        if(!contentHashes[source.canonicalPath].isEmpty()
           && contentHashes[source.canonicalPath] == contentHashes[target.canonicalPath]) {
            out.exact = true;
            out.similarity = 100.0;
        }
    }
    return true;
}
