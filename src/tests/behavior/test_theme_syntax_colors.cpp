#include "support/thumbgrid_test_support.h"

#include "gui/viewers/textsyntaxhighlighter.h"
#include "themestore.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

// Text viewer syntax colors come from optional syntax_* theme keys; a theme that
// omits a key keeps the built-in color for its dark or light background.
class ThemeSyntaxColorsTest : public QObject {
    Q_OBJECT

private slots:
    void themeWithoutKeysKeepsTheBuiltInColors();
    void themeKeyOverridesOnlyItsColor();
    void highlighterPaintsKeywordsWithTheThemeColor();
};

namespace {

ColorScheme schemeFrom(const QString &colors, bool dark) {
    static QTemporaryDir dir;
    const QString path = dir.filePath(QString::number(qHash(colors)) + ".ini");
    {
        QSettings ini(path, QSettings::IniFormat);
        ini.setValue("Colors/background", dark ? "#1a1a1a" : "#f0f0f0");
        ini.setValue("Colors/text", dark ? "#b6b6b6" : "#202020");
        ini.setValue("Colors/widget", dark ? "#252525" : "#e0e0e0");
        if(!colors.isEmpty())
            ini.setValue("Colors/" + colors.section('=', 0, 0), colors.section('=', 1));
        ini.sync();
    }
    QSettings ini(path, QSettings::IniFormat);
    return ColorScheme(ThemeStore::parseColors(ini, 100));
}

} // namespace

void ThemeSyntaxColorsTest::themeWithoutKeysKeepsTheBuiltInColors() {
    const ColorScheme dark = schemeFrom("", true);
    QCOMPARE(dark.syntax_comment, QColor(106, 130, 106));
    QCOMPARE(dark.syntax_keyword, QColor(130, 170, 255));
    QCOMPARE(dark.syntax_string, QColor(195, 165, 105));
    QCOMPARE(dark.syntax_number, QColor(210, 140, 210));
    QCOMPARE(dark.syntax_meta, QColor(120, 200, 200));
    const ColorScheme light = schemeFrom("", false);
    QCOMPARE(light.syntax_comment, QColor(90, 125, 90));
    QCOMPARE(light.syntax_keyword, QColor(30, 80, 180));
    QCOMPARE(light.syntax_string, QColor(140, 100, 30));
    QCOMPARE(light.syntax_number, QColor(150, 50, 150));
    QCOMPARE(light.syntax_meta, QColor(20, 130, 130));
}

void ThemeSyntaxColorsTest::themeKeyOverridesOnlyItsColor() {
    const ColorScheme scheme = schemeFrom("syntax_keyword=#ff0000", true);
    QCOMPARE(scheme.syntax_keyword, QColor(255, 0, 0));
    QCOMPARE(scheme.syntax_string, QColor(195, 165, 105));
}

void ThemeSyntaxColorsTest::highlighterPaintsKeywordsWithTheThemeColor() {
    const ColorScheme scheme = schemeFrom("syntax_keyword=#00ff00", true);
    QTextDocument doc;
    TextSyntaxHighlighter highlighter(&doc, "main.cpp", scheme);
    doc.setPlainText("return 1;");
    highlighter.rehighlight();
    const auto formats = doc.firstBlock().layout()->formats();
    QVERIFY(!formats.isEmpty());
    QCOMPARE(formats.first().format.foreground().color(), QColor(0, 255, 0));
}

TG_BEHAVIOR_TEST_MAIN(ThemeSyntaxColorsTest)

#include "test_theme_syntax_colors.moc"
