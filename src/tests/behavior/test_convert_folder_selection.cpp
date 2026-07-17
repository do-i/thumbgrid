#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class ConvertFolderSelectionTest : public QObject {
    Q_OBJECT

private slots:
    void convertSelectionToFormatExpandsSelectedFolder();
};

void ConvertFolderSelectionTest::convertSelectionToFormatExpandsSelectedFolder() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/conv"), "conv subfolder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString pngPath = root.filePath("gallery/conv/img.png");
    const QString txtPath = root.filePath("gallery/conv/notes.txt");
    const QString jpgPath = root.filePath("gallery/conv/img.jpg");
    const QString otherPngPath = root.filePath("gallery/other.png");

    QVERIFY2(tgtest::writeImage(pngPath, Qt::red), "Image should be written.");
    // The directory model treats a folder with no direct files as "empty" and
    // short-circuits convertSelectionToFormat before it even looks at the
    // selection, so keep at least one file directly in the gallery.
    QVERIFY2(tgtest::writeImage(otherPngPath, Qt::blue), "Sibling image should be written.");
    {
        QFile notes(txtPath);
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

    // Parent ".." (0), conv(1), other.png(2).
    QTRY_COMPARE(grid->itemCount(), 3);

    grid->select(1);
    bool invoked = QMetaObject::invokeMethod(&core, "convertSelectionToFormat",
                                              Q_ARG(QString, QString("jpg")));
    QVERIFY2(invoked, "convertSelectionToFormat should be invocable via the meta-object system.");

    QTRY_VERIFY2(QFile::exists(jpgPath), "The converted jpg should appear inside the folder.");
    QVERIFY2(QFile::exists(pngPath), "The original png should be untouched.");
    QVERIFY2(QFile::exists(txtPath), "The unrelated text file should be untouched.");

    // Nothing else should have appeared in the folder.
    QDir convDir(root.filePath("gallery/conv"));
    QCOMPARE(convDir.entryList(QDir::Files).count(), 3);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(ConvertFolderSelectionTest)

#include "test_convert_folder_selection.moc"
