#include "support/thumbgrid_test_support.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

// Standalone shortcuts.json lives beside the main config file.
static QString shortcutsJsonPath() {
    return QFileInfo(QSettings().fileName()).absolutePath() + "/shortcuts.json";
}

// Shortcuts are stored per context on disk. New configs round-trip per context,
// and legacy context-less configs (every binding worked everywhere) migrate into
// both contexts so upgrading users lose nothing.
class ShortcutContextsPersistAndLegacyConfigsMigrateTest : public QObject {
    Q_OBJECT

private slots:
    void perContextBindingsRoundTrip();
    void legacyFolderviewJsonStillReads();
    void legacyConfigMigratesIntoBothContexts();
};

void ShortcutContextsPersistAndLegacyConfigsMigrateTest::perContextBindingsRoundTrip() {
    QMap<ViewMode, QMap<QString, QString>> out;
    out[MODE_DOCUMENT].insert("C", "crop");
    out[MODE_FOLDERVIEW].insert("C", "copyFile");
    out[MODE_DOCUMENT].insert("=", "zoomIn"); // exercises the "=" -> "eq" escaping
    settings->saveShortcuts(out);

    QMap<ViewMode, QMap<QString, QString>> in;
    settings->readShortcuts(in);

    // Same key, different action per context, survives a save/load cycle.
    QCOMPARE(in[MODE_DOCUMENT].value("C"), QStringLiteral("crop"));
    QCOMPARE(in[MODE_FOLDERVIEW].value("C"), QStringLiteral("copyFile"));
    // The "=" key round-trips through JSON with no escaping and stays in its context.
    QCOMPARE(in[MODE_DOCUMENT].value("="), QStringLiteral("zoomIn"));
    QVERIFY2(!in[MODE_FOLDERVIEW].contains("="), "The = binding should not leak into the other context.");
    QVERIFY2(QFile::exists(shortcutsJsonPath()), "Saving shortcuts must produce shortcuts.json.");

    QFile file(shortcutsJsonPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY2(root.contains("grid"), "Folder/grid shortcuts must save under the grid token.");
    QVERIFY2(!root.contains("folderview"), "New shortcut saves should not write the legacy folderview token.");
}

void ShortcutContextsPersistAndLegacyConfigsMigrateTest::legacyFolderviewJsonStillReads() {
    QJsonObject root;
    QJsonObject folderView;
    folderView.insert("G", "copyFile");
    root.insert("folderview", folderView);
    QFile file(shortcutsJsonPath());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(root).toJson());
    file.close();

    QMap<ViewMode, QMap<QString, QString>> in;
    settings->readShortcuts(in);

    QCOMPARE(in[MODE_FOLDERVIEW].value("G"), QStringLiteral("copyFile"));
    QVERIFY2(!in[MODE_DOCUMENT].contains("G"), "Legacy folderview JSON should stay scoped to the grid context.");
}

void ShortcutContextsPersistAndLegacyConfigsMigrateTest::legacyConfigMigratesIntoBothContexts() {
    // Simulate an upgrading install: an old "[Controls] shortcuts=" list and no
    // shortcuts.json yet (a prior test in this binary may have created one).
    QFile::remove(shortcutsJsonPath());
    // Write a legacy "action=key" list (no context token) the way older versions
    // did. On Linux Settings uses a default QSettings(), so a matching one here
    // shares the same backing store.
    {
        QSettings legacy;
        legacy.beginGroup("Controls");
        legacy.setValue("shortcuts", QStringList{"nextImage=Right", "zoomIn=eq"});
        legacy.endGroup();
        legacy.sync();
    }

    QMap<ViewMode, QMap<QString, QString>> in;
    settings->readShortcuts(in);

    // The one-time migration produced the new file.
    QVERIFY2(QFile::exists(shortcutsJsonPath()), "Legacy migration must create shortcuts.json.");

    // A context-less binding was active everywhere, so it seeds both contexts.
    QCOMPARE(in[MODE_DOCUMENT].value("Right"), QStringLiteral("nextImage"));
    QCOMPARE(in[MODE_FOLDERVIEW].value("Right"), QStringLiteral("nextImage"));
    // Legacy "eq" still decodes to the "=" key in both contexts.
    QCOMPARE(in[MODE_DOCUMENT].value("="), QStringLiteral("zoomIn"));
    QCOMPARE(in[MODE_FOLDERVIEW].value("="), QStringLiteral("zoomIn"));
}

TG_BEHAVIOR_TEST_MAIN(ShortcutContextsPersistAndLegacyConfigsMigrateTest)

#include "test_shortcut_contexts_persist_and_legacy_configs_migrate.moc"
