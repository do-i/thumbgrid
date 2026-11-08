#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// When the current folder and its parent are deleted outside the app, going
// up must climb to the nearest ancestor that still exists instead of doing nothing.
class GoingUpSkipsRemovedAncestorsTest : public QObject {
    Q_OBJECT
private slots:
    void goingUpClimbsPastRemovedFolders();
};

void GoingUpSkipsRemovedAncestorsTest::goingUpClimbsPastRemovedFolders() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());
    QVERIFY(root.mkpath("top/keep"));
    QVERIFY(root.mkpath("top/M/N"));
    for(int i = 0; i < 3; i++)
        QVERIFY(tgtest::writeImage(root.filePath(QString("top/M/N/img%1.png").arg(i)), Qt::darkCyan));

    Core core;
    QVERIFY(core.loadPath(root.filePath("top/M/N")));
    core.showGui();
    QTRY_VERIFY(tgtest::mainWindow() != nullptr);
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY(grid != nullptr);
    QTRY_COMPARE(grid->itemCount(), 4); // ".." + 3 images

    // remove N and its parent M from outside the app
    QVERIFY(QDir(root.filePath("top/M")).removeRecursively());
    QVERIFY(!QFileInfo::exists(root.filePath("top/M")));

    QVERIFY(QMetaObject::invokeMethod(&core, "loadParentDir", Qt::DirectConnection));
    // landed in "top": ".." + the surviving "keep" folder
    QTRY_COMPARE(grid->itemCount(), 2);
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);
}

TG_BEHAVIOR_TEST_MAIN(GoingUpSkipsRemovedAncestorsTest)

#include "test_going_up_skips_removed_ancestors.moc"
