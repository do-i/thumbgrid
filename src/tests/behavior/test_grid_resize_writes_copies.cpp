// "Resize..." in the grid context menu opens the resize dialog for the whole
// selection and writes a resized copy of every image next to its original,
// in the background when more than one image is selected.
#include "support/thumbgrid_test_support.h"

#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QImageReader>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>

#include "core.h"
#include "components/fileoperationscontroller.h"
#include "gui/customwidgets/contextmenuitem.h"
#include "gui/dialogs/resizedialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/folderview/gridcontextmenu.h"
#include "gui/mainwindow.h"

class GridResizeWritesCopiesTest : public QObject {
    Q_OBJECT

private slots:
    void resizingTwoImagesFromTheGridWritesTwoCopies();
};

namespace {

bool writeSizedImage(const QString &path, QSize size) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(QColor(200, 120, 80));
    return image.save(path, "PNG");
}

QByteArray fileHash(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return {};
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

} // namespace

void GridResizeWritesCopiesTest::resizingTwoImagesFromTheGridWritesTwoCopies() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY(root.mkpath("gallery"));
    const QString galleryPath = root.filePath("gallery");
    const QString tallPath = root.filePath("gallery/tall.png");
    const QString widePath = root.filePath("gallery/wide.png");
    // different aspect ratios: one percentage must apply to each image's own size
    QVERIFY(writeSizedImage(tallPath, QSize(100, 300)));
    QVERIFY(writeSizedImage(widePath, QSize(400, 200)));
    const QByteArray tallBefore = fileHash(tallPath);
    const QByteArray wideBefore = fileHash(widePath);

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");
    // Parent ".." (0), tall.png (1), wide.png (2).
    QTRY_COMPARE(grid->itemCount(), 3);
    grid->select(QList<int>{1, 2});

    auto *fileOps = core.findChild<FileOperationsController *>();
    QVERIFY2(fileOps != nullptr, "Core should own a FileOperationsController.");
    QSignalSpy finished(fileOps, &FileOperationsController::resizeFinished);

    QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(0, 0), grid->mapToGlobal(QPoint(0, 0)));
    QApplication::sendEvent(grid->viewport(), &event);
    auto *menu = grid->findChild<GridContextMenu *>();
    QVERIFY2(menu != nullptr, "The grid context menu should exist.");
    auto *resizeItem = menu->findChild<ContextMenuItem *>("menuResize");
    QVERIFY2(resizeItem != nullptr, "The grid menu should have a Resize item.");
    QVERIFY2(resizeItem->isEnabled(), "Resize should be enabled for two images.");

    // The dialog runs a nested event loop; drive it from inside that loop.
    QString dialogTitle;
    bool dialogDriven = false;
    QTimer::singleShot(0, this, [&]() {
        QTRY_VERIFY(qobject_cast<ResizeDialog *>(QApplication::activeModalWidget()) != nullptr);
        auto *dialog = qobject_cast<ResizeDialog *>(QApplication::activeModalWidget());
        dialogTitle = dialog->windowTitle();
        dialog->findChild<QRadioButton *>("byPercentage")->setChecked(true);
        dialog->findChild<QDoubleSpinBox *>("percent")->setValue(50.0);
        QTest::mouseClick(dialog->findChild<QPushButton *>("okButton"), Qt::LeftButton);
        dialogDriven = true;
    });
    QTest::mousePress(resizeItem, Qt::LeftButton);
    QTest::mouseRelease(resizeItem, Qt::LeftButton);

    QTRY_VERIFY2(dialogDriven, "The resize dialog should have opened and been accepted.");
    QVERIFY2(dialogTitle.contains("2"), qPrintable("Dialog title should name the image count: " + dialogTitle));

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
    const QList<QVariant> counts = finished.takeFirst();
    QCOMPARE(counts.at(0).toInt(), 2); // resized
    QCOMPARE(counts.at(1).toInt(), 0); // skipped
    QCOMPARE(counts.at(2).toInt(), 0); // failed

    const QString tallCopy = root.filePath("gallery/tall_50x150.png");
    const QString wideCopy = root.filePath("gallery/wide_200x100.png");
    QVERIFY2(QFile::exists(tallCopy), "tall.png should get a 50% copy.");
    QVERIFY2(QFile::exists(wideCopy), "wide.png should get a 50% copy.");
    QCOMPARE(QImageReader(tallCopy).size(), QSize(50, 150));
    QCOMPARE(QImageReader(wideCopy).size(), QSize(200, 100));

    QVERIFY2(fileHash(tallPath) == tallBefore, "The original tall.png must be untouched.");
    QVERIFY2(fileHash(widePath) == wideBefore, "The original wide.png must be untouched.");
    QCOMPARE(QDir(galleryPath).entryList(QDir::Files).count(), 4);
    QVERIFY2(!fileOps->isResizeRunning(), "The worker should be cleared once it reports.");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(GridResizeWritesCopiesTest)

#include "test_grid_resize_writes_copies.moc"
