#include "support/thumbgrid_test_support.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

// Standalone shortcuts.json lives beside the main config file.
static QString shortcutsJsonPath() {
    return QFileInfo(QSettings().fileName()).absolutePath() + "/shortcuts.json";
}

// Shortcuts are stored per context on disk. New configs round-trip per context,
// and legacy context-less configs (every binding worked everywhere) migrate into
// the global context so upgrading users lose nothing.
class ShortcutContextsPersistAndLegacyConfigsMigrateTest : public QObject {
    Q_OBJECT

private slots:
    void perContextBindingsRoundTrip();
    void globalBindingsRoundTripAndCollapseDuplicateContexts();
    void primaryAndDisabledMetadataRoundTrip();
    void legacyFolderviewJsonStillReads();
    void legacyConfigMigratesIntoGlobalContext();
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
    QCOMPARE(root.value("document").toObject().value("crop").toArray().at(0).toString(), QStringLiteral("C"));
    const QJsonArray zoomKeys = root.value("document").toObject().value("zoomIn").toArray();
    QCOMPARE(zoomKeys.size(), 1);
    QCOMPARE(zoomKeys.at(0).toString(), QStringLiteral("="));
}

void ShortcutContextsPersistAndLegacyConfigsMigrateTest::globalBindingsRoundTripAndCollapseDuplicateContexts() {
    QMap<ViewMode, QMap<QString, QString>> out;
    out[MODE_GLOBAL].insert("F", "toggleFullscreen");
    out[MODE_DOCUMENT].insert("C", "crop");
    out[MODE_FOLDERVIEW].insert("C", "copyFile");
    settings->saveShortcuts(out);

    QMap<ViewMode, QMap<QString, QString>> in;
    settings->readShortcuts(in);

    QCOMPARE(in[MODE_GLOBAL].value("F"), QStringLiteral("toggleFullscreen"));
    QCOMPARE(in[MODE_DOCUMENT].value("C"), QStringLiteral("crop"));
    QCOMPARE(in[MODE_FOLDERVIEW].value("C"), QStringLiteral("copyFile"));

    QJsonObject root;
    QJsonObject document;
    QJsonObject grid;
    document.insert("P", "openSettings");
    grid.insert("P", "openSettings");
    root.insert("document", document);
    root.insert("grid", grid);
    QFile file(shortcutsJsonPath());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(root).toJson());
    file.close();

    in.clear();
    settings->readShortcuts(in);
    QCOMPARE(in[MODE_GLOBAL].value("P"), QStringLiteral("openSettings"));
    QVERIFY2(!in[MODE_DOCUMENT].contains("P"), "Duplicate document/grid binding should collapse to global.");
    QVERIFY2(!in[MODE_FOLDERVIEW].contains("P"), "Duplicate document/grid binding should collapse to global.");

    QFile rewritten(shortcutsJsonPath());
    QVERIFY(rewritten.open(QIODevice::ReadOnly));
    const QJsonObject rewrittenRoot = QJsonDocument::fromJson(rewritten.readAll()).object();
    QCOMPARE(rewrittenRoot.value("global").toObject().value("openSettings").toArray().at(0).toString(), QStringLiteral("P"));
    QVERIFY2(!rewrittenRoot.value("document").toObject().contains("openSettings"), "The JSON file should be rewritten without the duplicate document binding.");
    QVERIFY2(!rewrittenRoot.value("grid").toObject().contains("openSettings"), "The JSON file should be rewritten without the duplicate grid binding.");
}

void ShortcutContextsPersistAndLegacyConfigsMigrateTest::primaryAndDisabledMetadataRoundTrip() {
    QMap<ViewMode, QMap<QString, QString>> shortcuts;
    shortcuts[MODE_GLOBAL].insert("F12", "openSettings");
    shortcuts[MODE_GLOBAL].insert("P", "openSettings");
    shortcuts[MODE_FOLDERVIEW].insert("C", "copyFile");
    shortcuts[MODE_DOCUMENT].insert("=", "zoomIn");
    settings->saveShortcuts(shortcuts);

    QMap<ViewMode, QMap<QString, QString>> primary;
    primary[MODE_GLOBAL].insert("openSettings", "P");
    primary[MODE_FOLDERVIEW].insert("copyFile", "C");
    primary[MODE_DOCUMENT].insert("zoomIn", "=");
    settings->saveShortcutPrimary(primary);

    QMap<ViewMode, QStringList> disabled;
    disabled[MODE_FOLDERVIEW] = QStringList{"copyFile"};
    settings->saveDisabledShortcuts(disabled);

    QMap<ViewMode, QMap<QString, QString>> primaryIn;
    settings->readShortcutPrimary(primaryIn);
    QCOMPARE(primaryIn[MODE_GLOBAL].value("openSettings"), QStringLiteral("P"));
    QCOMPARE(primaryIn[MODE_FOLDERVIEW].value("copyFile"), QStringLiteral("C"));
    QCOMPARE(primaryIn[MODE_DOCUMENT].value("zoomIn"), QStringLiteral("="));

    QMap<ViewMode, QStringList> disabledIn;
    settings->readDisabledShortcuts(disabledIn);
    QCOMPARE(disabledIn[MODE_FOLDERVIEW], QStringList{"copyFile"});

    QFile file(shortcutsJsonPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject openSettings = root.value("global").toObject().value("openSettings").toObject();
    QCOMPARE(openSettings.value("primacy").toString(), QStringLiteral("P"));
    QCOMPARE(openSettings.value("keys").toArray().at(0).toString(), QStringLiteral("F12"));
    QCOMPARE(openSettings.value("keys").toArray().at(1).toString(), QStringLiteral("P"));
    QVERIFY2(!root.contains("_primary"), "Primary-key metadata should be embedded in each action entry.");
    QVERIFY2(root.contains("_disabled"), "Disabled shortcut metadata must be stored in shortcuts.json.");
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

    QFile rewritten(shortcutsJsonPath());
    QVERIFY(rewritten.open(QIODevice::ReadOnly));
    const QJsonObject rewrittenRoot = QJsonDocument::fromJson(rewritten.readAll()).object();
    QVERIFY2(!rewrittenRoot.contains("folderview"), "Legacy folderview token should be rewritten to grid.");
    QCOMPARE(rewrittenRoot.value("grid").toObject().value("copyFile").toArray().at(0).toString(), QStringLiteral("G"));
}

void ShortcutContextsPersistAndLegacyConfigsMigrateTest::legacyConfigMigratesIntoGlobalContext() {
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

    // A context-less binding was active everywhere, so it becomes global.
    QCOMPARE(in[MODE_GLOBAL].value("Right"), QStringLiteral("nextImage"));
    // Legacy "eq" still decodes to the "=" key.
    QCOMPARE(in[MODE_GLOBAL].value("="), QStringLiteral("zoomIn"));
}

TG_BEHAVIOR_TEST_MAIN(ShortcutContextsPersistAndLegacyConfigsMigrateTest)

#include "test_shortcut_contexts_persist_and_legacy_configs_migrate.moc"
