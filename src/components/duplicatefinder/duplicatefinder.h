#pragma once

#include <QHash>
#include <QObject>
#include <QThreadPool>
#include <atomic>
#include <memory>
#include <thread>
#include "duplicatematch.h"
#include "hashcache.h"

// Duplicate search engine (docs/003 §2). Runs the whole search on a
// coordinator thread with a private QThreadPool for hashing; all signals
// are emitted from worker threads and arrive queued on the GUI thread.
class DuplicateFinder : public QObject {
    Q_OBJECT
public:
    explicit DuplicateFinder(QObject *parent = nullptr);
    ~DuplicateFinder();

    bool isRunning() const;
    // Image files the search would scan; exposed for enumeration reuse.
    static QStringList enumerateFolder(const QString &path, bool recursive,
                                       const std::atomic<bool> *cancelFlag = nullptr);

public slots:
    void start(const DuplicateSearchRequest &request);
    void cancel();

signals:
    void progress(int filesHashed, int filesTotal, int matchesFound);
    void matchFound(const DuplicateMatch &match);
    void finished(bool cancelled);

private:
    struct HashEntry {
        QString canonicalPath;
        QList<quint64> hashes; // variants for source-set files, single otherwise
        QSize dimensions;
        qint64 fileSize = 0;
        bool valid = false;
    };

    void run(DuplicateSearchRequest request);
    void hashFiles(const QStringList &paths, bool sourceSet, const DuplicateSearchRequest &request,
                   QHash<QString, HashEntry> &entries, int totalCount, std::atomic<int> &hashedCount);
    bool comparePair(const HashEntry &source, const HashEntry &target, int maxDistance,
                     QHash<QString, QByteArray> &contentHashes, DuplicateMatch &out) const;
    void joinThread();

    QThreadPool mPool;
    std::thread mThread;
    std::atomic<bool> mCancel{false};
    std::atomic<bool> mRunning{false};
    std::atomic<int> mMatchCount{0};
    std::unique_ptr<HashCache> mCache;
    bool mCacheLoaded = false;
};
