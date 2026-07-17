#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// Renaming in the grid happens in place, in an editor drawn over the selected
// cell, rather than in the centered popup overlay used in document view. This
// drives the real UI entry point (Core::showRenameDialog) and asserts on the
// inline editor's presence, its preselection, and the commit/cancel outcomes.
class GridInlineRenameTest : public QObject {
    Q_OBJECT

private slots:
    void inlineEditorRenamesInPlace();

private:
    static QLineEdit *editorOf(FolderGridView *grid) {
        return grid->findChild<QLineEdit *>();
    }
};

void GridInlineRenameTest::inlineEditorRenamesInPlace() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString oldFilePath = root.filePath("gallery/a.png");
    const QString newFilePath = root.filePath("gallery/renamed.png");
    const QString discardedPath = root.filePath("gallery/discarded.png");

    QVERIFY2(tgtest::writeImage(oldFilePath, Qt::red), "Image should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), a.png (1).
    QTRY_COMPARE(grid->itemCount(), 2);

    // --- Commit path: the inline editor renames the file on disk. ---
    grid->select(1);
    QVERIFY2(QMetaObject::invokeMethod(&core, "showRenameDialog"),
             "showRenameDialog should be invocable via the meta-object system.");

    // An in-place editor should appear inside the grid, not a centered overlay.
    QLineEdit *editor = nullptr;
    QTRY_VERIFY2((editor = editorOf(grid)) != nullptr && editor->isVisible(),
                 "An inline rename editor should be shown inside the grid.");
    QCOMPARE(editor->text(), QString("a.png"));
    // Only the base name is preselected so the extension is preserved.
    QCOMPARE(editor->selectedText(), QString("a"));

    editor->setText("renamed.png");
    QTest::keyClick(editor, Qt::Key_Return);

    QTRY_VERIFY2(QFile::exists(newFilePath), "The renamed file should exist on disk.");
    QVERIFY2(!QFile::exists(oldFilePath), "The old file name should be gone from disk.");
    QVERIFY2(editor->isHidden(), "The inline editor should hide after committing.");

    // --- Cancel path: Escape discards the edit, leaving the file untouched. ---
    // Positions did not change, only the name: the file is still at index 1.
    grid->select(1);
    QVERIFY2(QMetaObject::invokeMethod(&core, "showRenameDialog"),
             "showRenameDialog should be invocable via the meta-object system.");
    QTRY_VERIFY2(editorOf(grid) != nullptr && editorOf(grid)->isVisible(),
                 "The inline rename editor should reopen for the second edit.");
    editor = editorOf(grid);

    editor->setText("discarded.png");
    QTest::keyClick(editor, Qt::Key_Escape);

    QTRY_VERIFY2(editor->isHidden(), "The inline editor should hide after cancelling.");
    QVERIFY2(QFile::exists(newFilePath), "The file should be untouched after cancel.");
    QVERIFY2(!QFile::exists(discardedPath),
             "No renamed file should be created when the edit is cancelled.");
}

TG_BEHAVIOR_TEST_MAIN(GridInlineRenameTest)

#include "test_grid_inline_rename.moc"
