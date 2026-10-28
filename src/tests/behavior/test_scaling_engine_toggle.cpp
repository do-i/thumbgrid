#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/viewers/imageviewerv2.h"
#include "utils/imagelib.h"
#include "gui/dialogs/settingsdialog.h"
#include <QComboBox>

// The Qt scaling engine is the default; OpenCV stays selectable (Settings or the
// toggleScalingEngine action) while beta users compare the two.
class ScalingEngineToggleTest : public QObject {
    Q_OBJECT

private slots:
    void qtEngineIsTheDefaultAndPersists();
    void sharpenLeavesFlatAreasAndAlphaAlone();
    void qtSharpenFilterOnlySharpensBelowHalfSize();
    void settingsListOnlyQtFiltersButKeepTheOpenCvChoice();
    void viewerRequestsTheEngineAndTheActionFlipsIt();
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

void ScalingEngineToggleTest::qtEngineIsTheDefaultAndPersists() {
    QVERIFY(!settings->legacyScalingEngine());
    QVERIFY(!settings->scalingSharpen());
    settings->setLegacyScalingEngine(true);
    settings->setScalingSharpen(true);
    QVERIFY(settings->legacyScalingEngine());
    QVERIFY(settings->scalingSharpen());
    settings->setLegacyScalingEngine(false);
    settings->setScalingSharpen(false);
    QVERIFY(!settings->legacyScalingEngine());
}

void ScalingEngineToggleTest::sharpenLeavesFlatAreasAndAlphaAlone() {
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
            const QRgb before = translucent.pixel(x, y), after = sharpenedTranslucent.pixel(x, y);
            alphaKept &= qAlpha(before) == qAlpha(after);
            const QRgb raw = reinterpret_cast<const QRgb *>(sharpenedTranslucent.constScanLine(y))[x];
            premultipliedValid &= qRed(raw) <= qAlpha(raw) && qGreen(raw) <= qAlpha(raw) && qBlue(raw) <= qAlpha(raw);
        }
    QVERIFY2(alphaKept, "alpha must not be sharpened");
    QVERIFY2(premultipliedValid, "premultiplied colors must stay within alpha");
}

void ScalingEngineToggleTest::qtSharpenFilterOnlySharpensBelowHalfSize() {
    const auto source = std::make_shared<const QImage>(stripes(QSize(1600, 400)));
    const QSize small(400, 100), large(1200, 300);
    QCOMPARE(*ImageLib::scaled(source, large, QI_FILTER_QT_SMOOTH_SHARPEN),
             *ImageLib::scaled(source, large, QI_FILTER_BILINEAR));
    QVERIFY(*ImageLib::scaled(source, small, QI_FILTER_QT_SMOOTH_SHARPEN) !=
            *ImageLib::scaled(source, small, QI_FILTER_BILINEAR));
}

// The Qt engine lists Nearest / Smooth only; a stored OpenCV filter survives a save
// so switching back to the legacy engine restores it.
void ScalingEngineToggleTest::settingsListOnlyQtFiltersButKeepTheOpenCvChoice() {
    settings->setScalingFilter(QI_FILTER_CV_CUBIC);
    settings->setLegacyScalingEngine(false);
    {
        SettingsDialog dialog;
        auto *engine = dialog.findChild<QComboBox *>("scalingEngineComboBox");
        auto *filter = dialog.findChild<QComboBox *>("scalingQualityComboBox");
        QVERIFY(engine && filter);
        QCOMPARE(filter->count(), 2);
        QCOMPARE(filter->currentIndex(), 1);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "saveSettings", Qt::DirectConnection));
        QCOMPARE(settings->scalingFilter(), QI_FILTER_CV_CUBIC);
#ifdef USE_OPENCV
        engine->setCurrentIndex(1);
        QCOMPARE(filter->count(), 5);
        QCOMPARE(filter->currentIndex(), int(QI_FILTER_CV_CUBIC));
        engine->setCurrentIndex(0);
#endif
        filter->setCurrentIndex(0);
        emit filter->activated(0);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "saveSettings", Qt::DirectConnection));
        QCOMPARE(settings->scalingFilter(), QI_FILTER_NEAREST);
    }
    settings->setScalingFilter(QI_FILTER_CV_CUBIC);
}

void ScalingEngineToggleTest::viewerRequestsTheEngineAndTheActionFlipsIt() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());
    QVERIFY(root.mkpath("gallery"));
    QVERIFY(stripes(QSize(2400, 1600)).save(root.filePath("gallery/photo.png"), "PNG"));

    settings->setScalingFilter(QI_FILTER_CV_CUBIC); // the legacy default
    settings->setLegacyScalingEngine(false);
    settings->setScalingSharpen(false);

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

    // Qt engine by default: the saved OpenCV filter scales with Qt smooth
    settings->sendChangeNotification();
    QMetaObject::invokeMethod(viewer, "requestScaling", Qt::DirectConnection);
    QTRY_VERIFY(!requests.isEmpty());
    QCOMPARE(lastFilter(), QI_FILTER_BILINEAR);

#ifdef USE_OPENCV
    requests.clear();
    QVERIFY(actionManager->invokeAction("toggleScalingEngine"));
    QTRY_VERIFY(!requests.isEmpty());
    QCOMPARE(lastFilter(), QI_FILTER_CV_CUBIC);

    requests.clear();
    QVERIFY(actionManager->invokeAction("toggleScalingEngine"));
    QTRY_VERIFY(!requests.isEmpty());
    QCOMPARE(lastFilter(), QI_FILTER_BILINEAR);
#endif

    requests.clear();
    settings->setScalingSharpen(true);
    settings->sendChangeNotification();
    QTRY_VERIFY(!requests.isEmpty());
    QCOMPARE(lastFilter(), QI_FILTER_QT_SMOOTH_SHARPEN);

    // nearest is honored by the Qt engine too
    requests.clear();
    settings->setScalingFilter(QI_FILTER_NEAREST);
    settings->sendChangeNotification();
    QMetaObject::invokeMethod(viewer, "requestScaling", Qt::DirectConnection);
    QTRY_VERIFY(!requests.isEmpty());
    QCOMPARE(lastFilter(), QI_FILTER_NEAREST);
}

TG_BEHAVIOR_TEST_MAIN(ScalingEngineToggleTest)
#include "test_scaling_engine_toggle.moc"
