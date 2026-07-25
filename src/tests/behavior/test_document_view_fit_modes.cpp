#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSet>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/viewers/imageviewerv2.h"

// Two fit-mode behaviours that were wrong in ways only visible on screen.
//
// Both use one square image in a deliberately landscape viewport - the shape
// that exposes them. A square image is wider-relative-to-the-window than the
// window itself, so:
//   - at 1:1 it overflows *both* axes, the case where nothing centered it;
//   - fit-window is height-limited, so a height-only "stretch" computed the
//     exact same scale and the mode appeared to do nothing.

namespace {

constexpr int kImageSide = 1200;
constexpr int kViewportW = 900;
constexpr int kViewportH = 500;
// comfortably smaller than the viewport on both axes
constexpr int kSmallW = 400;
constexpr int kSmallH = 300;

} // namespace

class DocumentViewFitModesTest : public QObject {
    Q_OBJECT

private:
    // Opens a square image in document view and returns its viewer, sized so the
    // viewport is landscape. Returns nullptr on any setup failure.
    ImageViewerV2 *openSquareImage(QTemporaryDir &fixture, std::unique_ptr<Core> &core);
    // Same flow, but with an image comfortably smaller than the viewport.
    ImageViewerV2 *openSmallImage(QTemporaryDir &fixture, std::unique_ptr<Core> &core);

private slots:
    void oversizedImageAtOneToOneOpensCentered();
    void stretchFillsTheWindowInsteadOfRepeatingFitWindow();
    void stretchActionReachesTheViewer();
    void stretchIsReachableAsTheDefaultFitMode();
    void explicitFitExpandsASmallImageEvenWithExpandImageOff();
    void openingAnImageStillHonoursExpandImageOff();
};

