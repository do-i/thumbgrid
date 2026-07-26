// Reported annoyance: open a small image from the grid and the window shrinks to
// fit it (that part is "Automatic window resize" working as intended), but going
// back to the grid leaves it at that tiny size. Nothing ever grew the window back
// again, so the grid stayed thumbnail-sized for the rest of the session - and the
// 30 ms geometry timer persisted the shrunken size, so it survived a restart too.
//
// The grid's own geometry is now remembered on the way into the document view and
// put back on the way out.

#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/mainwindow.h"
#include "settings.h"

class GridWindowSizeSurvivesADocumentVisitTest : public QObject {
    Q_OBJECT

private slots:
    void returningToTheGridRestoresTheWindowSizeTheGridHad();
};

void GridWindowSizeSurvivesADocumentVisitTest::returningToTheGridRestoresTheWindowSizeTheGridHad() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    // tgtest::writeImage() writes 32x24 - far smaller than any sane window, which
    // is exactly the case that used to strand the grid.
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/small.png"), Qt::red), "Image should be written.");

    // The whole behaviour is gated on this being on; with it off nothing resizes
    // and there is no bug to reproduce.
    settings->setAutoResizeWindow(true);

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QTRY_COMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    // Give the grid a deliberate, roomy size to stand in for "the size the user
    // had it at". Wait for it to actually land - setGeometry on a freshly shown
    // window is not synchronous everywhere.
    const QRect gridGeometry(80, 80, 900, 700);
    window->setGeometry(gridGeometry);
    QTRY_COMPARE(window->size(), gridGeometry.size());

    // Into the document view, on a 32x24 image: this is what shrinks the window.
    QVERIFY2(core.loadPath(root.filePath("gallery/small.png")), "Opening the image should succeed.");
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    QTRY_VERIFY2(window->width() < gridGeometry.width(),
                 "precondition: auto-resize must actually shrink the window for a tiny image");

    // ...and back out again.
    actionManager->invokeAction(QStringLiteral("toggleFolderView"));
    QTRY_COMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    QTRY_COMPARE(window->size(), gridGeometry.size());
}

TG_BEHAVIOR_TEST_MAIN(GridWindowSizeSurvivesADocumentVisitTest)

#include "test_grid_window_size_survives_a_document_visit.moc"
