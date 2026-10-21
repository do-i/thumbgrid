#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core.h"
#include "gui/contextmenu.h"
#include "gui/customwidgets/contextmenuitem.h"
#include "gui/mainwindow.h"
#include "gui/viewers/viewerwidget.h"

// The picture-view context menu had empty placeholders for Up/Down/Enter, so it
// could only be used with the mouse. Up/Down now move a highlight through the
// rows of the current page (wrapping at the ends), and Enter runs it.

class PictureContextMenuKeyboardNavigationTest : public QObject {
    Q_OBJECT

private slots:
    void arrowsMoveTheHighlightAndEnterRunsIt();
};

namespace {

QString highlightedText(ContextMenu *menu) {
    for(ContextMenuItem *item : menu->findChildren<ContextMenuItem *>())
        if(item->isHighlighted())
            return item->text();
    return QString();
}

} // namespace

void PictureContextMenuKeyboardNavigationTest::arrowsMoveTheHighlightAndEnterRunsIt() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    QVERIFY(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::darkCyan));

    Core core;
    QVERIFY(core.loadPath(root.filePath("gallery/a.png")));
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    // observe File info without opening the panel
    disconnect(actionManager, &ActionManager::toggleImageInfo, nullptr, nullptr);
    QSignalSpy infoAction(actionManager, &ActionManager::toggleImageInfo);

    auto viewerWidget = window->findChild<ViewerWidget *>();
    QVERIFY(viewerWidget);
    viewerWidget->showContextMenu(viewerWidget->mapToGlobal(QPoint(100, 100)));
    auto menu = window->findChild<ContextMenu *>();
    QVERIFY(menu);
    QTRY_VERIFY2(menu->isVisible(), "The context menu should open.");
    QCOMPARE(highlightedText(menu), QString());

    // Down starts at the top row; Up from there wraps to the bottom.
    QTest::keyClick(menu, Qt::Key_Down);
    QCOMPARE(highlightedText(menu), QStringLiteral("Print"));
    QTest::keyClick(menu, Qt::Key_Up);
    QCOMPARE(highlightedText(menu), QStringLiteral("Find duplicates..."));
    QTest::keyClick(menu, Qt::Key_Up);
    QCOMPARE(highlightedText(menu), QStringLiteral("File info"));

    // Enter runs the highlighted row and closes the menu.
    QTest::keyClick(menu, Qt::Key_Return);
    QCOMPARE(infoAction.count(), 1);
    QTRY_VERIFY2(!menu->isVisible(), "Running a row should close the menu.");

    // Reopening starts without a highlight.
    viewerWidget->showContextMenu(viewerWidget->mapToGlobal(QPoint(100, 100)));
    QTRY_VERIFY(menu->isVisible());
    QCOMPARE(highlightedText(menu), QString());

    // "Open with..." switches page and keeps the menu open, like a click.
    for(int i = 0; i < 20 && highlightedText(menu) != QStringLiteral("Open with..."); i++)
        QTest::keyClick(menu, Qt::Key_Down);
    QCOMPARE(highlightedText(menu), QStringLiteral("Open with..."));
    QTest::keyClick(menu, Qt::Key_Return);
    QVERIFY2(menu->isVisible(), "Open with... should keep the menu open.");
    QTest::keyClick(menu, Qt::Key_Down);
    QCOMPARE(highlightedText(menu), QStringLiteral("Back"));
    QTest::keyClick(menu, Qt::Key_Return);
    QVERIFY2(menu->isVisible(), "Back should keep the menu open.");
    QTest::keyClick(menu, Qt::Key_Down);
    QCOMPARE(highlightedText(menu), QStringLiteral("Print"));

    QTest::keyClick(menu, Qt::Key_Escape);
    QTRY_VERIFY(!menu->isVisible());
}

TG_BEHAVIOR_TEST_MAIN(PictureContextMenuKeyboardNavigationTest)

#include "test_picture_context_menu_keyboard_navigation.moc"
