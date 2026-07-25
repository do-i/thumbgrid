#include "support/thumbgrid_test_support.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QTemporaryDir>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// Regression test for M1 (docs/013-code-review-findings-action-plan.md): edited
// images dragged or copied used to be exported to a single fixed, guessable,
// never-deleted "<cacheDir>/image.png". Two exports of an edited image must now
// land in a private, owner-only directory under distinct file names.
class EditedImageExportUsesUniqueTempFilesTest : public QObject {
    Q_OBJECT

private slots:
    void exportingTheSameEditedImageTwiceYieldsDistinctFiles();
};

void EditedImageExportUsesUniqueTempFilesTest::exportingTheSameEditedImageTwiceYieldsDistinctFiles() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    const QString imagePath = root.filePath("a.png");
    QVERIFY2(tgtest::writeImage(imagePath, Qt::red), "Fixture image should be written.");

    Core core;
    QVERIFY2(core.loadPath(fixture.path()), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), then a.png (1).
    QTRY_COMPARE(grid->itemCount(), 2);
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    // Mark the image edited without touching disk (document view edits are
    // in-memory only; edit_template() only saves in folder view).
    QVERIFY2(actionManager->invokeAction("rotateLeft"), "rotateLeft action should be invocable.");

    QApplication::clipboard()->clear();
    QVERIFY2(actionManager->invokeAction("copyFileClipboard"), "copyFileClipboard action should be invocable.");
    const QList<QUrl> firstUrls = QApplication::clipboard()->mimeData()->urls();
    QCOMPARE(firstUrls.size(), 1);
    const QString firstPath = firstUrls.first().toLocalFile();
    QVERIFY2(QFileInfo::exists(firstPath), "The first export should exist on disk.");

    QApplication::clipboard()->clear();
    QVERIFY2(actionManager->invokeAction("copyFileClipboard"), "copyFileClipboard action should be invocable a second time.");
    const QList<QUrl> secondUrls = QApplication::clipboard()->mimeData()->urls();
    QCOMPARE(secondUrls.size(), 1);
    const QString secondPath = secondUrls.first().toLocalFile();
    QVERIFY2(QFileInfo::exists(secondPath), "The second export should exist on disk.");

    QVERIFY2(firstPath != secondPath, "Two exports of the same edited image must use distinct file paths.");
    // Both files must still exist: neither export may have been deleted or
    // overwritten by the other.
    QVERIFY2(QFileInfo::exists(firstPath), "The first export must still exist after the second export.");
    QVERIFY2(QFileInfo::exists(secondPath), "The second export must still exist.");

    // The containing directory must be owner-only (0700), mirroring the
    // video-thumbnail temp dir convention.
    const QString exportDir = QFileInfo(firstPath).absolutePath();
    QCOMPARE(exportDir, QFileInfo(secondPath).absolutePath());
    const QFileDevice::Permissions perms = QFileInfo(exportDir).permissions();
    const QFileDevice::Permissions ownerOnly =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
    const QFileDevice::Permissions groupOrOther =
        QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
        QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    QVERIFY2((perms & ownerOnly) == ownerOnly, "The export directory should be readable/writable/executable by its owner.");
    QVERIFY2((perms & groupOrOther) == QFileDevice::Permissions(), "The export directory should not be accessible by group or other.");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(EditedImageExportUsesUniqueTempFilesTest)

#include "test_edited_image_export_uses_unique_temp_files.moc"
