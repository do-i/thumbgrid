#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QSpontaneKeyEvent>
#include <QWheelEvent>

#include "core.h"
#include "gui/contextmenu.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/viewers/imageviewerv2.h"
#include "gui/viewers/textviewer.h"
#include "gui/viewers/viewerwidget.h"

// Arrow keys scroll text / pan a zoomed image (Ctrl+arrow in fine steps) through
// the configurable scroll actions, and
// scrolling stops at the end instead of jumping to the neighbouring file -
// reaching the bottom of a long file used to silently open the next one. The
// wheel's text scrolling is a binding too (scrollOrPrevImage/scrollOrNextImage),
// and Ctrl+wheel runs the zoom actions, which resize the text. With image
// scrolling set to trackpad and wheel, the same actions pan a tall image. A
// wheel turned with the right button held is RMB+WheelUp/RMB+WheelDown.

class ViewerArrowKeysAndWheelScrollTest : public QObject {
    Q_OBJECT

private slots:
    void arrowKeysAndWheelScrollWithinTheCurrentFile();
};

namespace {

// Delivers a spontaneous wheel notch at the widget's center, the way QTest
// sends keys. Synthetic sendEvent() wheel events don't propagate to parents,
// which is exactly the path that used to turn an unscrollable notch into
// "next file".
void wheelNotch(QWidget *target, int angleY, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                Qt::MouseButtons buttons = Qt::NoButton) {
    const QPointF local = QRectF(target->rect()).center();
    QWheelEvent event(local, target->mapToGlobal(local), QPoint(0, 0), QPoint(0, angleY),
                      buttons, modifiers, Qt::NoScrollPhase, false);
    QSpontaneKeyEvent::setSpontaneous(&event);
    qApp->notify(target, &event);
    QCoreApplication::processEvents();
}

// Presses a key the way the app resolves it: through the active shortcut
// mapping. (QTest key events carry no native scancode, which ShortcutBuilder
// needs to name a key, so they would never reach a binding.)
void press(const char *key) {
    actionManager->invokeAction(actionManager->actionForShortcut(MODE_DOCUMENT, QString::fromLatin1(key)));
    QCoreApplication::processEvents();
}

bool writeText(const QString &path, int lines) {
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly))
        return false;
    for(int i = 0; i < lines; ++i)
        file.write(QByteArray("line ") + QByteArray::number(i) + "\n");
    return true;
}

} // namespace

