#include "support/thumbgrid_test_support.h"

#include <memory>

#include <QPixmap>
#include <QScreen>
#include <QSize>

#include "gui/mainwindow.h"
#include "gui/panels/infobar/infobarproxy.h"

// Auto-resize has to size the *viewport* to the image and add the window chrome
// back on top, or the fit mode scales the image down to whatever is left after
// the status footer and the slack shows up as empty bars either side.
//
// That compensation used to be skipped whenever the window was not visible yet -
// which is exactly the startup path: main.cpp calls Core::loadPath() for a
// command line argument *before* Core::showGui(), so `thumbgrid img.jpg` always
// resizes while hidden. And the hidden branch does not call setGeometry(), it
// writes settings->setWindowGeometry(), which is both what the first show
// restores and what the next launch starts from - so the wrong geometry stuck.
//
// These checks drive that state directly: a main window in the state
// Core::initGui() leaves it in (constructed, never shown) being handed an image.
// No Core, no event loop - the defect is entirely in the hidden code path, and
// keeping the window hidden is the point of the test.

namespace {

// Comfortably inside the auto-resize limit on both axes, so the geometry below
// is the image plus chrome and not a clamped-to-screen size.
QSize imageSizeForThisScreen() {
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    const double limit = settings->autoResizeLimit() / 100.0;
    return QSize(static_cast<int>(available.width() * limit) / 2,
                 static_cast<int>(available.height() * limit) / 2);
}

// The windowed status footer. FolderViewProxy owns a second InfoBarProxy for the
// grid's own footer, so pick by the accessible name InfoBarProxy sets on itself;
// the folder one renames itself to "FolderViewStatusFooter".
InfoBarProxy *documentStatusFooter(MW *window) {
    for(InfoBarProxy *bar : window->findChildren<InfoBarProxy *>())
        if(bar->accessibleName() == QStringLiteral("InfoBarProxy"))
            return bar;
    return nullptr;
}

} // namespace

class StartupAutoResizeChromeTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void hiddenWindowPersistsRoomForTheStatusFooter();
    void hiddenWindowPersistsJustTheImageWhenTheFooterIsOff();
    void autoResizeOffLeavesThePersistedGeometryAlone();
};

void StartupAutoResizeChromeTest::init() {
    // preShowResize() bails out on any non-normal window state, and
    // restoreWindowGeometry() would apply a maximized state from a stale config.
    settings->setMaximizedWindow(false);
    settings->setFullscreenMode(false);
    // A pinned panel is chrome too, but its extent is unknowable before the
    // thumbnail strip is initialized (setupFullUi(), after the show). Keep it out
    // so these checks are about the part that is knowable.
    settings->setPanelEnabled(false);
    settings->setPanelPinned(false);
    settings->setAutoResizeWindow(true);
    settings->setAutoResizeLimit(90);
}

// The reported bug, on the path that persists it.
void StartupAutoResizeChromeTest::hiddenWindowPersistsRoomForTheStatusFooter() {
    settings->setInfoBarWindowed(true);

    // Deliberately leaked, like every other window in these tests: the app
    // deletes MW via deleteLater and nothing pumps the loop here.
    MW *window = new MW();
    InfoBarProxy *footer = documentStatusFooter(window);
    QVERIFY2(footer != nullptr, "The windowed status footer should exist.");
    // It pins its own height in its constructor, which is what makes the chrome
    // estimate exact while the window is hidden.
    const int footerHeight = footer->minimumHeight();
    QVERIFY2(footerHeight > 0, "The status footer should carry a fixed height.");
    QCOMPARE(footer->maximumHeight(), footerHeight);

    const QSize image = imageSizeForThisScreen();
    QVERIFY2(image.width() > 4 && image.height() > footerHeight + 4,
             "This check needs a screen big enough for a non-degenerate image.");

    window->showImage(std::make_unique<QPixmap>(image));

    QVERIFY2(!window->isVisible(), "The window must still be hidden - that is the case under test.");
    // The viewport, not the window, is what gets the image: the footer's height
    // has to be on top of it.
    QCOMPARE(settings->windowGeometry().size(),
             QSize(image.width(), image.height() + footerHeight));
}

// The other side of the same rule: nothing may be added when there is no chrome,
// otherwise the fix would just move the empty bars to the other axis.
void StartupAutoResizeChromeTest::hiddenWindowPersistsJustTheImageWhenTheFooterIsOff() {
    settings->setInfoBarWindowed(false);

    MW *window = new MW();
    const QSize image = imageSizeForThisScreen();
    QVERIFY2(image.width() > 4 && image.height() > 4,
             "This check needs a screen big enough for a non-degenerate image.");

    window->showImage(std::make_unique<QPixmap>(image));

    QVERIFY2(!window->isVisible(), "The window must still be hidden - that is the case under test.");
    QCOMPARE(settings->windowGeometry().size(), image);
}

// Auto-resize off means auto-resize off: opening an image must not touch the
// remembered geometry at all.
void StartupAutoResizeChromeTest::autoResizeOffLeavesThePersistedGeometryAlone() {
    settings->setInfoBarWindowed(true);
    settings->setAutoResizeWindow(false);
    const QRect remembered(120, 140, 803, 607);
    settings->setWindowGeometry(remembered);

    MW *window = new MW();
    window->showImage(std::make_unique<QPixmap>(imageSizeForThisScreen()));

    QVERIFY2(!window->isVisible(), "The window must still be hidden - that is the case under test.");
    QCOMPARE(settings->windowGeometry(), remembered);
}

TG_BEHAVIOR_TEST_MAIN(StartupAutoResizeChromeTest)

#include "test_startup_auto_resize_accounts_for_chrome.moc"
