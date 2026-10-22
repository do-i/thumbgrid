#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/mainwindow.h"
#include "gui/overlays/clickzoneoverlay.h"
#include "gui/viewers/imageviewerv2.h"

// With clickable edges on, a left click in the picture's left/right edge used
// to change file in code, swallowing the click ahead of any binding. The edges
// are now the bindings LMB_LeftEdge / LMB_RightEdge (prevImage / nextImage by
// default): another action can take them, modifiers make their own names, and
// an unbound edge is an ordinary click on the picture that doesn't light up.

class ClickableEdgesAreBindingsTest : public QObject {
    Q_OBJECT

private slots:
    void edgeClicksAreBindings();
};

void ClickableEdgesAreBindingsTest::edgeClicksAreBindings() {
    settings->setClickableEdges(true);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY(root.mkpath("gallery"));
    for(const char *name : {"a.png", "b.png", "c.png"})
        QVERIFY(tgtest::writeImage(root.filePath(QString("gallery/") + name), Qt::darkCyan));

    Core core;
    QVERIFY2(core.loadPath(root.filePath("gallery/b.png")), "Opening the image should succeed.");
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    window->resize(900, 500);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    QTRY_VERIFY(window->windowTitle().contains("b.png"));
    auto imageViewer = window->findChild<ImageViewerV2 *>();
    auto zones = window->findChild<ClickZoneOverlay *>();
    QVERIFY(imageViewer && zones);
    QTRY_VERIFY(imageViewer->isDisplaying());
    QTest::qWait(150);

    QWidget *viewport = imageViewer->viewport();
    const QPoint left(20, viewport->height() / 2);
    const QPoint right(viewport->width() - 20, viewport->height() / 2);
    QVERIFY(zones->leftZone().contains(left) && zones->rightZone().contains(right));
    const auto hover = [&](const QPoint &pos) {
        QMouseEvent move(QEvent::MouseMove, pos, viewport->mapToGlobal(pos), Qt::NoButton, Qt::NoButton,
                         Qt::NoModifier);
        QApplication::sendEvent(viewport, &move);
    };

    // The defaults: the edges are bindings for the previous / next file.
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "LMB_LeftEdge"), QStringLiteral("prevImage"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "LMB_RightEdge"), QStringLiteral("nextImage"));
    hover(left);
    QVERIFY2(zones->isHighlighted(), "A bound edge lights up under the cursor.");
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, left);
    QTRY_VERIFY(window->windowTitle().contains("a.png"));
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, right);
    QTRY_VERIFY(window->windowTitle().contains("b.png"));

    // Another action can take an edge, and a modifier makes its own name.
    actionManager->addShortcut(MODE_DOCUMENT, "LMB_RightEdge", "jumpToLast");
    actionManager->addShortcut(MODE_DOCUMENT, "Ctrl+LMB_LeftEdge", "jumpToFirst");
    core.loadPath(root.filePath("gallery/a.png"));
    QTRY_VERIFY(window->windowTitle().contains("a.png"));
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, right);
    QTRY_VERIFY2(window->windowTitle().contains("c.png"), "The rebound right edge should run jumpToLast.");
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::ControlModifier, left);
    QTRY_VERIFY2(window->windowTitle().contains("a.png"), "Ctrl+LMB_LeftEdge should run jumpToFirst.");

    // Unbound, an edge is an ordinary click: no file change, no highlight.
    actionManager->removeShortcut(MODE_DOCUMENT, "LMB_RightEdge");
    actionManager->removeShortcut(MODE_GLOBAL, "LMB_RightEdge");
    hover(right);
    QVERIFY2(!zones->isHighlighted(), "An unbound edge must not light up.");
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, right);
    QTest::qWait(200);
    QVERIFY2(window->windowTitle().contains("a.png"), "An unbound edge must not change file.");
}

TG_BEHAVIOR_TEST_MAIN(ClickableEdgesAreBindingsTest)

#include "test_clickable_edges_are_bindings.moc"
