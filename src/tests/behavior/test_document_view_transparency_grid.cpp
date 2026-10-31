#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <functional>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/viewers/imageviewerv2.h"
#include "gui/dialogs/settingsdialog.h"

#include <QPushButton>

class DocumentViewTransparencyGridTest : public QObject {
    Q_OBJECT

private slots:
    void transparentImageShowsCheckerGridInDocumentView();
    void checkerUsesTheConfiguredColors();
    void checkerColorsPersistAsOpaqueColors();
    void settingsDialogEditsTheCheckerColors();
};

static bool writeTransparentImage(const QString &path) {
    QImage image(1024, 768, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image.save(path, "PNG");
}

// opens a transparent image in the document view and checks the pixels of its checker
static void verifyCheckerInDocumentView(const std::function<bool(const QColor &)> &isCheckerPixel,
                                        const std::function<bool(const QImage &, const QRect &)> &bothColorsSeen) {
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

    auto checkerVisible = [viewer, &isCheckerPixel, &bothColorsSeen]() {
        QCoreApplication::processEvents();
        const QImage viewport = viewer->viewport()->grab().toImage();
        const QRect imageRect = viewer->scaledRectR().intersected(viewport.rect());
        if(viewport.isNull() || imageRect.isEmpty())
            return false;
        return isCheckerPixel(viewport.pixelColor(imageRect.center()))
               && (!bothColorsSeen || bothColorsSeen(viewport, imageRect));
    };

    QTest::qWait(250);
    QTRY_VERIFY2(checkerVisible(), "Transparent pixels should reveal the checker grid.");
}

void DocumentViewTransparencyGridTest::transparentImageShowsCheckerGridInDocumentView() {
    settings->setTransparencyGrid(true);
    settings->setTransparencyGridColors(Settings::defaultTransparencyGridColor1(),
                                        Settings::defaultTransparencyGridColor2());
    settings->sendChangeNotification();

    verifyCheckerInDocumentView([](const QColor &color) {
        const bool neutral = color.red() == color.green() && color.green() == color.blue();
        return neutral && (color.red() == 218 || color.red() == 242);
    }, nullptr);
}

void DocumentViewTransparencyGridTest::checkerUsesTheConfiguredColors() {
    const QColor color1(200, 40, 60);
    const QColor color2(30, 90, 210);
    settings->setTransparencyGrid(true);
    settings->setTransparencyGridColors(color1, color2);
    settings->sendChangeNotification();

    auto isConfigured = [color1, color2](const QColor &color) {
        return color.rgb() == color1.rgb() || color.rgb() == color2.rgb();
    };
    // a 16px tile at 1:1 or fit scale: a 32px square around the center holds both colors
    auto bothSeen = [color1, color2](const QImage &viewport, const QRect &imageRect) {
        bool seen1 = false, seen2 = false;
        const QRect probe = QRect(imageRect.center() - QPoint(16, 16), QSize(32, 32)).intersected(imageRect);
        for(int y = probe.top(); y <= probe.bottom(); y++) {
            for(int x = probe.left(); x <= probe.right(); x++) {
                const QRgb pixel = viewport.pixel(x, y);
                seen1 |= qRgb(qRed(pixel), qGreen(pixel), qBlue(pixel)) == color1.rgb();
                seen2 |= qRgb(qRed(pixel), qGreen(pixel), qBlue(pixel)) == color2.rgb();
            }
        }
        return seen1 && seen2;
    };
    verifyCheckerInDocumentView(isConfigured, bothSeen);
}

void DocumentViewTransparencyGridTest::checkerColorsPersistAsOpaqueColors() {
    settings->setTransparencyGridColors(QColor(10, 20, 30, 40), QColor(250, 240, 230));
    settings->sync();
    QCOMPARE(settings->transparencyGridColor1(), QColor(10, 20, 30));
    QCOMPARE(settings->transparencyGridColor2(), QColor(250, 240, 230));
    QCOMPARE(QSettings().value("Document/transparencyGridColor1").toString(), QStringLiteral("#0a141e"));

    // a preview only swaps the live colors
    settings->sync();
    settings->setTransparencyGridColors(Qt::black, Qt::white, false);
    QCOMPARE(settings->transparencyGridColor1(), QColor(Qt::black));
    QCOMPARE(QSettings().value("Document/transparencyGridColor1").toString(), QStringLiteral("#0a141e"));

    // invalid colors fall back to the defaults
    settings->setTransparencyGridColors(QColor(), QColor());
    QCOMPARE(settings->transparencyGridColor1(), Settings::defaultTransparencyGridColor1());
    QCOMPARE(settings->transparencyGridColor2(), Settings::defaultTransparencyGridColor2());
}

// Two swatches under the grid checkbox: Load defaults previews live without
// saving, Apply saves, and neither turns the theme into a custom one.
void DocumentViewTransparencyGridTest::settingsDialogEditsTheCheckerColors() {
    settings->setTransparencyGridColors(QColor(1, 2, 3), QColor(4, 5, 6));
    settings->sync();
    const int themeTid = settings->colorScheme().tid;

    SettingsDialog dialog;
    dialog.show();
    auto color1 = dialog.findChild<ColorSelectorButton *>("transparencyGridColor1Selector");
    auto color2 = dialog.findChild<ColorSelectorButton *>("transparencyGridColor2Selector");
    auto reset = dialog.findChild<QPushButton *>("transparencyGridColorsResetButton");
    QVERIFY(color1 && color2 && reset);
    QCOMPARE(color1->color(), QColor(1, 2, 3));
    QCOMPARE(color2->color(), QColor(4, 5, 6));

    const QString shots = qEnvironmentVariable("TG_SHOT_DIR");
    if(!shots.isEmpty())
        dialog.findChild<QWidget *>("displayGroup")->grab().save(shots + "/transparency-grid-colors.png");

    reset->click();
    QCOMPARE(color1->color(), Settings::defaultTransparencyGridColor1());
    QCOMPARE(color2->color(), Settings::defaultTransparencyGridColor2());
    QCOMPARE(settings->transparencyGridColor1(), Settings::defaultTransparencyGridColor1());
    QCOMPARE(QSettings().value("Document/transparencyGridColor1").toString(), QStringLiteral("#010203"));

    dialog.findChild<QPushButton *>("applyButton")->click();
    settings->sync();
    QCOMPARE(QSettings().value("Document/transparencyGridColor1").toString(),
             Settings::defaultTransparencyGridColor1().name());
    QCOMPARE(QSettings().value("Document/transparencyGridColor2").toString(),
             Settings::defaultTransparencyGridColor2().name());
    QCOMPARE(settings->colorScheme().tid, themeTid);
}

TG_BEHAVIOR_TEST_MAIN(DocumentViewTransparencyGridTest)

#include "test_document_view_transparency_grid.moc"
