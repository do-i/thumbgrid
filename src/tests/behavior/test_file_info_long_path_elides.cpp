#include "support/thumbgrid_test_support.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QTemporaryDir>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class FileInfoLongPathElidesTest : public QObject {
    Q_OBJECT

private slots:
    void longPathElidesWithTooltipAndCopyAction();
};

namespace {

// Every General/EXIF row is an EntryInfoItem with two QLabel children (name,
// then value) added in that order by EntryInfoItem's constructor.
EntryInfoItem *rowByName(QWidget *root, const QString &rowName) {
    for(EntryInfoItem *item : root->findChildren<EntryInfoItem *>()) {
        const QList<QLabel *> labels = item->findChildren<QLabel *>();
        if(labels.size() >= 2 && labels.at(0)->text() == rowName)
            return item;
    }
    return nullptr;
}

} // namespace

void FileInfoLongPathElidesTest::longPathElidesWithTooltipAndCopyAction() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    // Long enough that no reasonable dialog width shows it unelided.
    const QString longName = QString(200, QLatin1Char('x')) + QStringLiteral(".png");
    const QString longPath = root.filePath("gallery/" + longName);
    QVERIFY2(tgtest::writeImage(longPath, Qt::red), "Long-named image should be written.");
    const QString absoluteLongPath = QFileInfo(longPath).absoluteFilePath();

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), then the long-named file (1).
    QTRY_COMPARE(grid->itemCount(), 2);
    const int longNamedFile = 1;

    grid->select(longNamedFile);
    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable.");

    auto *dialog = window->findChild<FileInfoDialog *>();
    QTRY_VERIFY2(dialog != nullptr, "The File info popup should exist.");
    QTRY_VERIFY2(dialog->isVisible(), "The File info popup should be visible.");

    EntryInfoItem *pathRow = nullptr;
    QTRY_VERIFY2((pathRow = rowByName(dialog, "Path")) != nullptr, "The Path row should exist.");
    auto *valueLabel = pathRow->findChildren<QLabel *>().at(1);
    QVERIFY2(valueLabel != nullptr, "The Path row should have a value label.");

    // --- Elided display text, full path in the tooltip. ---
    QTRY_VERIFY2(valueLabel->text() != absoluteLongPath,
                 "A path this long should be elided rather than shown in full.");
    QVERIFY2(valueLabel->text().contains(QChar(0x2026)),
             "The elided text should contain an ellipsis character.");
    QCOMPARE(valueLabel->toolTip(), absoluteLongPath);

    // --- Right-click "Copy" always copies the full, un-elided path. ---
    QApplication::clipboard()->clear();
    QContextMenuEvent contextEvent(QContextMenuEvent::Mouse, QPoint(5, 5),
                                    valueLabel->mapToGlobal(QPoint(5, 5)));
    QCoreApplication::sendEvent(valueLabel, &contextEvent);

    auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY2(menu != nullptr, "Right-clicking the value label should open a context menu.");
    QVERIFY2(!menu->actions().isEmpty(), "The context menu should have a Copy action.");
    menu->actions().first()->trigger();
    menu->close();

    QCOMPARE(QApplication::clipboard()->text(), absoluteLongPath);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoLongPathElidesTest)

#include "test_file_info_long_path_elides.moc"
