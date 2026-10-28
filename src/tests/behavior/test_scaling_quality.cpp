#include "support/thumbgrid_test_support.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core.h"
#include "gui/dialogs/settingsdialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/viewers/imageviewerv2.h"
#include "utils/imagelib.h"

// Image scaling without OpenCV: "High quality" is bicubic, and below half size Qt's
// smooth scaling plus an unsharp mask, which is the look the OpenCV filters gave.
class ScalingQualityTest : public QObject {
    Q_OBJECT

private slots:
    void defaultsAndStoredOpenCvValuesMigrate();
    void sharpenLeavesFlatAreasAndAlphaAlone();
    void bicubicReproducesALinearGradient();
    void bicubicKeepsPremultipliedAlphaValid();
    void highQualityPicksItsPathBySize();
    void settingsListTheFiltersAndScopeSharpening();
    void viewerRequestsHighQualityAndHonoursSharpenAndNearest();
};

namespace {

QImage stripes(const QSize &size) {
    QImage img(size, QImage::Format_RGB32);
    for(int y = 0; y < size.height(); y++)
        for(int x = 0; x < size.width(); x++)
            img.setPixel(x, y, ((x / 40) % 2) ? qRgb(230, 230, 230) : qRgb(30, 30, 30));
    return img;
}

int contrast(const QImage &img) {
    int lo = 255, hi = 0;
    for(int x = 0; x < img.width(); x++) {
        const int v = qGray(img.pixel(x, img.height() / 2));
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    return hi - lo;
}

} // namespace

void ScalingQualityTest::defaultsAndStoredOpenCvValuesMigrate() {
    QCOMPARE(settings->scalingFilter(), QI_FILTER_HIGH_QUALITY);
    QVERIFY(settings->scalingSharpen());
    // a default QSettings shares Settings' store; these keys live in [Document]
    QSettings conf;
    const QList<QPair<int, ScalingFilter>> viewer{{0, QI_FILTER_NEAREST}, {1, QI_FILTER_BILINEAR}, {2, QI_FILTER_BILINEAR},
                                                  {3, QI_FILTER_HIGH_QUALITY}, {4, QI_FILTER_HIGH_QUALITY}, {99, QI_FILTER_HIGH_QUALITY}};
    for(const auto &[stored, expected] : viewer) {
        conf.setValue("Document/scalingFilter", stored);
        QCOMPARE(settings->scalingFilter(), expected);
    }
    const QList<QPair<int, ScalingFilter>> resize{{0, QI_FILTER_NEAREST}, {1, QI_FILTER_BILINEAR}, {2, QI_FILTER_BILINEAR},
                                                  {3, QI_FILTER_HIGH_QUALITY}, {4, QI_FILTER_HIGH_QUALITY_SHARPEN}, {99, QI_FILTER_BILINEAR}};
    for(const auto &[stored, expected] : resize) {
        conf.setValue("Document/resizeFilter", stored);
        QCOMPARE(settings->resizeFilter(), expected);
    }
    settings->setScalingFilter(QI_FILTER_HIGH_QUALITY);
    settings->setResizeFilter(QI_FILTER_BILINEAR);
}

void ScalingQualityTest::sharpenLeavesFlatAreasAndAlphaAlone() {
    QImage flat(64, 64, QImage::Format_RGB32);
    flat.fill(qRgb(120, 60, 200));
    QImage sharpenedFlat = flat;
    ImageLib::sharpen(sharpenedFlat);
    QCOMPARE(sharpenedFlat, flat);

    QImage edges = stripes(QSize(200, 50));
    QImage sharpenedEdges = edges;
    ImageLib::sharpen(sharpenedEdges);
    QVERIFY2(contrast(sharpenedEdges) > contrast(edges), "an edge should gain contrast");

    QImage translucent(64, 64, QImage::Format_ARGB32_Premultiplied);
    translucent.fill(Qt::transparent);
    for(int y = 0; y < 64; y++)
        for(int x = 32; x < 64; x++)
            translucent.setPixel(x, y, qPremultiply(qRgba(250, 20, 20, 128)));
    QImage sharpenedTranslucent = translucent;
    ImageLib::sharpen(sharpenedTranslucent);
    bool alphaKept = true, premultipliedValid = true;
    for(int y = 0; y < 64; y++)
        for(int x = 0; x < 64; x++) {
            alphaKept &= qAlpha(translucent.pixel(x, y)) == qAlpha(sharpenedTranslucent.pixel(x, y));
            const QRgb raw = reinterpret_cast<const QRgb *>(sharpenedTranslucent.constScanLine(y))[x];
            premultipliedValid &= qRed(raw) <= qAlpha(raw) && qGreen(raw) <= qAlpha(raw) && qBlue(raw) <= qAlpha(raw);
        }
    QVERIFY2(alphaKept, "alpha must not be sharpened");
    QVERIFY2(premultipliedValid, "premultiplied colors must stay within alpha");
}

// A cubic kernel reproduces linear functions exactly, so away from the edges a
// resampled ramp must still be the same ramp.
void ScalingQualityTest::bicubicReproducesALinearGradient() {
    QImage ramp(1000, 8, QImage::Format_RGB32);
    for(int y = 0; y < ramp.height(); y++)
        for(int x = 0; x < ramp.width(); x++) {
            const int v = x * 255 / 999;
            ramp.setPixel(x, y, qRgb(v, v, v));
        }
    const QImage out = ImageLib::bicubicScaled(ramp, QSize(840, 8));
    int worst = 0;
    for(int x = 4; x < out.width() - 4; x++) {
        const double source = (x + 0.5) * 1000.0 / 840 - 0.5;
        worst = std::max(worst, std::abs(qRed(out.pixel(x, 4)) - int(std::lround(source * 255 / 999))));
    }
    QVERIFY2(worst <= 1, qPrintable(QString("ramp off by %1").arg(worst)));
}

void ScalingQualityTest::bicubicKeepsPremultipliedAlphaValid() {
    QImage edge(200, 20, QImage::Format_ARGB32);
    edge.fill(Qt::transparent);
    for(int y = 0; y < 20; y++)
        for(int x = 100; x < 200; x++)
            edge.setPixel(x, y, qRgba(255, 255, 255, 255));
    const QImage out = ImageLib::bicubicScaled(edge, QSize(150, 15));
    QCOMPARE(out.format(), QImage::Format_ARGB32_Premultiplied);
    bool valid = true;
    for(int y = 0; y < out.height(); y++)
        for(int x = 0; x < out.width(); x++) {
            const QRgb raw = reinterpret_cast<const QRgb *>(out.constScanLine(y))[x];
            valid &= qRed(raw) <= qAlpha(raw) && qGreen(raw) <= qAlpha(raw) && qBlue(raw) <= qAlpha(raw);
        }
    QVERIFY2(valid, "cubic overshoot must not leave colors above alpha");
}

void ScalingQualityTest::highQualityPicksItsPathBySize() {
    const auto source = std::make_shared<const QImage>(stripes(QSize(1600, 400)));
    const QSize mild(1200, 300), strong(400, 100);
    const QImage bicubic = ImageLib::bicubicScaled(*source, mild);
    QImage smooth = source->scaled(strong, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QCOMPARE(*ImageLib::scaled(source, mild, QI_FILTER_HIGH_QUALITY), bicubic);
    QCOMPARE(*ImageLib::scaled(source, mild, QI_FILTER_HIGH_QUALITY_SOFT), bicubic);
    QImage bicubicSharpened = bicubic;
    ImageLib::sharpen(bicubicSharpened);
    QCOMPARE(*ImageLib::scaled(source, mild, QI_FILTER_HIGH_QUALITY_SHARPEN), bicubicSharpened);

    QCOMPARE(*ImageLib::scaled(source, strong, QI_FILTER_HIGH_QUALITY_SOFT), smooth);
    ImageLib::sharpen(smooth);
    QCOMPARE(*ImageLib::scaled(source, strong, QI_FILTER_HIGH_QUALITY), smooth);
}

void ScalingQualityTest::settingsListTheFiltersAndScopeSharpening() {
    settings->setScalingFilter(QI_FILTER_HIGH_QUALITY);
    settings->setScalingSharpen(true);
    SettingsDialog dialog;
    auto *filter = dialog.findChild<QComboBox *>("scalingQualityComboBox");
    auto *sharpen = dialog.findChild<QCheckBox *>("scalingSharpenCheckBox");
    QVERIFY(filter && sharpen);
    QCOMPARE(filter->count(), 3);
    QCOMPARE(filter->currentData().toInt(), int(QI_FILTER_HIGH_QUALITY));
    QVERIFY(sharpen->isChecked());
    QVERIFY(sharpen->isEnabled());

    filter->setCurrentIndex(filter->findData(QI_FILTER_BILINEAR));
    QVERIFY(!sharpen->isEnabled());
    sharpen->setChecked(false);
    QVERIFY(QMetaObject::invokeMethod(&dialog, "saveSettings", Qt::DirectConnection));
    QCOMPARE(settings->scalingFilter(), QI_FILTER_BILINEAR);
    QVERIFY(!settings->scalingSharpen());

    settings->setScalingFilter(QI_FILTER_HIGH_QUALITY);
    settings->setScalingSharpen(true);
}

void ScalingQualityTest::viewerRequestsHighQualityAndHonoursSharpenAndNearest() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());
    QVERIFY(root.mkpath("gallery"));
    QVERIFY(stripes(QSize(2400, 1600)).save(root.filePath("gallery/photo.png"), "PNG"));
    settings->setScalingFilter(QI_FILTER_HIGH_QUALITY);
    settings->setScalingSharpen(true);

