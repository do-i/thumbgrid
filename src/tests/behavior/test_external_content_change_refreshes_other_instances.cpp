#include "support/thumbgrid_test_support.h"
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <filesystem>

#include "components/directorymanager/directorymanager.h"

// An external change reaches sibling instances via updateFileEntry(), which re-announces only an entry it believes
// changed. size is compared as well as mtime (timestamp-preserving writes keep mtime), while a rename, which
// changes neither, must not re-announce.

namespace {

bool writeImage(const QString &path, int w, int h, QColor color) {
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(color);
    return img.save(path, "PNG");
}

std::filesystem::file_time_type mtimeOf(const QString &path) {
    return std::filesystem::last_write_time(path.toStdString());
}

// Restores an exact mtime. std::filesystem is used rather than utime() so the
// value round-trips at full precision - a seconds-granularity restore would
// leave mtime *different* and the test would pass for the wrong reason.
bool restoreMtime(const QString &path, std::filesystem::file_time_type when) {
    std::error_code ec;
    std::filesystem::last_write_time(path.toStdString(), when, ec);
    return !ec && mtimeOf(path) == when;
}

} // namespace

class ExternalContentChangeRefreshesTest : public QObject {
    Q_OBJECT
private slots:
    void aContentChangeThatPreservesTheTimestampIsStillAnnounced();
    void aReStatWithNothingChangedIsNotAnnounced();
    void anOrdinaryContentChangeIsStillAnnounced();
};

// The cross-instance case: same mtime, different size.
void ExternalContentChangeRefreshesTest::aContentChangeThatPreservesTheTimestampIsStillAnnounced() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = tmp.path() + QStringLiteral("/shared.png");
    QVERIFY(writeImage(file, 64, 64, Qt::red));

    DirectoryManager manager;
    QVERIFY(manager.setDirectory(tmp.path()));
    const int index = manager.indexOfFile(file);
    QVERIFY(index >= 0);
    const std::uintmax_t sizeBefore = manager.fileEntryAt(index).size;

    const auto originalMtime = mtimeOf(file);
    // A different geometry gives a different encoded size, which is what the
    // size comparison has to notice.
    QVERIFY(writeImage(file, 256, 192, Qt::blue));
    if(!restoreMtime(file, originalMtime))
        QSKIP("filesystem refused an exact mtime restore, cannot simulate a timestamp-preserving write");

    // Guard against a vacuous pass: the whole point is that mtime is identical
    // and only size gives the change away.
    QCOMPARE(mtimeOf(file), originalMtime);
    QVERIFY2(static_cast<std::uintmax_t>(QFileInfo(file).size()) != sizeBefore,
             "the rewrite must change the file size for this case to be meaningful");

    QSignalSpy spy(&manager, &DirectoryManager::fileModified);
    manager.updateFileEntry(file);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), file);
    QCOMPARE(manager.fileEntryAt(manager.indexOfFile(file)).size,
             static_cast<std::uintmax_t>(QFileInfo(file).size()));
}

// The reason the early return exists: a speculative re-stat of an untouched
// file must stay silent, or every eager rename triggers a reload.
void ExternalContentChangeRefreshesTest::aReStatWithNothingChangedIsNotAnnounced() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = tmp.path() + QStringLiteral("/quiet.png");
    QVERIFY(writeImage(file, 48, 48, Qt::green));

    DirectoryManager manager;
    QVERIFY(manager.setDirectory(tmp.path()));
    QVERIFY(manager.indexOfFile(file) >= 0);

    QSignalSpy spy(&manager, &DirectoryManager::fileModified);
    manager.updateFileEntry(file);
    manager.updateFileEntry(file);

    QCOMPARE(spy.count(), 0);
}

void ExternalContentChangeRefreshesTest::anOrdinaryContentChangeIsStillAnnounced() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = tmp.path() + QStringLiteral("/plain.png");
    QVERIFY(writeImage(file, 32, 32, Qt::yellow));

    DirectoryManager manager;
    QVERIFY(manager.setDirectory(tmp.path()));
    QVERIFY(manager.indexOfFile(file) >= 0);

    QSignalSpy spy(&manager, &DirectoryManager::fileModified);
    // No mtime tampering here: an ordinary write bumps the timestamp, the case
    // that already worked. Kept so a future change cannot fix the case above by
    // breaking this one.
    QVERIFY(writeImage(file, 33, 33, Qt::magenta));
    manager.updateFileEntry(file);

    QCOMPARE(spy.count(), 1);
}

TG_BEHAVIOR_TEST_MAIN(ExternalContentChangeRefreshesTest)

#include "test_external_content_change_refreshes_other_instances.moc"
