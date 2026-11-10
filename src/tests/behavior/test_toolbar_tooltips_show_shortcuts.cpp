#include "support/thumbgrid_test_support.h"

#include "components/actionmanager/actionmanager.h"
#include "gui/customwidgets/iconbutton.h"
#include "gui/folderview/folderview.h"

// A grid top bar button's tooltip names its action and the key bound to it,
// and follows the key when the user rebinds it while the app is running.

class ToolbarTooltipsShowShortcutsTest : public QObject {
    Q_OBJECT

private slots:
    void aTopBarTooltipFollowsARebind();
};

void ToolbarTooltipsShowShortcutsTest::aTopBarTooltipFollowsARebind() {
    FolderView view;
    auto *button = view.findChild<IconButton *>("settingsButton");
    QVERIFY(button);

    const QString before = actionManager->shortcutForAction(MODE_FOLDERVIEW, "openSettings");
    QVERIFY2(!before.isEmpty(), "openSettings should have a default key.");
    QCOMPARE(button->toolTip(), QStringLiteral("Settings (%1)").arg(before));

    actionManager->removeAllShortcuts("openSettings");
    QCOMPARE(button->toolTip(), QStringLiteral("Settings"));

    actionManager->addShortcut(MODE_GLOBAL, "Ctrl+Alt+F9", "openSettings");
    QCOMPARE(button->toolTip(), QStringLiteral("Settings (Ctrl+Alt+F9)"));

    // buttons without a bindable action carry plain text
    QCOMPARE(view.findChild<QWidget *>("homeButton")->toolTip(), QStringLiteral("Go to home folder"));
}

TG_BEHAVIOR_TEST_MAIN(ToolbarTooltipsShowShortcutsTest)

#include "test_toolbar_tooltips_show_shortcuts.moc"
