// The picture view's context menu shows its zoom and transform actions as bare
// icons. Each must name itself (and its shortcut) on hover, or Resize and Crop
// are a guessing game.
#include "support/thumbgrid_test_support.h"

#include "gui/contextmenu.h"

class ViewerMenuIconsHaveTooltipsTest : public QObject {
    Q_OBJECT

private slots:
    void everyIconButtonNamesItsAction();
};

void ViewerMenuIconsHaveTooltipsTest::everyIconButtonNamesItsAction() {
    QWidget host;
    ContextMenu menu(&host);
    menu.showAt(QPoint(0, 0));

    const QStringList names = {"zoomIn", "zoomOut", "zoomOriginal", "fitWidth", "fitWindow",
                               "fitWindowStretch", "rotateLeft", "rotateRight", "flipH", "flipV",
                               "crop", "resize"};
    for(const QString &name : names) {
        auto *button = menu.findChild<QWidget *>(name);
        QVERIFY2(button, qPrintable("Missing icon button " + name));
        QVERIFY2(!button->toolTip().isEmpty(), qPrintable(name + " should have a tooltip"));
    }
    QVERIFY(menu.findChild<QWidget *>("resize")->toolTip().startsWith("Resize"));
    menu.hide();
}

TG_BEHAVIOR_TEST_MAIN(ViewerMenuIconsHaveTooltipsTest)

#include "test_viewer_menu_icons_have_tooltips.moc"
