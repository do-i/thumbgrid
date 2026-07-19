#include "support/thumbgrid_test_support.h"
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeView>
#include "core.h"
#include "components/actionmanager/actionmanager.h"
#include "gui/dialogs/duplicatefinderdialog.h"

static QImage makeScene(int id, int w = 512, int h = 384) {
    QImage img(w, h, QImage::Format_RGB32);
    QPainter p(&img);
    if(id == 0) {
        QLinearGradient g(0, 0, w, h);
        g.setColorAt(0, QColor(230, 225, 210));
        g.setColorAt(1, QColor(40, 60, 90));
        p.fillRect(img.rect(), g);
        p.setBrush(QColor(25, 20, 30));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(w / 4, h / 4), w / 6, w / 6);
        p.setBrush(QColor(240, 200, 60));
        p.drawRect(w / 2, h / 2, w / 3, h / 5);
    } else {
        for(int y = 0; y < 3; y++)
            for(int x = 0; x < 4; x++)
                p.fillRect(x * w / 4, y * h / 3, w / 4, h / 3,
                           ((x + y) % 2) ? QColor(235, 235, 235) : QColor(30, 30, 30));
    }
    p.end();
    return img;
}

class DuplicateFinderDialogTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir tmp;
private slots:
    void initTestCase();
    void dialogRunsSearchAndShowsGroupedResults();
    void smartSelectKeepsTheBestCopy();
    void movingCheckedFilesCreatesAuditableSession();
    void findDuplicatesActionOpensSeededDialog();
};

void DuplicateFinderDialogTest::initTestCase() {
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QVERIFY(root.mkpath("src"));
    QVERIFY(root.mkpath("tgt"));
    QImage sceneA = makeScene(0);
    QVERIFY(sceneA.save(tmp.filePath("src/A.png")));
    QVERIFY(QFile::copy(tmp.filePath("src/A.png"), tmp.filePath("tgt/exact.png")));
    QVERIFY(sceneA.save(tmp.filePath("tgt/reenc.jpg"), "JPG", 80));
    QVERIFY(makeScene(1).save(tmp.filePath("tgt/other.png")));
}

void DuplicateFinderDialogTest::dialogRunsSearchAndShowsGroupedResults() {
    DuplicateFinderDialog dialog;
    dialog.presetFor(tmp.filePath("src/A.png"), tmp.filePath("tgt"));
    dialog.show();
    auto *startButton = dialog.findChild<QPushButton *>("duplicateFinderStartButton");
    QVERIFY(startButton);
    startButton->click();
    QTRY_COMPARE_WITH_TIMEOUT(dialog.resultsModel()->matchCount(), 2, 15000);
    QTRY_VERIFY(!dialog.finder()->isRunning());
    // one group (the source image), expanded, with both matches under it
    auto *view = dialog.resultsView();
    QCOMPARE(view->model()->rowCount(), 1);
    QModelIndex group = view->model()->index(0, 0);
    QCOMPARE(view->model()->rowCount(group), 2);
    QVERIFY(view->isExpanded(group));
    // checking a match row is reflected in checkedPaths
    QModelIndex firstMatch = view->model()->index(0, DuplicateResultsModel::COL_CHECK, group);
    QVERIFY(view->model()->setData(firstMatch, Qt::Checked, Qt::CheckStateRole));
    QCOMPARE(dialog.resultsModel()->checkedPaths().count(), 1);
}

void DuplicateFinderDialogTest::smartSelectKeepsTheBestCopy() {
    DuplicateFinderDialog dialog;
    dialog.presetFor(tmp.filePath("src/A.png"), tmp.filePath("tgt"));
    dialog.findChild<QPushButton *>("duplicateFinderStartButton")->click();
    QTRY_COMPARE_WITH_TIMEOUT(dialog.resultsModel()->matchCount(), 2, 15000);
    QTRY_VERIFY(!dialog.finder()->isRunning());
    // the source (a PNG) is at least as large as both matches, so
    // "keep largest file" selects every match for removal
    dialog.resultsModel()->smartSelect(DuplicateResultsModel::KEEP_LARGEST_FILE);
    QCOMPARE(dialog.resultsModel()->checkedPaths().count(), 2);
    dialog.resultsModel()->smartSelect(DuplicateResultsModel::CLEAR_SELECTION);
    QCOMPARE(dialog.resultsModel()->checkedPaths().count(), 0);
    dialog.resultsModel()->smartSelect(DuplicateResultsModel::SELECT_ALL);
    QCOMPARE(dialog.resultsModel()->checkedPaths().count(), 2);
}

void DuplicateFinderDialogTest::movingCheckedFilesCreatesAuditableSession() {
    // separate fixture so the shared one stays intact for other tests
    QTemporaryDir work, moveDest;
    QVERIFY(work.isValid() && moveDest.isValid());
    QVERIFY(QDir(work.path()).mkpath("scope/sub"));
    QImage sceneA = makeScene(0);
    QVERIFY(sceneA.save(work.filePath("A.png")));
    QVERIFY(QFile::copy(work.filePath("A.png"), work.filePath("scope/copy1.png")));
    QVERIFY(QFile::copy(work.filePath("A.png"), work.filePath("scope/sub/copy2.png")));

    DuplicateFinderDialog dialog;
    dialog.presetFor(work.filePath("A.png"), work.filePath("scope"));
    dialog.findChild<QPushButton *>("duplicateFinderStartButton")->click();
    QTRY_COMPARE_WITH_TIMEOUT(dialog.resultsModel()->matchCount(), 2, 15000);
    QTRY_VERIFY(!dialog.finder()->isRunning());
    dialog.resultsModel()->smartSelect(DuplicateResultsModel::SELECT_ALL);
    dialog.moveCheckedTo(moveDest.path());

    // originals gone, model emptied
    QVERIFY(!QFile::exists(work.filePath("scope/copy1.png")));
    QVERIFY(!QFile::exists(work.filePath("scope/sub/copy2.png")));
    QCOMPARE(dialog.resultsModel()->matchCount(), 0);
    // one dedupe-* session folder with relative structure + manifest
    QStringList sessions = QDir(moveDest.path()).entryList({"dedupe-*"}, QDir::Dirs);
    QCOMPARE(sessions.count(), 1);
    QDir session(QDir(moveDest.path()).filePath(sessions.first()));
    QVERIFY(session.exists("copy1.png"));
    QVERIFY(session.exists("sub/copy2.png"));
    QFile manifest(session.filePath("manifest.txt"));
    QVERIFY(manifest.open(QIODevice::ReadOnly | QIODevice::Text));
    QString manifestText = manifest.readAll();
    QVERIFY(manifestText.contains("copy1.png"));
    QVERIFY(manifestText.contains("sub/copy2.png"));
    QCOMPARE(manifestText.trimmed().split('\n').count(), 2);
}

void DuplicateFinderDialogTest::findDuplicatesActionOpensSeededDialog() {
    Core core;
    core.loadPath(tmp.filePath("src/A.png"));
    core.showGui();
    QTRY_VERIFY(tgtest::mainWindow() != nullptr);
    QVERIFY(actionManager->invokeAction("findDuplicates"));
    auto *dialog = tgtest::mainWindow()->findChild<DuplicateFinderDialog *>();
    QTRY_VERIFY(dialog != nullptr);
    QTRY_VERIFY(dialog->isVisible());
}

TG_BEHAVIOR_TEST_MAIN(DuplicateFinderDialogTest)

#include "test_duplicate_finder_dialog.moc"