void ViewerArrowKeysAndWheelScrollTest::arrowKeysAndWheelScrollWithinTheCurrentFile() {
    settings->setShowOtherFileTypes(true);
    settings->setImageFitMode(FIT_ORIGINAL);
    settings->setFocusPointIn1to1Mode(FOCUS_CENTER);
    settings->setImageScrolling(SCROLL_BY_TRACKPAD_AND_WHEEL);
    settings->setTrackpadDetection(false);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    {
        QImage big(3000, 2000, QImage::Format_RGB32);
        big.fill(Qt::darkCyan);
        QVERIFY(big.save(root.filePath("gallery/a-big.png"), "PNG"));
    }
    QVERIFY(writeText(root.filePath("gallery/b-long.txt"), 300));
    QVERIFY(writeText(root.filePath("gallery/c-short.txt"), 2));
    QVERIFY(tgtest::writeImage(root.filePath("gallery/d-small.png"), Qt::red));

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    window->resize(900, 500);
    QTest::qWait(150);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");
    QTRY_COMPARE(grid->itemCount(), 5);
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    QTRY_VERIFY(window->windowTitle().contains("a-big.png"));

    auto imageViewer = window->findChild<ImageViewerV2 *>();
    QVERIFY(imageViewer);

    // The arrows are ordinary, rebindable actions in the default mapping.
    const QStringList keys{"Up", "Down", "Left", "Right", "Ctrl+Up", "Ctrl+Down", "Ctrl+Left", "Ctrl+Right"};
    const QStringList actions{"scrollUp", "scrollDown", "scrollLeft", "scrollRight",
                              "scrollUpFine", "scrollDownFine", "scrollLeftFine", "scrollRightFine"};
    for(int i = 0; i < keys.size(); ++i)
        QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, keys[i]), actions[i]);
    QTRY_VERIFY(imageViewer->isDisplaying());
    QTest::qWait(150);
    QScrollBar *hs = imageViewer->horizontalScrollBar();
    QScrollBar *vs = imageViewer->verticalScrollBar();
    const QRect vport = imageViewer->viewport()->rect();
    QVERIFY2(imageViewer->scaledRectR().width() > vport.width() &&
             imageViewer->scaledRectR().height() > vport.height(),
             "The 1:1 image should overflow both axes.");

    // Down pans the zoomed image instead of doing nothing.
    const int v0 = vs->value();
    press("Down");
    QTRY_VERIFY(vs->value() > v0);

    // Ctrl+Right pans by one fine, unanimated step.
    const int h0 = hs->value();
    press("Ctrl+Right");
    QCOMPARE(hs->value(), h0 + 24);

    // Right pans, then stops at the edge rather than opening the next file.
    for(int i = 0; i < 20; ++i)
        press("Right");
    // the image's right edge lands on the view's right edge (scrollbars span a
    // much larger scene, so their maximum isn't the image edge)
    QTRY_VERIFY(qAbs(imageViewer->scaledRectR().right() - imageViewer->width()) <= 2);
    QTest::qWait(300);
    const int hEdge = hs->value();
    press("Right");
    QTest::qWait(300);
    QCOMPARE(hs->value(), hEdge);
    QVERIFY2(window->windowTitle().contains("a-big.png"),
             "Right at the image edge should not change file.");

    // The wheel pans vertically and stops at the bottom edge too.
    for(int i = 0; i < 30; ++i)
        wheelNotch(imageViewer->viewport(), -120);
    QTRY_VERIFY(qAbs(imageViewer->scaledRectR().bottom() - imageViewer->height()) <= 2);
    QTest::qWait(300);
    const int vEdge = vs->value();
    wheelNotch(imageViewer->viewport(), -120);
    QTest::qWait(300);
    QCOMPARE(vs->value(), vEdge);
    QVERIFY2(window->windowTitle().contains("a-big.png"),
             "Wheel at the image bottom should not change file.");

    // Long text file: the wheel stops at the end of the file.
    QMetaObject::invokeMethod(&core, "nextImage");
    QTRY_VERIFY(window->windowTitle().contains("b-long.txt"));
    auto textViewer = window->findChild<TextViewer *>();
    QVERIFY(textViewer);
    QTRY_VERIFY(textViewer->isVisible());
    auto textEdit = textViewer->findChild<QPlainTextEdit *>();
    QVERIFY(textEdit);
    QScrollBar *tvs = textEdit->verticalScrollBar();
    QTRY_VERIFY(tvs->maximum() > 10);
    for(int i = 0; i < 150; ++i)
        wheelNotch(textEdit->viewport(), -120);
    QCOMPARE(tvs->value(), tvs->maximum());
    QTest::qWait(100);
    QVERIFY2(window->windowTitle().contains("b-long.txt"),
             "Wheel at the end of a text file should not change file.");

    // Up/Down scroll the text by wheel-lines, Ctrl by a single line.
    const int t0 = tvs->value();
    press("Up");
    QCOMPARE(tvs->value(), t0 - QApplication::wheelScrollLines());
    press("Ctrl+Up");
    QCOMPARE(tvs->value(), t0 - QApplication::wheelScrollLines() - 1);
    press("Ctrl+Down");
    QCOMPARE(tvs->value(), t0 - QApplication::wheelScrollLines());
    QVERIFY(window->windowTitle().contains("b-long.txt"));

    // A text file that fits on screen still lets the wheel change file.
    QMetaObject::invokeMethod(&core, "nextImage");
    QTRY_VERIFY(window->windowTitle().contains("c-short.txt"));
    QTRY_COMPARE(tvs->maximum(), 0);
    // (which neighbour depends on the wheel-direction migration a fresh test
    // profile goes through, so only check that the file changed)
    wheelNotch(textEdit->viewport(), -120);
    QTRY_VERIFY2(!window->windowTitle().contains("c-short.txt"),
                 "Wheel over a text file that fits should change file.");

    // Nothing to pan on a fitting image: Left falls back to the previous file.
    core.loadPath(root.filePath("gallery/d-small.png"));
    QTRY_VERIFY(window->windowTitle().contains("d-small.png"));
    QTRY_VERIFY(imageViewer->isDisplaying());
    press("Left");
    QTRY_VERIFY2(window->windowTitle().contains("c-short.txt"),
                 "Left on a fitting image should go to the previous file.");

    // Up on a window-fitted image has nothing to scroll, so it switches to
    // fit-width (this used to be a special case ahead of the bindings).
    core.loadPath(root.filePath("gallery/d-small.png"));
    QTRY_VERIFY(window->windowTitle().contains("d-small.png"));
    QTRY_VERIFY(imageViewer->isDisplaying());
    imageViewer->setFitWindow();
    QCOMPARE(imageViewer->fitMode(), FIT_WINDOW);
    press("Up");
    QCOMPARE(imageViewer->fitMode(), FIT_WIDTH);

    // An unbound Left really does nothing now (it used to fall back to the
    // previous file in code).
    auto viewerWidget = window->findChild<ViewerWidget *>();
    QVERIFY(viewerWidget);
    actionManager->removeAllShortcuts("scrollLeft");
    QTest::keyClick(viewerWidget, Qt::Key_Left);
    QTest::qWait(200);
    QVERIFY2(window->windowTitle().contains("d-small.png"), "An unbound Left must not change file.");

    // The wheel's default bindings are the scroll-or-change-file actions (which
    // wheel direction gets which depends on the fresh profile's migration).
    const QStringList wheelActions{"scrollOrPrevImage", "scrollOrNextImage"};
    const QString wheelDownAction = actionManager->actionForShortcut(MODE_DOCUMENT, "WheelDown");
    QVERIFY(wheelActions.contains(wheelDownAction));
    QVERIFY(wheelActions.contains(actionManager->actionForShortcut(MODE_DOCUMENT, "WheelUp")));

    // Run from a key, the action scrolls a long text file by wheel-lines...
    core.loadPath(root.filePath("gallery/b-long.txt"));
    QTRY_VERIFY(window->windowTitle().contains("b-long.txt"));
    QTRY_VERIFY(tvs->maximum() > 10);
    QCOMPARE(tvs->value(), 0);
    actionManager->invokeAction("scrollOrNextImage");
    QCOMPARE(tvs->value(), QApplication::wheelScrollLines());
    // ...and at the end it stays put rather than opening the next file.
    tvs->setValue(tvs->maximum());
    actionManager->invokeAction("scrollOrNextImage");
    QTest::qWait(100);
    QCOMPARE(tvs->value(), tvs->maximum());
    QVERIFY(window->windowTitle().contains("b-long.txt"));

    // Bound to another action, the wheel runs it even while the text can scroll
    // (the text view used to scroll it in code, ahead of any binding).
    tvs->setValue(0);
    actionManager->addShortcut(MODE_DOCUMENT, "WheelDown", "nextImage");
    wheelNotch(textEdit->viewport(), -120);
    QTRY_VERIFY2(window->windowTitle().contains("c-short.txt"),
                 "A wheel bound to nextImage should change file over a long text file.");

    // Unbound, the wheel still scrolls a long text file but changes nothing else.
    actionManager->removeShortcut(MODE_DOCUMENT, "WheelDown");
    QVERIFY(actionManager->actionForShortcut(MODE_DOCUMENT, "WheelDown").isEmpty());
    core.loadPath(root.filePath("gallery/b-long.txt"));
    QTRY_VERIFY(window->windowTitle().contains("b-long.txt"));
    QTRY_VERIFY(tvs->maximum() > 10);
    wheelNotch(textEdit->viewport(), -120);
    QVERIFY2(tvs->value() > 0, "An unbound wheel should still scroll a long text file.");
    core.loadPath(root.filePath("gallery/c-short.txt"));
    QTRY_VERIFY(window->windowTitle().contains("c-short.txt"));
    QTRY_COMPARE(tvs->maximum(), 0);
    wheelNotch(textEdit->viewport(), -120);
    QTest::qWait(200);
    QVERIFY2(window->windowTitle().contains("c-short.txt"), "An unbound wheel must not change file.");
    actionManager->addShortcut(MODE_DOCUMENT, "WheelDown", wheelDownAction);

    // The zoom actions resize the text: a zoom key...
    const qreal size0 = textEdit->font().pointSizeF();
    actionManager->invokeAction("zoomIn");
    QCOMPARE(textEdit->font().pointSizeF(), size0 + 1);
    actionManager->invokeAction("zoomOutCursor");
    QCOMPARE(textEdit->font().pointSizeF(), size0);
    // ...and Ctrl+wheel, through its binding: bound to zoom out, Ctrl+WheelUp
    // shrinks the text (Qt's own handling would grow it).
    actionManager->addShortcut(MODE_DOCUMENT, "Ctrl+WheelUp", "zoomOutCursor");
    wheelNotch(textEdit->viewport(), 120, Qt::ControlModifier);
    QCOMPARE(textEdit->font().pointSizeF(), size0 - 1);
    // Bound to another action, Ctrl+wheel runs it instead of zooming.
    actionManager->addShortcut(MODE_DOCUMENT, "Ctrl+WheelUp", "nextImage");
    wheelNotch(textEdit->viewport(), 120, Qt::ControlModifier);
    QTRY_VERIFY2(window->windowTitle().contains("d-small.png"),
                 "A Ctrl+wheel bound to nextImage should change file over a text file.");
    // Unbound, it keeps Qt's own text zoom.
    actionManager->removeShortcut(MODE_DOCUMENT, "Ctrl+WheelUp");
    QVERIFY(actionManager->actionForShortcut(MODE_DOCUMENT, "Ctrl+WheelUp").isEmpty());
    core.loadPath(root.filePath("gallery/c-short.txt"));
    QTRY_VERIFY(window->windowTitle().contains("c-short.txt"));
    const qreal size1 = textEdit->font().pointSizeF();
    wheelNotch(textEdit->viewport(), 120, Qt::ControlModifier);
    QCOMPARE(textEdit->font().pointSizeF(), size1 + 1);
    QVERIFY(window->windowTitle().contains("c-short.txt"));

    // A tall image pans on the wheel only while the wheel is bound to the scroll
    // actions; bound to another action, that action runs (it used to be
    // swallowed by the image viewer while the image could pan).
    core.loadPath(root.filePath("gallery/a-big.png"));
    QTRY_VERIFY(window->windowTitle().contains("a-big.png"));
    QTRY_VERIFY(imageViewer->isDisplaying());
    QTest::qWait(150);
    QVERIFY(imageViewer->scaledRectR().height() > imageViewer->viewport()->height());
    actionManager->addShortcut(MODE_DOCUMENT, "WheelUp", "nextImage");
    const int top0 = imageViewer->scaledRectR().top();
    wheelNotch(imageViewer->viewport(), 120);
    QTRY_VERIFY2(window->windowTitle().contains("b-long.txt"),
                 "A wheel bound to nextImage should change file over a tall image.");
    actionManager->addShortcut(MODE_DOCUMENT, "WheelUp",
                               wheelDownAction == "scrollOrNextImage" ? "scrollOrPrevImage" : "scrollOrNextImage");

    // Run from a key, the action pans the tall image instead of changing file...
    core.loadPath(root.filePath("gallery/a-big.png"));
    QTRY_VERIFY(window->windowTitle().contains("a-big.png"));
    QTRY_VERIFY(imageViewer->isDisplaying());
    QTest::qWait(150);
    QTRY_COMPARE(imageViewer->scaledRectR().top(), top0);
    actionManager->invokeAction("scrollOrNextImage");
    QTRY_VERIFY(imageViewer->scaledRectR().top() < top0);
    QVERIFY(window->windowTitle().contains("a-big.png"));
    // ...but with the wheel set to change file, it changes file.
    settings->setImageScrolling(SCROLL_BY_TRACKPAD);
    settings->sendChangeNotification();
    actionManager->invokeAction("scrollOrNextImage");
    QTRY_VERIFY2(window->windowTitle().contains("b-long.txt"),
                 "With trackpad-only scrolling the action should change file.");
    settings->setImageScrolling(SCROLL_BY_TRACKPAD_AND_WHEEL);
    settings->sendChangeNotification();

    // A wheel turned with the right button held runs RMB+WheelUp/RMB+WheelDown,
    // zoom at the cursor by default (which way depends on the fresh profile's
    // wheel migration), and the release that ends it opens no context menu.
    QVERIFY(QStringList({"zoomInCursor", "zoomOutCursor"})
                .contains(actionManager->actionForShortcut(MODE_DOCUMENT, "RMB+WheelUp")));
    core.loadPath(root.filePath("gallery/a-big.png"));
    QTRY_VERIFY(window->windowTitle().contains("a-big.png"));
    QTRY_VERIFY(imageViewer->isDisplaying());
    QTest::qWait(150);
    QWidget *viewport = imageViewer->viewport();
    const auto rightButtonWheel = [&]() {
        QTest::mousePress(viewport, Qt::RightButton, Qt::NoModifier, viewport->rect().center());
        wheelNotch(viewport, 120, Qt::NoModifier, Qt::RightButton);
        QTest::mouseRelease(viewport, Qt::RightButton, Qt::NoModifier, viewport->rect().center());
        QTest::qWait(150);
    };
    const auto contextMenuShown = [&]() {
        auto menu = window->findChild<ContextMenu *>();
        return menu && menu->isVisible();
    };
    // (a plain right click does open it)
    QTest::mouseClick(viewport, Qt::RightButton, Qt::NoModifier, viewport->rect().center());
    QTRY_VERIFY(contextMenuShown());
    viewerWidget->hideContextMenu();
    QTRY_VERIFY(!contextMenuShown());
    const float scale0 = imageViewer->currentScale();
    rightButtonWheel();
    QTRY_VERIFY2(imageViewer->currentScale() != scale0, "Right button + wheel should zoom by default.");
    QVERIFY2(!contextMenuShown(), "Ending a right button + wheel zoom must not open the context menu.");
    // Bound to another action, it runs that instead.
    actionManager->addShortcut(MODE_DOCUMENT, "RMB+WheelUp", "nextImage");
    rightButtonWheel();
    QTRY_VERIFY2(window->windowTitle().contains("b-long.txt"),
                 "Right button + wheel bound to nextImage should change file.");
    QVERIFY(!contextMenuShown());
    // Unbound, it does nothing.
    actionManager->removeShortcut(MODE_DOCUMENT, "RMB+WheelUp");
    actionManager->removeShortcut(MODE_GLOBAL, "RMB+WheelUp");
    core.loadPath(root.filePath("gallery/a-big.png"));
    QTRY_VERIFY(window->windowTitle().contains("a-big.png"));
    QTRY_VERIFY(imageViewer->isDisplaying());
    QTest::qWait(150);
    const float scale1 = imageViewer->currentScale();
    rightButtonWheel();
    QCOMPARE(imageViewer->currentScale(), scale1);
    QVERIFY(window->windowTitle().contains("a-big.png"));
    QVERIFY(!contextMenuShown());
}

TG_BEHAVIOR_TEST_MAIN(ViewerArrowKeysAndWheelScrollTest)

#include "test_viewer_arrow_keys_and_wheel_scroll.moc"
