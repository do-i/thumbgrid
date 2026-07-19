#include "support/thumbgrid_test_support.h"

#include "components/storeddata/storeddataregistry.h"
#include "gui/dialogs/settingsdialog.h"
#include "themestore.h"

#include <QCheckBox>
#include <QDir>
#include <QPushButton>
#include <QTreeWidget>

// The "Stored data" settings page and its backing registry: every store the
// app keeps about the user's files can be cleared individually, in a checked
// batch, or automatically at exit via State/clearOnExit.
class StoredDataCleanupTest : public QObject {
    Q_OBJECT

private slots:
    void registryClearsEachStoreExactly();
    void exitClearRespectsSelection();
    void pageChecksPersistTheExitList();
    void pathColumnShowsFileOrParentDir();

private:
    StoredDataStore storeById(const QString &id);
    void seedAllStores();
    QString thumbDir() { return settings->thumbnailCacheDir(); }
};

StoredDataStore StoredDataCleanupTest::storeById(const QString &id) {
    const QList<StoredDataStore> all = StoredData::stores();
    for(const StoredDataStore &store : all)
        if(store.id == id)
            return store;
    return {};
}

void StoredDataCleanupTest::seedAllStores() {
    settings->setSavedPaths({"/photos/a", "/photos/b"});
    settings->setBookmarks({"/photos/keep"});
    settings->setDuplicateFinderTargets({"/photos/dupes"});
    QVERIFY(tgtest::writeImage(thumbDir() + "thumb1.png", Qt::red));
    QVERIFY(tgtest::writeImage(thumbDir() + "thumb2.png", Qt::blue));
    QFile marker(thumbDir() + "not-ours.txt");
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("keep");
    marker.close();
    QFile index(settings->duplicateHashCachePath());
    QVERIFY(index.open(QIODevice::WriteOnly));
    index.write("fake-hash-index");
    index.close();
}

void StoredDataCleanupTest::registryClearsEachStoreExactly() {
    seedAllStores();

    QCOMPARE(StoredData::stores().count(), 5);

    storeById("savedPaths").clear();
    QVERIFY2(!settings->savedPathsStored(), "savedPaths should be removed from the config.");
    QCOMPARE(settings->bookmarks(), QStringList{"/photos/keep"});

    storeById("bookmarks").clear();
    QVERIFY(settings->bookmarks().isEmpty());

    storeById("dupeHistory").clear();
    QVERIFY(settings->duplicateFinderTargets().isEmpty());

    storeById("dupeIndex").clear();
    QVERIFY(!QFile::exists(settings->duplicateHashCachePath()));

    storeById("thumbnails").clear();
    QVERIFY(!QFile::exists(thumbDir() + "thumb1.png"));
    QVERIFY(!QFile::exists(thumbDir() + "thumb2.png"));
    QVERIFY2(QFile::exists(thumbDir() + "not-ours.txt"),
             "Clearing thumbnails must only touch the cache's own PNGs.");
}

// Stores that live as keys inside thumbgrid.conf (single file) report that
// file's path; the thumbnail cache (many PNGs) reports its parent directory.
void StoredDataCleanupTest::pathColumnShowsFileOrParentDir() {
    const QString confPath = QDir::cleanPath(settings->settingsFilePath());
    QCOMPARE(QDir::cleanPath(storeById("savedPaths").path), confPath);
    QCOMPARE(QDir::cleanPath(storeById("bookmarks").path), confPath);
    QCOMPARE(QDir::cleanPath(storeById("dupeHistory").path), confPath);
    QCOMPARE(QDir::cleanPath(storeById("dupeIndex").path),
             QDir::cleanPath(settings->duplicateHashCachePath()));
    QCOMPARE(QDir::cleanPath(storeById("thumbnails").path),
             QDir::cleanPath(settings->thumbnailCacheDir()));

    SettingsDialog dialog;
    auto *table = dialog.findChild<QTreeWidget *>("storedDataTable");
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 4);
    QCOMPARE(table->headerItem()->text(1), QStringLiteral("Path"));
    QCOMPARE(QDir::cleanPath(table->topLevelItem(0)->text(1)), confPath);
    QCOMPARE(QDir::cleanPath(table->topLevelItem(4)->text(1)),
             QDir::cleanPath(settings->thumbnailCacheDir()));
}

