#include "support/thumbgrid_test_support.h"
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include "components/duplicatefinder/duplicatefinder.h"
#include "settings.h"

static QImage makeScene(int id, int w = 512, int h = 384) {
    QImage img(w, h, QImage::Format_RGB32);
    QPainter p(&img);
    if(id == 0) {
        QLinearGradient g(0, 0, w, h);
        g.setColorAt(0, QColor(230, 225, 210));
        g.setColorAt(1, QColor(40, 60, 90));
        p.fillRect(img.rect(), g);
        p.setBrush(QColor(25, 20, 30));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(w / 4, h / 4), w / 6, w / 6);
        p.setBrush(QColor(240, 200, 60));
        p.drawRect(w / 2, h / 2, w / 3, h / 5);
    } else {
        for(int y = 0; y < 3; y++)
            for(int x = 0; x < 4; x++)
                p.fillRect(x * w / 4, y * h / 3, w / 4, h / 3,
                           ((x + y) % 2) ? QColor(235, 235, 235) : QColor(30, 30, 30));
    }
    p.end();
    return img;
}

class DuplicateFinderEngineTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir tmp;
    QString srcDir, tgtDir;
    QList<DuplicateMatch> runSearch(const DuplicateSearchRequest &request, bool *cancelled = nullptr);
private slots:
    void initTestCase();
    void singleImageSearchFindsCopiesRecursively();
    void nonRecursiveSearchSkipsSubfolders();
    void sourceImageInsideScopeDoesNotMatchItself();
    void compareFoldersFindsSourceCopiesInTargets();
    void withinFoldersFindsInternalDuplicates();
    void threadSettingsShareCoreBudget();
};

QList<DuplicateMatch> DuplicateFinderEngineTest::runSearch(const DuplicateSearchRequest &request,
                                                           bool *cancelled) {
    DuplicateFinder finder;
    QSignalSpy matchSpy(&finder, &DuplicateFinder::matchFound);
    QSignalSpy finishedSpy(&finder, &DuplicateFinder::finished);
    finder.start(request);
    [&] { QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() > 0, 15000); }();
    if(cancelled)
        *cancelled = finishedSpy.first().first().toBool();
    QList<DuplicateMatch> matches;
    for(const auto &args : matchSpy)
        matches << args.first().value<DuplicateMatch>();
    return matches;
}

void DuplicateFinderEngineTest::initTestCase() {
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QVERIFY(root.mkpath("src"));
    QVERIFY(root.mkpath("tgt/sub"));
    srcDir = tmp.filePath("src");
    tgtDir = tmp.filePath("tgt");
    QImage sceneA = makeScene(0);
    QVERIFY(sceneA.save(tmp.filePath("src/A.png")));
    QVERIFY(QFile::copy(tmp.filePath("src/A.png"), tmp.filePath("tgt/exact.png")));
    QVERIFY(sceneA.save(tmp.filePath("tgt/reenc.jpg"), "JPG", 80));
    QVERIFY(makeScene(1).save(tmp.filePath("tgt/other.png")));
    QVERIFY(sceneA.save(tmp.filePath("tgt/sub/nested.jpg"), "JPG", 85));
}

void DuplicateFinderEngineTest::singleImageSearchFindsCopiesRecursively() {
    DuplicateSearchRequest req;
    req.mode = DuplicateSearchRequest::SINGLE_IMAGE;
    req.sourceImage = tmp.filePath("src/A.png");
    req.targetFolders = QStringList{tgtDir};
    bool cancelled = true;
    auto matches = runSearch(req, &cancelled);
    QVERIFY(!cancelled);
    QCOMPARE(matches.count(), 3);
    QStringList found;
    for(const auto &m : matches) {
        QCOMPARE(m.sourcePath, req.sourceImage);
        found << QFileInfo(m.matchPath).fileName();
        if(m.matchPath.endsWith("exact.png")) {
            QVERIFY(m.exact);
            QCOMPARE(m.similarity, 100.0);
        } else {
            QVERIFY(m.similarity >= 87.0);
        }
        QCOMPARE(m.matchDimensions, QSize(512, 384));
        QVERIFY(m.matchFileSize > 0);
    }
    found.sort();
    QCOMPARE(found, QStringList({"exact.png", "nested.jpg", "reenc.jpg"}));
}

