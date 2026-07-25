#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSet>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/viewers/imageviewerv2.h"

// Fit-mode behaviours that were wrong in ways only visible on screen.
//
// Most use one square image in a deliberately landscape viewport - the shape
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
// Small enough that fit-window wants to scale them past the 2x expand limit
// below, so a clamped fit and an unclamped one are different sizes on screen.
constexpr int kExpandLimit = 2;
constexpr int kTinyW = 200;
constexpr int kTinyH = 150;
constexpr int kNextTinyW = 240;
constexpr int kNextTinyH = 180;

} // namespace

class DocumentViewFitModesTest : public QObject {
    Q_OBJECT

private:
    // Opens a square image in document view and returns its viewer, sized so the
    // viewport is landscape. Returns nullptr on any setup failure.
    ImageViewerV2 *openSquareImage(QTemporaryDir &fixture, std::unique_ptr<Core> &core);
    // Same flow, but with an image comfortably smaller than the viewport.
    ImageViewerV2 *openSmallImage(QTemporaryDir &fixture, std::unique_ptr<Core> &core);
    // Two tiny images in one gallery, opened on the first, so a test can step to
    // the second the way the "next image" shortcut does.
    ImageViewerV2 *openTinyImagePair(QTemporaryDir &fixture, std::unique_ptr<Core> &core);

private slots:
    void oversizedImageAtOneToOneOpensCentered();
    void stretchFillsTheWindowInsteadOfRepeatingFitWindow();
    void stretchActionReachesTheViewer();
    void stretchIsReachableAsTheDefaultFitMode();
    void explicitFitExpandsASmallImageEvenWithExpandImageOff();
    void openingAnImageStillHonoursExpandImageOff();
    void explicitFitDoesNotOutliveTheImageEvenWithKeepFitModeOn();
};

namespace {

bool writeGalleryImage(const QDir &root, const QString &fileName, const QSize &size) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    return image.save(root.filePath("gallery/" + fileName), "PNG");
}

// Shared open flow: build a gallery, show the window at a fixed landscape size,
// then enter document view on its first image. A second image is written too
// when secondSize is valid, so a test can navigate to it.
ImageViewerV2 *openGalleryImage(QTemporaryDir &fixture, std::unique_ptr<Core> &core,
                                const QSize &imageSize, const QString &fileName,
                                const QSize &secondSize = QSize(),
                                const QString &secondName = QString()) {
    if(!fixture.isValid())
        return nullptr;
    QDir root(fixture.path());
    if(!root.mkpath("gallery"))
        return nullptr;
    const QString galleryPath = root.filePath("gallery");
    if(!writeGalleryImage(root, fileName, imageSize))
        return nullptr;
    if(secondSize.isValid() && !writeGalleryImage(root, secondName, secondSize))
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
    // One item per file, plus the entry that sits ahead of them.
    const int expectedItems = secondSize.isValid() ? 3 : 2;
    for(int i = 0; i < 200 && grid->itemCount() < expectedItems; ++i)
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

ImageViewerV2 *DocumentViewFitModesTest::openTinyImagePair(QTemporaryDir &fixture,
                                                           std::unique_ptr<Core> &core) {
    return openGalleryImage(fixture, core, QSize(kTinyW, kTinyH),
                            QStringLiteral("a-tiny.png"),
                            QSize(kNextTinyW, kNextTinyH),
                            QStringLiteral("b-tiny.png"));
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

// "Keep fit mode" is meant to carry the fit *mode* to the next image. It used to
// carry the explicit-fit exemption along with it, because the flag was cleared
// only inside the branch keepFitMode skips: one click on any fit button and
// every later image ignored "Expand images, up to: Nx" for the rest of the
// session. An explicit fit belongs to the image the user clicked on, so the next
// image is laid out under the expand policy again - in the mode that was kept.
void DocumentViewFitModesTest::explicitFitDoesNotOutliveTheImageEvenWithKeepFitModeOn() {
    settings->setImageFitMode(FIT_WINDOW);
    settings->setExpandImage(true);
    settings->setExpandLimit(kExpandLimit);
    settings->setKeepFitMode(true);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    std::unique_ptr<Core> core;
    ImageViewerV2 *viewer = openTinyImagePair(fixture, core);
    QVERIFY2(viewer != nullptr, "The document image viewer should exist.");
    QTRY_VERIFY2(viewer->isDisplaying(), "The first tiny image should be displayed.");
    QCOMPARE(viewer->sourceSize(), QSize(kTinyW, kTinyH));

    const QRect vport = viewer->viewport()->rect();
    QVERIFY2(kTinyW * kExpandLimit < vport.width() && kNextTinyW * kExpandLimit < vport.width(),
             "This check is only meaningful while the expand limit stops short of the viewport width.");
    QVERIFY2(kNextTinyH * kExpandLimit < vport.height(),
             "This check is only meaningful while the expand limit stops short of the viewport height.");

    // The click. An explicit fit is a direct instruction, so it ignores the
    // expand limit and spans the viewport.
    viewer->setFitWidth();
    QTest::qWait(200);
    QCOMPARE(viewer->fitMode(), FIT_WIDTH);
    QVERIFY2(viewer->scaledRectR().width() >= vport.width(),
             qPrintable(QStringLiteral("the explicit fit should span the viewport, got %1 of %2")
                            .arg(viewer->scaledRectR().width())
                            .arg(vport.width())));

    // Step to the next image, the way the "next image" shortcut does. Retried
    // because the second file may still be arriving from the directory loader.
    for(int i = 0; i < 100 && viewer->sourceSize() != QSize(kNextTinyW, kNextTinyH); ++i) {
        QVERIFY2(actionManager->invokeAction("nextImage"),
                 "nextImage should be a known, invocable action.");
        QTest::qWait(50);
    }
    QCOMPARE(viewer->sourceSize(), QSize(kNextTinyW, kNextTinyH));

    // The mode carried over - that is what keepFitMode is for - but the expand
    // limit applies again, so the new image stops at 2x instead of filling 900.
    QCOMPARE(viewer->fitMode(), FIT_WIDTH);
    const QRect shown = viewer->scaledRectR();
    QVERIFY2(shown.width() <= kNextTinyW * kExpandLimit + 2,
             qPrintable(QStringLiteral("the new image should stop at the %1x expand limit, got %2 wide")
                            .arg(kExpandLimit)
                            .arg(shown.width())));

    // The fit-window scale is computed once during that load, so it has to be
    // computed with the flag already cleared. Switching to fit-window without a
    // click reads exactly that cached value - it stays unclamped if the clear
    // happens after updateMinScale() instead of before it.
    viewer->setFitMode(FIT_WINDOW);
    QTest::qWait(200);
    QVERIFY2(viewer->scaledRectR().height() <= kNextTinyH * kExpandLimit + 2,
             qPrintable(QStringLiteral("fit-window should honour the %1x expand limit too, got %2 high")
                            .arg(kExpandLimit)
                            .arg(viewer->scaledRectR().height())));

    // Leave the shared settings as the other slots expect to find them.
    settings->setKeepFitMode(false);
    settings->sendChangeNotification();
}

TG_BEHAVIOR_TEST_MAIN(DocumentViewFitModesTest)

#include "test_document_view_fit_modes.moc"
