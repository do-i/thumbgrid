#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core.h"
#include "gui/contextmenu.h"
#include "gui/mainwindow.h"
#include "gui/viewers/imageviewerv2.h"

// Dragging on the picture used to be fixed in code: left pans (or drags the
// file out when the picture fits), right zooms. Drags are now bindings -
// LMB_Drag, RMB_Drag, MiddleButton_Drag - of the drag actions dragPan, dragZoom
// and dragFileOut, behaving as before by default; a modified drag without a
// binding of its own uses the plain one, and an unbound drag does nothing.

class PictureDragsAreBindingsTest : public QObject {
    Q_OBJECT

private slots:
    void dragsAreBindings();
};

namespace {

// press at from, move to to in a few steps, release - with button held throughout
void drag(QWidget *target, Qt::MouseButton button, QPoint from, QPoint to,
          Qt::KeyboardModifiers mods = Qt::NoModifier) {
    const auto send = [&](QEvent::Type type, QPoint pos, Qt::MouseButtons held) {
        QMouseEvent event(type, pos, target->mapToGlobal(pos), button, held, mods);
        QApplication::sendEvent(target, &event);
    };
    send(QEvent::MouseButtonPress, from, button);
    for(int step = 1; step <= 4; step++)
        send(QEvent::MouseMove, from + (to - from) * step / 4, button);
    send(QEvent::MouseButtonRelease, to, Qt::NoButton);
    QCoreApplication::processEvents();
}

} // namespace

void PictureDragsAreBindingsTest::dragsAreBindings() {
    settings->setImageFitMode(FIT_ORIGINAL);
    settings->setFocusPointIn1to1Mode(FOCUS_CENTER);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY(root.mkpath("gallery"));
    {
        QImage big(3000, 2000, QImage::Format_RGB32);
        big.fill(Qt::darkCyan);
        QVERIFY(big.save(root.filePath("gallery/a-big.png"), "PNG"));
    }
    QVERIFY(tgtest::writeImage(root.filePath("gallery/b-small.png"), Qt::red));

    Core core;
    QVERIFY2(core.loadPath(root.filePath("gallery/a-big.png")), "Opening the image should succeed.");
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    window->resize(900, 500);
    QTRY_VERIFY(window->windowTitle().contains("a-big.png"));
    auto imageViewer = window->findChild<ImageViewerV2 *>();
    QVERIFY(imageViewer);
    QTRY_VERIFY(imageViewer->isDisplaying());
    QTest::qWait(150);
    QWidget *viewport = imageViewer->viewport();
    const QPoint center = viewport->rect().center();
    const auto contextMenuShown = [&]() {
        auto menu = window->findChild<ContextMenu *>();
        return menu && menu->isVisible();
    };

    // The defaults: left drag pans, right drag zooms (and its release opens no
    // context menu), and Ctrl+left drag still pans through LMB_Drag.
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "LMB_Drag"), QStringLiteral("dragPan"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "RMB_Drag"), QStringLiteral("dragZoom"));
    QRect rect = imageViewer->scaledRectR();
    drag(viewport, Qt::LeftButton, center, center + QPoint(-80, -60));
    QTRY_VERIFY2(imageViewer->scaledRectR().topLeft() != rect.topLeft(), "Left drag should pan.");
    rect = imageViewer->scaledRectR();
    drag(viewport, Qt::LeftButton, center, center + QPoint(80, 60), Qt::ControlModifier);
    QTRY_VERIFY2(imageViewer->scaledRectR().topLeft() != rect.topLeft(), "Ctrl+left drag should pan too.");
    float scale = imageViewer->currentScale();
    drag(viewport, Qt::RightButton, center, center + QPoint(0, -80));
    QTRY_VERIFY2(imageViewer->currentScale() != scale, "Right drag should zoom.");
    QTest::qWait(100);
    QVERIFY2(!contextMenuShown(), "Ending a right drag must not open the context menu.");

    // Rebound, the right drag pans instead of zooming.
    actionManager->addShortcut(MODE_DOCUMENT, "RMB_Drag", "dragPan");
    imageViewer->setFitOriginal();
    QTest::qWait(150);
    scale = imageViewer->currentScale();
    rect = imageViewer->scaledRectR();
    drag(viewport, Qt::RightButton, center, center + QPoint(-80, -60));
    QTRY_VERIFY2(imageViewer->scaledRectR().topLeft() != rect.topLeft(), "A right drag bound to dragPan should pan.");
    QCOMPARE(imageViewer->currentScale(), scale);
    QTest::qWait(100);
    QVERIFY(!contextMenuShown());

    // Unbound, a left drag does nothing.
    actionManager->removeShortcut(MODE_DOCUMENT, "LMB_Drag");
    actionManager->removeShortcut(MODE_GLOBAL, "LMB_Drag");
    rect = imageViewer->scaledRectR();
    drag(viewport, Qt::LeftButton, center, center + QPoint(-80, -60));
    QTest::qWait(150);
    QCOMPARE(imageViewer->scaledRectR(), rect);

    // dragFileOut drags the file out even where the picture could pan, and
    // dragPan does so when the picture fits. (The real drag-and-drop is cut
    // off here; it would block, swallowing the rest of the drag.)
    disconnect(imageViewer, &ImageViewerV2::draggedOut, nullptr, nullptr);
    QSignalSpy draggedOut(imageViewer, &ImageViewerV2::draggedOut);
    actionManager->addShortcut(MODE_DOCUMENT, "MiddleButton_Drag", "dragFileOut");
    rect = imageViewer->scaledRectR();
    drag(viewport, Qt::MiddleButton, center, center + QPoint(-80, -60));
    QVERIFY2(!draggedOut.isEmpty(), "A middle drag bound to dragFileOut should drag the file out.");
    QCOMPARE(imageViewer->scaledRectR(), rect);
    draggedOut.clear();
    core.loadPath(root.filePath("gallery/b-small.png"));
    QTRY_VERIFY(window->windowTitle().contains("b-small.png"));
    QTRY_VERIFY(imageViewer->isDisplaying());
    QTest::qWait(150);
    drag(viewport, Qt::RightButton, center, center + QPoint(-80, -60));
    QVERIFY2(!draggedOut.isEmpty(), "dragPan on a fitting picture should drag the file out.");
}

TG_BEHAVIOR_TEST_MAIN(PictureDragsAreBindingsTest)

#include "test_picture_drags_are_bindings.moc"
