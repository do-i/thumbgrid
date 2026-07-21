#include "support/thumbgrid_test_support.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include "shortcutpresetstore.h"
#include "themestore.h"

// Themes and presets are searched across the XDG base directories rather than a
// single compiled-in location, so a user can drop their own copy into
// ~/.config/thumbgrid/<kind>/ and have it win over the packaged/embedded one.
//
// These tests resolve the target directory the way the spec says to (writable
// generic config location + app name + kind) instead of asking AppPaths, so
// they check the documented contract rather than restating the implementation.
class XdgSearchPathOverridesTest : public QObject {
    Q_OBJECT

private slots:
    void userThemeOverridesEmbeddedCopy();
    void userPresetIsDiscoveredAndResolved();
    void missingUserCopyStillFallsBackToEmbedded();

private:
    static QString userDir(const QString &kind);
};

QString XdgSearchPathOverridesTest::userDir(const QString &kind) {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QLatin1Char('/') + QCoreApplication::applicationName() + QLatin1Char('/') + kind;
}

void XdgSearchPathOverridesTest::userThemeOverridesEmbeddedCopy() {
    // Baseline: with nothing in the user dir, the embedded qrc copy is used.
    QCOMPARE(ThemeStore::colorScheme(COLORS_LIGHT).accent.name(), QStringLiteral("#719ccd"));

    const QString dir = userDir(QStringLiteral("themes"));
    QVERIFY(QDir().mkpath(dir));
    QFile file(dir + QStringLiteral("/light.ini"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream(&file) << "[Theme]\ntid=1\nname=Light\n\n[Colors]\n"
                          "accent=#ff00ff\nbackground=#101010\ntext=#fafafa\nwidget=#202020\n";
    file.close();

    const ColorScheme overridden = ThemeStore::colorScheme(COLORS_LIGHT);
    QCOMPARE(overridden.accent.name(), QStringLiteral("#ff00ff"));
    QCOMPARE(overridden.background.name(), QStringLiteral("#101010"));

    QVERIFY(file.remove());
    // Removing the override restores the embedded copy - nothing is cached.
    QCOMPARE(ThemeStore::colorScheme(COLORS_LIGHT).accent.name(), QStringLiteral("#719ccd"));
}

void XdgSearchPathOverridesTest::userPresetIsDiscoveredAndResolved() {
    const QString id = QStringLiteral("xdg-test-preset");
    QVERIFY(ShortcutPresetStore::resolvePath(id).isEmpty());

    const QString dir = userDir(QStringLiteral("presets"));
    QVERIFY(QDir().mkpath(dir));
    const QString path = dir + QLatin1Char('/') + id + QStringLiteral(".json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream(&file) << "{\n"
                          "  \"_meta\": {\n"
                          "    \"id\": \"" << id << "\",\n"
                          "    \"name\": \"XDG Test\",\n"
                          "    \"os\": [\"linux\", \"windows\", \"macos\"],\n"
                          "    \"order\": 900\n"
                          "  },\n"
                          "  \"global\": { \"exit\": [\"$Ctrl+Q\"] }\n"
                          "}\n";
    file.close();

    QCOMPARE(ShortcutPresetStore::resolvePath(id), path);

    const QList<PresetInfo> presets = ShortcutPresetStore::available();
    const auto found = std::find_if(presets.cbegin(), presets.cend(),
                                    [&id](const PresetInfo &p) { return p.id == id; });
    QVERIFY2(found != presets.cend(), "User-supplied preset should appear in available()");
    QCOMPARE(found->name, QStringLiteral("XDG Test"));

    QVERIFY(file.remove());
    QVERIFY(ShortcutPresetStore::resolvePath(id).isEmpty());
}

void XdgSearchPathOverridesTest::missingUserCopyStillFallsBackToEmbedded() {
    // The built-in presets stay reachable when the user dir holds nothing.
    const QList<PresetInfo> presets = ShortcutPresetStore::available();
    const auto qimgv = std::find_if(presets.cbegin(), presets.cend(),
                                    [](const PresetInfo &p) { return p.id == QLatin1String("qimgv"); });
    QVERIFY2(qimgv != presets.cend(), "Embedded qimgv preset should remain available");
}

TG_BEHAVIOR_TEST_MAIN(XdgSearchPathOverridesTest)

#include "test_xdg_search_path_overrides.moc"
