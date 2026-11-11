#include "support/thumbgrid_test_support.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <atomic>
#include <memory>
#include <thread>

#include "components/directorymanager/directorymanager.h"
#include "components/directorymanager/watchers/directorywatcher.h"

// The file watcher thread is joined whenever the watcher stops or is destroyed. A thread left running either
// emits into a watcher that is gone (crash on teardown) or keeps the "running" flag from a previous start, so a
// stopped watcher is never restarted and later external changes are silently missed.

namespace {

bool touch(const QString &path) {
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly))
        return false;
    f.write("x");
    return true;
}

} // namespace

class DirectoryWatcherStopsAndRestartsTest : public QObject {
    Q_OBJECT
private slots:
    void destroyingTheOwnerWhileFilesChangeNeverCrashes();
    void aWatcherRestartedAfterStopStillSeesNewFiles();
    void stoppingAndObservingTwiceIsHarmless();
};

void DirectoryWatcherStopsAndRestartsTest::destroyingTheOwnerWhileFilesChangeNeverCrashes() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    std::atomic<bool> stop{false};
    std::thread churn([&] {
        int n = 0;
        while(!stop.load()) {
            const QString p = tmp.path() + QStringLiteral("/churn%1.txt").arg(n++ % 8);
            touch(p);
            QFile::remove(p);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    for(int i = 0; i < 40; i++) {
        auto manager = std::make_unique<DirectoryManager>();
        QVERIFY(manager->setDirectory(tmp.path()));
        QVERIFY(manager->fileWatcherActive());
        QTest::qWait(i % 5);
        QCoreApplication::processEvents();
        manager.reset();
        QCoreApplication::processEvents();
    }

    stop = true;
    churn.join();
    QVERIFY(true); // reaching here without abort or hang is the assertion
}

void DirectoryWatcherStopsAndRestartsTest::aWatcherRestartedAfterStopStillSeesNewFiles() {
    QTemporaryDir a, b;
    QVERIFY(a.isValid() && b.isValid());

    DirectoryManager manager;
    QVERIFY(manager.setDirectory(a.path()));
    QVERIFY(manager.fileWatcherActive());

    QVERIFY(manager.setDirectoryRecursive(b.path()));
    QVERIFY(!manager.fileWatcherActive());

    QVERIFY(manager.setDirectory(a.path()));
    QVERIFY(manager.fileWatcherActive());

    QSignalSpy added(&manager, &DirectoryManager::fileAdded);
    const QString file = a.path() + QStringLiteral("/after_restart.png");
    QVERIFY(touch(file));
    QTRY_VERIFY_WITH_TIMEOUT(added.count() >= 1, 5000);
    QVERIFY(manager.indexOfFile(file) >= 0);
}

void DirectoryWatcherStopsAndRestartsTest::stoppingAndObservingTwiceIsHarmless() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    std::unique_ptr<DirectoryWatcher> watcher(DirectoryWatcher::newInstance());
    QVERIFY(watcher);
    watcher->setWatchPath(tmp.path());

    watcher->stopObserving(); // before ever observing
    QVERIFY(!watcher->isObserving());

    watcher->observe();
    watcher->observe();
    QVERIFY(watcher->isObserving());

    watcher->stopObserving();
    watcher->stopObserving();
    QVERIFY(!watcher->isObserving());

    watcher->observe();
    QVERIFY(watcher->isObserving());

    QSignalSpy created(watcher.get(), &DirectoryWatcher::fileCreated);
    QVERIFY(touch(tmp.path() + QStringLiteral("/second_run.txt")));
    QTRY_VERIFY_WITH_TIMEOUT(created.count() >= 1, 5000);

    watcher->stopObserving();
    QVERIFY(!watcher->isObserving());
    // destroyed while stopped, and a second watcher destroyed while observing
    watcher.reset();
    watcher.reset(DirectoryWatcher::newInstance());
    watcher->setWatchPath(tmp.path());
    watcher->observe();
    watcher.reset();
}

TG_BEHAVIOR_TEST_MAIN(DirectoryWatcherStopsAndRestartsTest)

#include "test_directory_watcher_stops_and_restarts.moc"