namespace {

// Shared open flow: build a one-image gallery, show the window at a fixed
// landscape size, then enter document view on that image.
ImageViewerV2 *openGalleryImage(QTemporaryDir &fixture, std::unique_ptr<Core> &core,
                                const QSize &imageSize, const QString &fileName) {
    if(!fixture.isValid())
        return nullptr;
    QDir root(fixture.path());
    if(!root.mkpath("gallery"))
        return nullptr;
    const QString galleryPath = root.filePath("gallery");
    QImage image(imageSize, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    if(!image.save(root.filePath("gallery/" + fileName), "PNG"))
        return nullptr;

    // Earlier slots' windows can still be alive (they are deleted via
    // deleteLater), and tgtest::mainWindow() hands back the first one it finds -
    // which would silently point every assertion at a previous test's image.
    // Remember what already exists and wait for the window this Core creates.
    QSet<QWidget *> preexisting;
    for(QWidget *w : QApplication::topLevelWidgets())
        if(qobject_cast<MW *>(w))
            preexisting.insert(w);

    core.reset(new Core());
    if(!core->loadPath(galleryPath))
        return nullptr;
    core->showGui();

    MW *window = nullptr;
    for(int i = 0; i < 200 && !window; ++i) {
        for(QWidget *w : QApplication::topLevelWidgets()) {
            if(preexisting.contains(w))
                continue;
            if(auto candidate = qobject_cast<MW *>(w)) {
                window = candidate;
                break;
            }
        }
        QCoreApplication::processEvents();
    }
    if(!window)
        return nullptr;

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    if(!grid)
        return nullptr;
    for(int i = 0; i < 200 && grid->itemCount() < 2; ++i)
        QCoreApplication::processEvents();

    // Settle the window into a landscape shape *before* opening the image, so it
    // is laid out once, at load, the way a user opening a picture sees it.
    window->resize(kViewportW, kViewportH);
    QTest::qWait(150);

    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    for(int i = 0; i < 200 && window->currentViewMode() != MODE_DOCUMENT; ++i)
        QCoreApplication::processEvents();

    auto viewer = window->findChild<ImageViewerV2 *>();
    if(!viewer)
        return nullptr;
    QTest::qWait(150);
    return viewer;
}

} // namespace

ImageViewerV2 *DocumentViewFitModesTest::openSquareImage(QTemporaryDir &fixture,
                                                         std::unique_ptr<Core> &core) {
    return openGalleryImage(fixture, core, QSize(kImageSide, kImageSide),
                            QStringLiteral("square.png"));
}

ImageViewerV2 *DocumentViewFitModesTest::openSmallImage(QTemporaryDir &fixture,
                                                        std::unique_ptr<Core> &core) {
    return openGalleryImage(fixture, core, QSize(kSmallW, kSmallH),
                            QStringLiteral("small.png"));
}

// reset() parks the scene on the pixmap's top-left. At 1:1 nothing moved it
// back: the anchored zoom is a no-op because the scale is already 1.0, and
// neither centerIfNecessary() nor snapToEdges() touches an image that overflows
// both axes - so a large image opened showing only its top-left corner.
void DocumentViewFitModesTest::oversizedImageAtOneToOneOpensCentered() {
    settings->setImageFitMode(FIT_ORIGINAL);
    settings->setFocusPointIn1to1Mode(FOCUS_CENTER);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    std::unique_ptr<Core> core;
    ImageViewerV2 *viewer = openSquareImage(fixture, core);
    QVERIFY2(viewer != nullptr, "The document image viewer should exist.");
    QTRY_VERIFY2(viewer->isDisplaying(), "The square image should be displayed.");

    // No interaction: this is how the picture looks the moment it opens.
    QCOMPARE(viewer->fitMode(), FIT_ORIGINAL);
    const QRect imageRect = viewer->scaledRectR();
    const QRect vport = viewer->viewport()->rect();
    QVERIFY2(imageRect.width() > vport.width() && imageRect.height() > vport.height(),
             "This check is only meaningful while the image overflows both axes.");

    // Centered means equal overhang on opposite sides. The top-left-corner bug
    // put the whole overhang on the right and bottom instead.
    const int leftOver = -imageRect.left();
    const int rightOver = imageRect.right() - vport.right();
    const int topOver = -imageRect.top();
    const int bottomOver = imageRect.bottom() - vport.bottom();

    QVERIFY2(qAbs(leftOver - rightOver) <= 2,
             qPrintable(QStringLiteral("horizontal overhang should be even, got left=%1 right=%2")
                            .arg(leftOver)
                            .arg(rightOver)));
    QVERIFY2(qAbs(topOver - bottomOver) <= 2,
             qPrintable(QStringLiteral("vertical overhang should be even, got top=%1 bottom=%2")
                            .arg(topOver)
                            .arg(bottomOver)));
}

// "Fit in window (stretch)" scaled on height alone, which is exactly
// min(scaleX, scaleY) - i.e. fitWindowScale - for any image narrower than the
// window. Picking the larger ratio makes it fill the window for real.
void DocumentViewFitModesTest::stretchFillsTheWindowInsteadOfRepeatingFitWindow() {
    settings->setImageFitMode(FIT_WINDOW);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    std::unique_ptr<Core> core;
    ImageViewerV2 *viewer = openSquareImage(fixture, core);
    QVERIFY2(viewer != nullptr, "The document image viewer should exist.");
    QTRY_VERIFY2(viewer->isDisplaying(), "The square image should be displayed.");

    viewer->setFitWindow();
    QTest::qWait(150);
    const float fitWindowScale = viewer->currentScale();
    QCOMPARE(viewer->fitMode(), FIT_WINDOW);

    viewer->setFitWindowStretch();
    QTest::qWait(150);
    const float stretchScale = viewer->currentScale();

    QCOMPARE(viewer->fitMode(), FIT_WINDOW_STRETCH);
    QVERIFY2(stretchScale > fitWindowScale,
             qPrintable(QStringLiteral("stretch must differ from fit-window, got %1 vs %2")
                            .arg(static_cast<double>(stretchScale))
                            .arg(static_cast<double>(fitWindowScale))));

    // Filling the window means no empty bar on the previously letterboxed axis.
    const QRect imageRect = viewer->scaledRectR();
    const QRect vport = viewer->viewport()->rect();
    QVERIFY2(imageRect.width() >= vport.width(),
             qPrintable(QStringLiteral("stretch should span the viewport width, got %1 of %2")
                            .arg(imageRect.width())
                            .arg(vport.width())));
}

// The scale fix above is only half the story: the context menu button does not
// call the viewer, it fires the "fitWindowStretch" *action*. This drives that
// whole chain - action -> MW -> ViewerWidget -> viewer - exactly as a click
// does, so a break anywhere along it is caught.
void DocumentViewFitModesTest::stretchActionReachesTheViewer() {
    settings->setImageFitMode(FIT_WINDOW);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    std::unique_ptr<Core> core;
    ImageViewerV2 *viewer = openSquareImage(fixture, core);
    QVERIFY2(viewer != nullptr, "The document image viewer should exist.");
    QTRY_VERIFY2(viewer->isDisplaying(), "The square image should be displayed.");

    viewer->setFitWindow();
    QTest::qWait(150);
    const float fitWindowScale = viewer->currentScale();

    QVERIFY2(actionManager->invokeAction("fitWindowStretch"),
             "fitWindowStretch should be a known, invocable action.");
    QTest::qWait(200);

    QCOMPARE(viewer->fitMode(), FIT_WINDOW_STRETCH);
    QVERIFY2(viewer->currentScale() > fitWindowScale,
             qPrintable(QStringLiteral("the action should have restretched, got %1 vs %2")
                            .arg(static_cast<double>(viewer->currentScale()))
                            .arg(static_cast<double>(fitWindowScale))));
}

// Choosing "Fit in window (stretch)" as the default fit mode in Settings goes
// through ViewerWidget::setFitMode(), which used to handle only three of the
// four modes and dropped this one on the floor.
void DocumentViewFitModesTest::stretchIsReachableAsTheDefaultFitMode() {
    settings->setImageFitMode(FIT_WINDOW_STRETCH);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    std::unique_ptr<Core> core;
    ImageViewerV2 *viewer = openSquareImage(fixture, core);
    QVERIFY2(viewer != nullptr, "The document image viewer should exist.");
    QTRY_VERIFY2(viewer->isDisplaying(), "The square image should be displayed.");

    QCOMPARE(viewer->fitMode(), FIT_WINDOW_STRETCH);

    const QRect imageRect = viewer->scaledRectR();
    const QRect vport = viewer->viewport()->rect();
    QVERIFY2(imageRect.width() >= vport.width(),
             qPrintable(QStringLiteral("the stretch default should span the viewport width, got %1 of %2")
                            .arg(imageRect.width())
                            .arg(vport.width())));
}

// The reported "stretch wide does not stretch": with "Expand images" off, every
// fit action clamped to 1.0 for an image already smaller than the window, so
// fit-width / fit-window / stretch all left a 400x300 image at 1:1 and the
// buttons looked dead. An explicit fit is a direct instruction and now scales to
// the window regardless of that setting.
void DocumentViewFitModesTest::explicitFitExpandsASmallImageEvenWithExpandImageOff() {
    settings->setImageFitMode(FIT_WINDOW);
    settings->setExpandImage(false);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    std::unique_ptr<Core> core;
    ImageViewerV2 *viewer = openSmallImage(fixture, core);
    QVERIFY2(viewer != nullptr, "The document image viewer should exist.");
    QTRY_VERIFY2(viewer->isDisplaying(), "The small image should be displayed.");

    const QRect vport = viewer->viewport()->rect();
    QVERIFY2(kSmallW < vport.width() && kSmallH < vport.height(),
             "This check is only meaningful while the image is smaller than the viewport.");

    viewer->setFitWidth();
    QTest::qWait(200);
    QVERIFY2(viewer->scaledRectR().width() >= vport.width(),
             qPrintable(QStringLiteral("fit width should span the viewport, got %1 of %2")
                            .arg(viewer->scaledRectR().width())
                            .arg(vport.width())));

    viewer->setFitWindow();
    QTest::qWait(200);
    const QRect fitted = viewer->scaledRectR();
    QVERIFY2(fitted.width() >= vport.width() || fitted.height() >= vport.height(),
             qPrintable(QStringLiteral("fit window should reach an edge, got %1x%2 in %3x%4")
                            .arg(fitted.width()).arg(fitted.height())
                            .arg(vport.width()).arg(vport.height())));

    viewer->setFitWindowStretch();
    QTest::qWait(200);
    QVERIFY2(viewer->scaledRectR().width() >= vport.width(),
             "stretch should fill the viewport width for a small image too");
}

// The other half of that rule: "Expand images" still governs the *automatic*
// fit, so simply opening a small image must leave it at 1:1 as before.
void DocumentViewFitModesTest::openingAnImageStillHonoursExpandImageOff() {
    settings->setImageFitMode(FIT_WINDOW);
    settings->setExpandImage(false);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    std::unique_ptr<Core> core;
    ImageViewerV2 *viewer = openSmallImage(fixture, core);
    QVERIFY2(viewer != nullptr, "The document image viewer should exist.");
    QTRY_VERIFY2(viewer->isDisplaying(), "The small image should be displayed.");

    // Not expanded: still its own 400x300, not scaled up to the 900x477 viewport.
    const QRect shown = viewer->scaledRectR();
    QVERIFY2(shown.width() <= kSmallW + 2 && shown.height() <= kSmallH + 2,
             qPrintable(QStringLiteral("opening should leave the image at 1:1, got %1x%2")
                            .arg(shown.width())
                            .arg(shown.height())));
}

TG_BEHAVIOR_TEST_MAIN(DocumentViewFitModesTest)

#include "test_document_view_fit_modes.moc"