    QSet<QWidget *> preexisting;
    for(QWidget *w : QApplication::topLevelWidgets())
        preexisting.insert(w);
    auto core = std::make_unique<Core>();
    QVERIFY(core->loadPath(root.filePath("gallery")));
    core->showGui();
    MW *window = nullptr;
    for(int i = 0; i < 200 && !window; ++i) {
        for(QWidget *w : QApplication::topLevelWidgets())
            if(!preexisting.contains(w))
                if(auto candidate = qobject_cast<MW *>(w))
                    window = candidate;
        QCoreApplication::processEvents();
    }
    QVERIFY(window);
    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY(grid);
    QTRY_VERIFY(grid->itemCount() >= 2);
    window->resize(900, 500);
    QTest::qWait(150);
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    auto viewer = window->findChild<ImageViewerV2 *>();
    QVERIFY(viewer);

    qRegisterMetaType<ScalingFilter>("ScalingFilter");
    QSignalSpy requests(viewer, &ImageViewerV2::scalingRequested);
    const auto lastFilter = [&requests]() { return requests.last().at(1).value<ScalingFilter>(); };
    const auto rescale = [viewer]() { QMetaObject::invokeMethod(viewer, "requestScaling", Qt::DirectConnection); };

    rescale();
    QTRY_VERIFY(!requests.isEmpty());
    QCOMPARE(lastFilter(), QI_FILTER_HIGH_QUALITY);

    requests.clear();
    settings->setScalingSharpen(false);
    settings->sendChangeNotification();
    QTRY_VERIFY(!requests.isEmpty());
    QCOMPARE(lastFilter(), QI_FILTER_HIGH_QUALITY_SOFT);

    requests.clear();
    settings->setScalingFilter(QI_FILTER_NEAREST);
    settings->sendChangeNotification();
    rescale();
    QTRY_VERIFY(!requests.isEmpty());
    QCOMPARE(lastFilter(), QI_FILTER_NEAREST);

    settings->setScalingFilter(QI_FILTER_HIGH_QUALITY);
    settings->setScalingSharpen(true);
}

TG_BEHAVIOR_TEST_MAIN(ScalingQualityTest)
#include "test_scaling_quality.moc"
