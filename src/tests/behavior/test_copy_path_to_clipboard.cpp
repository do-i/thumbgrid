#include "support/thumbgrid_test_support.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class CopyPathToClipboardTest : public QObject {
    Q_OBJECT

private slots:
    void copyPathActionCopiesFolderPathToClipboard();
};

void CopyPathToClipboardTest::copyPathActionCopiesFolderPathToClipboard() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/noimg"), "noimg subfolder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString noimgPath = root.filePath("gallery/noimg");

    {
        // A folder with no images: this used to be silently blocked by
        // Core::copyPathClipboard's now-removed model->isEmpty() check.
        QFile notes(root.filePath("gallery/noimg/notes.txt"));
        QVERIFY(notes.open(QIODevice::WriteOnly));
        notes.write("just some notes");
    }

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), then noimg (1).
    QTRY_COMPARE(grid->itemCount(), 2);
    const int noimgDir = 1;

    grid->select(noimgDir);
    QCOMPARE(grid->selection(), QList<int>{noimgDir});

    QApplication::clipboard()->clear();
    QVERIFY2(actionManager->invokeAction("copyPathClipboard"), "copyPathClipboard action should be invocable.");
    QCOMPARE(QApplication::clipboard()->text(), QDir(noimgPath).absolutePath());

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(CopyPathToClipboardTest)

#include "test_copy_path_to_clipboard.moc"