void DuplicateFinderEngineTest::nonRecursiveSearchSkipsSubfolders() {
    DuplicateSearchRequest req;
    req.mode = DuplicateSearchRequest::SINGLE_IMAGE;
    req.sourceImage = tmp.filePath("src/A.png");
    req.targetFolders = QStringList{tgtDir};
    req.recursive = false;
    auto matches = runSearch(req);
    QCOMPARE(matches.count(), 2);
    for(const auto &m : matches)
        QVERIFY(!m.matchPath.contains("/sub/"));
}

void DuplicateFinderEngineTest::sourceImageInsideScopeDoesNotMatchItself() {
    DuplicateSearchRequest req;
    req.mode = DuplicateSearchRequest::SINGLE_IMAGE;
    req.sourceImage = tmp.filePath("src/A.png");
    req.targetFolders = QStringList{srcDir};
    auto matches = runSearch(req);
    QCOMPARE(matches.count(), 0);
}

void DuplicateFinderEngineTest::compareFoldersFindsSourceCopiesInTargets() {
    DuplicateSearchRequest req;
    req.mode = DuplicateSearchRequest::COMPARE_FOLDERS;
    req.sourceFolders = QStringList{srcDir};
    req.targetFolders = QStringList{tgtDir};
    auto matches = runSearch(req);
    QCOMPARE(matches.count(), 3);
}

void DuplicateFinderEngineTest::withinFoldersFindsInternalDuplicates() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QImage sceneA = makeScene(0);
    QVERIFY(sceneA.save(dir.filePath("one.png")));
    QVERIFY(QFile::copy(dir.filePath("one.png"), dir.filePath("two.png")));
    QVERIFY(makeScene(1).save(dir.filePath("other.png")));
    DuplicateSearchRequest req;
    req.mode = DuplicateSearchRequest::WITHIN_FOLDERS;
    req.targetFolders = QStringList{dir.path()};
    auto matches = runSearch(req);
    QCOMPARE(matches.count(), 1);
    QVERIFY(matches.first().exact);
    QVERIFY(matches.first().sourcePath != matches.first().matchPath);
}

void DuplicateFinderEngineTest::threadSettingsShareCoreBudget() {
    int cores = qMax(1, QThread::idealThreadCount());
    if(cores < 2)
        QSKIP("needs a multi-core machine");
    // each setter clamps to cores minus the other's current allocation
    int finderBefore = settings->duplicateSearchThreadCount();
    settings->setThumbnailerThreadCount(cores * 2);
    QCOMPARE(settings->thumbnailerThreadCount(), qMax(1, cores - finderBefore));
    settings->setDuplicateSearchThreadCount(cores * 2);
    QCOMPARE(settings->duplicateSearchThreadCount(), qMax(1, cores - settings->thumbnailerThreadCount()));
    // rebalance: shrink thumbnailer, finder can then grow
    settings->setThumbnailerThreadCount(1);
    settings->setDuplicateSearchThreadCount(cores - 1);
    QCOMPARE(settings->duplicateSearchThreadCount(), cores - 1);
    QVERIFY(settings->thumbnailerThreadCount() + settings->duplicateSearchThreadCount() <= cores);
    // back to defaults for any later tests
    settings->setDuplicateSearchThreadCount(2);
    settings->setThumbnailerThreadCount(2);
}

TG_BEHAVIOR_TEST_MAIN(DuplicateFinderEngineTest)

#include "test_duplicate_finder_engine.moc"