void StoredDataCleanupTest::exitClearRespectsSelection() {
    seedAllStores();
    settings->setClearOnExitStores({"savedPaths", "dupeIndex"});

    StoredData::clearStoresMarkedForExit();

    QVERIFY2(!settings->savedPathsStored(), "Selected savedPaths should be cleared at exit.");
    QVERIFY(!QFile::exists(settings->duplicateHashCachePath()));
    QCOMPARE(settings->bookmarks(), QStringList{"/photos/keep"});
    QVERIFY2(QFile::exists(thumbDir() + "thumb1.png"),
             "Unselected stores must survive the exit clear.");
    QVERIFY2(settings->clearOnExitStores() == QStringList({"savedPaths", "dupeIndex"}),
             "The exit selection itself persists across runs.");
}

void StoredDataCleanupTest::pageChecksPersistTheExitList() {
    settings->setClearOnExitStores({});
    SettingsDialog dialog;
    auto *table = dialog.findChild<QTreeWidget *>("storedDataTable");
    auto *selectAll = dialog.findChild<QCheckBox *>("storedDataSelectAll");
    auto *exitBox = dialog.findChild<QCheckBox *>("storedDataClearOnExit");
    auto *deleteButton = dialog.findChild<QPushButton *>("storedDataDeleteSelected");
    QVERIFY(table && selectAll && exitBox && deleteButton);
    QCOMPARE(table->topLevelItemCount(), 5);
    QVERIFY(!deleteButton->isEnabled());

    // checking rows while "delete on exit" is on persists their ids
    exitBox->setChecked(true);
    table->topLevelItem(0)->setCheckState(0, Qt::Checked);
    QCOMPARE(settings->clearOnExitStores(), QStringList{"savedPaths"});
    QVERIFY(deleteButton->isEnabled());
    QCOMPARE(selectAll->checkState(), Qt::PartiallyChecked);

    // select-all checks every row and persists all ids
    selectAll->click();
    QCOMPARE(settings->clearOnExitStores().count(), 5);
    QCOMPARE(selectAll->checkState(), Qt::Checked);

    // unchecking the exit box empties the persisted list but keeps row checks
    exitBox->setChecked(false);
    QVERIFY(settings->clearOnExitStores().isEmpty());
    QVERIFY(deleteButton->isEnabled());

    // a fresh dialog restores checks and the exit box from the persisted list
    settings->setClearOnExitStores({"bookmarks", "thumbnails"});
    SettingsDialog dialog2;
    auto *table2 = dialog2.findChild<QTreeWidget *>("storedDataTable");
    auto *exitBox2 = dialog2.findChild<QCheckBox *>("storedDataClearOnExit");
    auto *selectAll2 = dialog2.findChild<QCheckBox *>("storedDataSelectAll");
    QVERIFY(table2 && exitBox2 && selectAll2);
    QVERIFY(exitBox2->isChecked());
    QCOMPARE(table2->topLevelItem(1)->checkState(0), Qt::Checked);
    QCOMPARE(table2->topLevelItem(0)->checkState(0), Qt::Unchecked);
    QCOMPARE(selectAll2->checkState(), Qt::PartiallyChecked);
    // reopening must not rewrite the list as a side effect
    QCOMPARE(settings->clearOnExitStores(), QStringList({"bookmarks", "thumbnails"}));

    // optional offscreen render for visual verification across presets
    const QString shot = qEnvironmentVariable("TG_STORED_DATA_SHOT");
    if(!shot.isEmpty()) {
        dialog2.switchToPage(7);
        dialog2.resize(900, 720);
        for(const auto &[tid, suffix] : std::initializer_list<std::pair<ColorSchemes, QString>>{
                {ColorSchemes::COLORS_DARK, "-dark"}, {ColorSchemes::COLORS_LIGHT, "-light"}}) {
            settings->setColorScheme(ThemeStore::colorScheme(tid));
            QTest::qWait(200);
            dialog2.grab().save(shot + suffix + ".png");
        }
    }
}

TG_BEHAVIOR_TEST_MAIN(StoredDataCleanupTest)

#include "test_stored_data_cleanup.moc"
