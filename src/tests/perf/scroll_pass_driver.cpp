#include "support/thumbgrid_test_support.h"

#include <QElapsedTimer>
#include <QScrollBar>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// Perf driver, not a test: opens THUMBGRID_PERF_FIXTURE, then steps the grid's
// vertical scrollbar through its full range in fixed increments, processing
// events after each step. Prints "scroll_pass_ms=<total> steps=<n> per_step_ms=<avg>".
// Built only on request (scripts/perf-baseline.sh), never registered with ctest.
class ScrollPassDriver : public QObject {
    Q_OBJECT
private slots:
    void run();
};

void ScrollPassDriver::run() {
    const QString fixture = qEnvironmentVariable("THUMBGRID_PERF_FIXTURE");
    QVERIFY2(!fixture.isEmpty(), "Set THUMBGRID_PERF_FIXTURE.");

    Core core;
    QVERIFY(core.loadPath(fixture));
    core.showGui();
    QTRY_VERIFY(tgtest::mainWindow() && tgtest::mainWindow()->isVisible());
    MW *window = tgtest::mainWindow();
    window->resize(1280, 800);
    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY(grid != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(grid->itemCount() >= 2000, 20000);
    QTest::qWait(1000); // let the first visible thumbnails settle

    QScrollBar *bar = grid->verticalScrollBar();
    const int maximum = bar->maximum();
    QVERIFY2(maximum > 0, "The grid should be scrollable.");
    constexpr int stepPx = 120;

    QElapsedTimer timer;
    timer.start();
    int steps = 0;
    for(int pos = 0; pos <= maximum; pos += stepPx, steps++) {
        bar->setValue(pos);
        QCoreApplication::processEvents();
    }
    const double total = timer.nsecsElapsed() / 1e6;
    printf("scroll_pass_ms=%.3f steps=%d per_step_ms=%.4f\n", total, steps, total / steps);
    fflush(stdout);
}

TG_BEHAVIOR_TEST_MAIN(ScrollPassDriver)

#include "scroll_pass_driver.moc"
