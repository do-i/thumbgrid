#include "support/thumbgrid_test_support.h"

#include <QSettings>

// Shortcuts are stored per context on disk. New configs round-trip per context,
// and legacy context-less configs (every binding worked everywhere) migrate into
// both contexts so upgrading users lose nothing.
class ShortcutContextsPersistAndLegacyConfigsMigrateTest : public QObject {
    Q_OBJECT

private slots:
    void perContextBindingsRoundTrip();
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
    // The "=" key is encoded/decoded correctly and stays in its own context.
    QCOMPARE(in[MODE_DOCUMENT].value("="), QStringLiteral("zoomIn"));
    QVERIFY2(!in[MODE_FOLDERVIEW].contains("="), "The = binding should not leak into the other context.");
}

void ShortcutContextsPersistAndLegacyConfigsMigrateTest::legacyConfigMigratesIntoBothContexts() {
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

    // A context-less binding was active everywhere, so it seeds both contexts.
    QCOMPARE(in[MODE_DOCUMENT].value("Right"), QStringLiteral("nextImage"));
    QCOMPARE(in[MODE_FOLDERVIEW].value("Right"), QStringLiteral("nextImage"));
    // Legacy "eq" still decodes to the "=" key in both contexts.
    QCOMPARE(in[MODE_DOCUMENT].value("="), QStringLiteral("zoomIn"));
    QCOMPARE(in[MODE_FOLDERVIEW].value("="), QStringLiteral("zoomIn"));
}

TG_BEHAVIOR_TEST_MAIN(ShortcutContextsPersistAndLegacyConfigsMigrateTest)

#include "test_shortcut_contexts_persist_and_legacy_configs_migrate.moc"
