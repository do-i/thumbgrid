#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QTemporaryDir>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class FileInfoPopupOpensInBothViewsTest : public QObject {
    Q_OBJECT

private slots:
    void popupOpensInGridViewAndDocumentViewAndToggles();
};

namespace {

// Every General/EXIF row is an EntryInfoItem with two QLabel children (name,
// then value) added in that order by EntryInfoItem's constructor. Rows for
// both tabs are children of the dialog regardless of which tab is current, so
// searching the whole dialog and matching on the name label finds any row.
// The value label elides long text and moves the full string to its tooltip
// (EntryInfoItem::updateElidedText), so prefer the tooltip when set.
QString rowValueByName(QWidget *root, const QString &rowName) {
    for(EntryInfoItem *item : root->findChildren<EntryInfoItem *>()) {
        const QList<QLabel *> labels = item->findChildren<QLabel *>();
        if(labels.size() >= 2 && labels.at(0)->text() == rowName) {
            const QString tip = labels.at(1)->toolTip();
            return tip.isEmpty() ? labels.at(1)->text() : tip;
        }
    }
    return QString();
}

} // namespace

void FileInfoPopupOpensInBothViewsTest::popupOpensInGridViewAndDocumentViewAndToggles() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString aPath = root.filePath("gallery/a.png");
    const QString bPath = root.filePath("gallery/b.png");
    QVERIFY2(tgtest::writeImage(aPath, Qt::red), "Image a should be written.");
    QVERIFY2(tgtest::writeImage(bPath, Qt::blue), "Image b should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), then files sorted (a.png=1, b.png=2).
    QTRY_COMPARE(grid->itemCount(), 3);
    const int aPng = 1;
    const int bPng = 2;

    // --- Grid view: open, verify target, toggle closed. ---
    grid->select(aPng);
    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable in grid view.");

    auto *dialog = window->findChild<FileInfoDialog *>();
    QTRY_VERIFY2(dialog != nullptr, "The File info popup should exist after toggling it on.");
    QTRY_VERIFY2(dialog->isVisible(), "The File info popup should be visible in grid view.");
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(aPath).absoluteFilePath());

    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable a second time.");
    QTRY_VERIFY2(!dialog->isVisible(), "The File info popup should hide on a second toggle.");

    // --- Document view: open the other image, verify no early-return regression. ---
    grid->select(bPng);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable in document view (no MODE_FOLDERVIEW-only early return).");
    QTRY_VERIFY2(dialog->isVisible(), "The File info popup should be visible in document view.");
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(bPath).absoluteFilePath());

    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable a second time in document view.");
    QTRY_VERIFY2(!dialog->isVisible(), "The File info popup should hide on a second toggle in document view.");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoPopupOpensInBothViewsTest)

#include "test_file_info_popup_opens_in_both_views.moc"
