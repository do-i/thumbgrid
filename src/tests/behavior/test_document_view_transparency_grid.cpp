#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/viewers/imageviewerv2.h"

class DocumentViewTransparencyGridTest : public QObject {
    Q_OBJECT

private slots:
    void transparentImageShowsCheckerGridInDocumentView();
};

static bool writeTransparentImage(const QString &path) {
    QImage image(1024, 768, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image.save(path, "PNG");
}

static bool isTransparencyGridPixel(const QColor &color) {
    const bool neutral = color.red() == color.green() && color.green() == color.blue();
    return neutral && (color.red() == 218 || color.red() == 242);
}

void DocumentViewTransparencyGridTest::transparentImageShowsCheckerGridInDocumentView() {
    settings->setTransparencyGrid(true);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");

    const QString galleryPath = root.filePath("gallery");
    QVERIFY2(writeTransparentImage(root.filePath("gallery/transparent.png")),
             "Transparent image should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");
    QTRY_COMPARE(grid->itemCount(), 2);

    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    auto viewer = window->findChild<ImageViewerV2 *>();
    QVERIFY2(viewer != nullptr, "The document image viewer should exist.");
    QTRY_VERIFY2(viewer->isVisible(), "The document image viewer should be visible.");
    QTRY_VERIFY2(viewer->isDisplaying(), "The transparent image should be displayed.");

    auto checkerVisible = [viewer]() {
        QCoreApplication::processEvents();
        const QImage viewport = viewer->viewport()->grab().toImage();
        const QRect imageRect = viewer->scaledRectR().intersected(viewport.rect());
        if(viewport.isNull() || imageRect.isEmpty())
            return false;
        return isTransparencyGridPixel(viewport.pixelColor(imageRect.center()));
    };

    QTest::qWait(250);
    QTRY_VERIFY2(checkerVisible(), "Transparent pixels should reveal the checker grid.");
}

TG_BEHAVIOR_TEST_MAIN(DocumentViewTransparencyGridTest)

#include "test_document_view_transparency_grid.moc"
