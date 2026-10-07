// Unit tests for ResizeCopy, the GUI-free core of "Resize..." in the grid:
// how one spec maps onto images of different sizes, how copies are named, and
// that a resize writes a new file and never the original.
#include "support/thumbgrid_test_support.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QTemporaryDir>

#include "utils/resizecopy.h"

class ResizeCopyRulesTest : public QObject {
    Q_OBJECT

private slots:
    void percentAppliesToEachImageOwnSize();
    void fitWithinKeepsEachImageAspect();
    void fitWithinSnapsToTheBoxTheDialogShowed();
    void exactIgnoresAspect();
    void longEdgeSetsTheLongerSideOfEachImage();
    void shrinkOnlySkipsAnythingNotSmaller();
    void unchangedSizeIsAlwaysSkipped();
    void copyPathAddsSizeSuffixAndAvoidsCollisions();
    void resizeFileWritesACopyAndLeavesTheOriginal();
    void resizeFileSkipsWithoutWriting();
};

namespace {

ResizeSpec percent(double value, bool shrinkOnly = true) {
    ResizeSpec spec;
    spec.mode = ResizeSpec::Percent;
    spec.percent = value;
    spec.shrinkOnly = shrinkOnly;
    return spec;
}

ResizeSpec boxed(ResizeSpec::Mode mode, QSize size) {
    ResizeSpec spec;
    spec.mode = mode;
    spec.size = size;
    return spec;
}

QByteArray fileHash(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return {};
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

bool writeSizedImage(const QString &path, QSize size) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(QColor(90, 140, 200));
    return image.save(path);
}

} // namespace

void ResizeCopyRulesTest::percentAppliesToEachImageOwnSize() {
    QCOMPARE(ResizeCopy::targetSize(QSize(400, 200), percent(50)), QSize(200, 100));
    QCOMPARE(ResizeCopy::targetSize(QSize(100, 300), percent(50)), QSize(50, 150));
    // truncates like ResizeDialog, and never collapses a side to zero
    QCOMPARE(ResizeCopy::targetSize(QSize(333, 3), percent(10)), QSize(33, 1));
}

void ResizeCopyRulesTest::fitWithinKeepsEachImageAspect() {
    auto spec = boxed(ResizeSpec::FitWithin, QSize(200, 200));
    QCOMPARE(ResizeCopy::targetSize(QSize(400, 200), spec), QSize(200, 100));
    QCOMPARE(ResizeCopy::targetSize(QSize(300, 600), spec), QSize(100, 200));
}

void ResizeCopyRulesTest::fitWithinSnapsToTheBoxTheDialogShowed() {
    // Typing width 500 for a 1000x667 image makes the dialog show 500x333
    // (truncated). Fitting 1000x667 into 500x333 gives 499x333; the user
    // asked for 500 wide, so the seed image must come out at exactly the box.
    auto spec = boxed(ResizeSpec::FitWithin, QSize(500, 333));
    QCOMPARE(ResizeCopy::targetSize(QSize(1000, 667), spec), QSize(500, 333));
}

void ResizeCopyRulesTest::exactIgnoresAspect() {
    QCOMPARE(ResizeCopy::targetSize(QSize(400, 200), boxed(ResizeSpec::Exact, QSize(300, 50))), QSize(300, 50));
}

void ResizeCopyRulesTest::longEdgeSetsTheLongerSideOfEachImage() {
    ResizeSpec spec;
    spec.mode = ResizeSpec::LongEdge;
    spec.longEdge = 200;
    QCOMPARE(ResizeCopy::targetSize(QSize(400, 300), spec), QSize(200, 150));
    QCOMPARE(ResizeCopy::targetSize(QSize(300, 400), spec), QSize(150, 200));
    QCOMPARE(ResizeCopy::targetSize(QSize(1000, 1000), spec), QSize(200, 200));
    // the long side is exact and the short one rounds: 400 * 100/600 = 66.67 -> 67
    spec.longEdge = 100;
    QCOMPARE(ResizeCopy::targetSize(QSize(600, 400), spec), QSize(100, 67));
    // already at or below the long edge: nothing to shrink
    QVERIFY(!ResizeCopy::targetSize(QSize(80, 60), spec).isValid());
}

