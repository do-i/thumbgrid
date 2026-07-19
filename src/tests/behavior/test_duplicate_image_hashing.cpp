#include "support/thumbgrid_test_support.h"
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include "components/duplicatefinder/imagehasher.h"

// Deterministic test scenes with distinct coarse structure (pHash operates
// on low-frequency content, so scenes must differ at large scale).
static QImage makeScene(int id, int w = 512, int h = 384) {
    QImage img(w, h, QImage::Format_RGB32);
    QPainter p(&img);
    switch(id) {
    case 0: {
        QLinearGradient g(0, 0, w, h);
        g.setColorAt(0, QColor(230, 225, 210));
        g.setColorAt(1, QColor(40, 60, 90));
        p.fillRect(img.rect(), g);
        p.setBrush(QColor(25, 20, 30));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(w / 4, h / 4), w / 6, w / 6);
        p.setBrush(QColor(240, 200, 60));
        p.drawRect(w / 2, h / 2, w / 3, h / 5);
        break;
    }
    case 1:
        for(int y = 0; y < 3; y++)
            for(int x = 0; x < 4; x++)
                p.fillRect(x * w / 4, y * h / 3, w / 4, h / 3,
                           ((x + y) % 2) ? QColor(235, 235, 235) : QColor(30, 30, 30));
        break;
    case 2:
        for(int y = 0; y < 6; y++)
            p.fillRect(0, y * h / 6, w, h / 6,
                       (y % 2) ? QColor(200, 60, 60) : QColor(60, 60, 200));
        break;
    default:
        p.fillRect(img.rect(), QColor(220, 220, 220));
        p.setPen(Qt::NoPen);
        for(int i = 0; i < 4; i++) {
            p.setBrush(QColor(40 + i * 50, 40, 120));
            p.drawRect(w / 2 - (4 - i) * w / 10, h / 2 - (4 - i) * h / 10,
                       (4 - i) * w / 5, (4 - i) * h / 5);
        }
        break;
    }
    p.end();
    return img;
}

class DuplicateImageHashingTest : public QObject {
    Q_OBJECT
private slots:
    void reencodedAndRescaledCopiesMatch();
    void distinctScenesDiffer();
    void rotatedAndMirroredCopiesMatchViaVariants();
    void thresholdMapping();
    void contentHashDetectsExactCopies();
};

void DuplicateImageHashingTest::reencodedAndRescaledCopiesMatch() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QImage scene = makeScene(0);
    QString png = dir.filePath("orig.png");
    QString jpg = dir.filePath("reencoded.jpg");
    QString small = dir.filePath("rescaled.jpg");
    QVERIFY(scene.save(png));
    QVERIFY(scene.save(jpg, "JPG", 80));
    QVERIFY(scene.scaled(scene.size() / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                .save(small, "JPG", 90));

    auto hPng = ImageHasher::phashFromFile(png);
    auto hJpg = ImageHasher::phashFromFile(jpg);
    auto hSmall = ImageHasher::phashFromFile(small);
    QVERIFY(hPng && hJpg && hSmall);

    int threshold = ImageHasher::maxDistanceForSimilarity(87);
    for(auto [a, b] : {std::pair{*hPng, *hJpg}, {*hPng, *hSmall}, {*hJpg, *hSmall}}) {
        int d = ImageHasher::hammingDistance(a, b);
        QVERIFY2(d <= threshold, qPrintable(QString("copy distance %1 > %2").arg(d).arg(threshold)));
    }
}

void DuplicateImageHashingTest::distinctScenesDiffer() {
    quint64 hashes[4];
    for(int i = 0; i < 4; i++)
        hashes[i] = ImageHasher::phash(makeScene(i));
    int threshold = ImageHasher::maxDistanceForSimilarity(87);
    for(int i = 0; i < 4; i++) {
        for(int j = i + 1; j < 4; j++) {
            int d = ImageHasher::hammingDistance(hashes[i], hashes[j]);
            QVERIFY2(d > threshold, qPrintable(
                QString("scenes %1/%2 distance %3 <= %4").arg(i).arg(j).arg(d).arg(threshold)));
        }
    }
}

void DuplicateImageHashingTest::rotatedAndMirroredCopiesMatchViaVariants() {
    QImage scene = makeScene(0);
    int threshold = ImageHasher::maxDistanceForSimilarity(87);

    QImage rotated = scene.transformed(QTransform().rotate(90));
    quint64 hRotated = ImageHasher::phash(rotated);
    QVERIFY2(ImageHasher::hammingDistance(ImageHasher::phash(scene), hRotated) > threshold,
             "rotated copy should NOT match without variants");
    auto withRotations = ImageHasher::phashVariants(scene, true, false);
    QCOMPARE(withRotations.size(), 4);
    bool found = false;
    for(quint64 v : withRotations)
        found = found || ImageHasher::hammingDistance(v, hRotated) <= threshold;
    QVERIFY2(found, "rotation variants should match a rotated copy");

    QImage mirrored = scene.mirrored(true, false);
    quint64 hMirrored = ImageHasher::phash(mirrored);
    auto withMirrors = ImageHasher::phashVariants(scene, false, true);
    QCOMPARE(withMirrors.size(), 2);
    found = false;
    for(quint64 v : withMirrors)
        found = found || ImageHasher::hammingDistance(v, hMirrored) <= threshold;
    QVERIFY2(found, "mirror variants should match a mirrored copy");

    QCOMPARE(ImageHasher::phashVariants(scene, true, true).size(), 8);
}

void DuplicateImageHashingTest::thresholdMapping() {
    QCOMPARE(ImageHasher::maxDistanceForSimilarity(87), 8);
    QCOMPARE(ImageHasher::maxDistanceForSimilarity(100), 0);
    QCOMPARE(ImageHasher::maxDistanceForSimilarity(0), 64);
    quint64 h = ImageHasher::phash(makeScene(0));
    QCOMPARE(ImageHasher::similarityPercent(h, h), 100.0);
    QCOMPARE(ImageHasher::hammingDistance(h, h), 0);
}

void DuplicateImageHashingTest::contentHashDetectsExactCopies() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QImage scene = makeScene(1);
    QString a = dir.filePath("a.png");
    QString b = dir.filePath("b.png");
    QVERIFY(scene.save(a));
    QVERIFY(QFile::copy(a, b));
    QVERIFY(scene.save(dir.filePath("c.jpg"), "JPG", 80));

    QByteArray ha = ImageHasher::contentHash(a);
    QVERIFY(!ha.isEmpty());
    QCOMPARE(ha, ImageHasher::contentHash(b));
    QVERIFY(ha != ImageHasher::contentHash(dir.filePath("c.jpg")));
    QVERIFY(ImageHasher::contentHash(dir.filePath("missing.png")).isEmpty());
}

TG_BEHAVIOR_TEST_MAIN(DuplicateImageHashingTest)

#include "test_duplicate_image_hashing.moc"