void ResizeCopyRulesTest::shrinkOnlySkipsAnythingNotSmaller() {
    QVERIFY(!ResizeCopy::targetSize(QSize(400, 200), percent(150)).isValid());
    QCOMPARE(ResizeCopy::targetSize(QSize(400, 200), percent(150, false)), QSize(600, 300));
    // already fits inside the box: nothing to shrink
    QVERIFY(!ResizeCopy::targetSize(QSize(100, 50), boxed(ResizeSpec::FitWithin, QSize(200, 200))).isValid());
    // one side grows -> not a shrink
    QVERIFY(!ResizeCopy::targetSize(QSize(400, 200), boxed(ResizeSpec::Exact, QSize(300, 250))).isValid());
}

void ResizeCopyRulesTest::unchangedSizeIsAlwaysSkipped() {
    QVERIFY(!ResizeCopy::targetSize(QSize(400, 200), percent(100, false)).isValid());
    QVERIFY(!ResizeCopy::targetSize(QSize(400, 200), boxed(ResizeSpec::Exact, QSize(400, 200))).isValid());
    QVERIFY(!ResizeCopy::targetSize(QSize(), percent(50)).isValid());
}

void ResizeCopyRulesTest::copyPathAddsSizeSuffixAndAvoidsCollisions() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString src = dir.filePath("photo.jpg");
    const QString first = dir.filePath("photo_200x100.jpg");
    QCOMPARE(ResizeCopy::copyPath(src, QSize(200, 100)), first);

    QVERIFY(writeSizedImage(first, QSize(2, 2)));
    QCOMPARE(ResizeCopy::copyPath(src, QSize(200, 100)), dir.filePath("photo_200x100-1.jpg"));

    // a name already claimed by this batch counts as taken too
    QSet<QString> reserved{dir.filePath("photo_200x100-1.jpg")};
    QCOMPARE(ResizeCopy::copyPath(src, QSize(200, 100), reserved), dir.filePath("photo_200x100-2.jpg"));

    // only the last suffix is the extension
    QCOMPARE(ResizeCopy::copyPath(dir.filePath("archive.tar.png"), QSize(3, 4)),
             dir.filePath("archive.tar_3x4.png"));
}

void ResizeCopyRulesTest::resizeFileWritesACopyAndLeavesTheOriginal() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString src = dir.filePath("wide.png");
    QVERIFY(writeSizedImage(src, QSize(64, 32)));
    const QByteArray before = fileHash(src);

    QSet<QString> reserved;
    ResizeCopy::Result result = ResizeCopy::resizeFile(src, percent(50), &reserved);
    QCOMPARE(result.outcome, ResizeCopy::Outcome::Resized);
    QCOMPARE(result.destPath, dir.filePath("wide_32x16.png"));
    QVERIFY(reserved.contains(result.destPath));
    QCOMPARE(QImageReader(result.destPath).size(), QSize(32, 16));

    QCOMPARE(fileHash(src), before);
    QCOMPARE(QImageReader(src).size(), QSize(64, 32));

    // running it again picks a fresh name instead of overwriting the copy
    ResizeCopy::Result again = ResizeCopy::resizeFile(src, percent(50), &reserved);
    QCOMPARE(again.outcome, ResizeCopy::Outcome::Resized);
    QCOMPARE(again.destPath, dir.filePath("wide_32x16-1.png"));
}

void ResizeCopyRulesTest::resizeFileSkipsWithoutWriting() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString png = dir.filePath("small.png");
    QVERIFY(writeSizedImage(png, QSize(20, 10)));
    const QString txt = dir.filePath("notes.txt");
    {
        QFile notes(txt);
        QVERIFY(notes.open(QIODevice::WriteOnly));
        notes.write("not an image");
    }

    QCOMPARE(ResizeCopy::resizeFile(png, percent(200)).outcome, ResizeCopy::Outcome::Skipped);
    QCOMPARE(ResizeCopy::resizeFile(txt, percent(50)).outcome, ResizeCopy::Outcome::Skipped);
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 2);
}

TG_BEHAVIOR_TEST_MAIN(ResizeCopyRulesTest)

#include "test_resize_copy_rules.moc"
