#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QScrollBar>
#include <QSpontaneKeyEvent>
#include <QTemporaryDir>
#include <QWheelEvent>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// The grid caches its wheel settings; turning smooth scrolling off at runtime
// must take effect on the next wheel notch, without recreating the view.
class WheelScrollSettingChangeAppliesWithoutRestartTest : public QObject {
    Q_OBJECT

private slots:
    void turningSmoothScrollOffTakesEffectImmediately();
};

namespace {

void wheelDown(QWidget *target) {
    const QPointF local = QRectF(target->rect()).center();
    QWheelEvent event(local, target->mapToGlobal(local), QPoint(0, 0), QPoint(0, -120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QSpontaneKeyEvent::setSpontaneous(&event);
    qApp->notify(target, &event);
}

} // namespace

void WheelScrollSettingChangeAppliesWithoutRestartTest::turningSmoothScrollOffTakesEffectImmediately() {
    settings->setTrackpadDetection(false);
    settings->setEnableSmoothScroll(true);
    settings->sendChangeNotification();

    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    for(int i = 0; i < 200; ++i)
        QVERIFY2(tgtest::writeImage(root.filePath(QString("gallery/img%1.png").arg(i, 3, 10, QChar('0'))), Qt::red),
                 "Gallery image should be written.");

    Core core;
    QVERIFY2(core.loadPath(root.filePath("gallery")), "Opening the gallery folder should succeed.");
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    auto grid = tgtest::mainWindow()->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");
    QTRY_VERIFY2(grid->verticalScrollBar()->maximum() > 0, "The grid should be scrollable.");

    // smooth scrolling animates, so the scrollbar has not moved yet
    wheelDown(grid->viewport());
    QCOMPARE(grid->verticalScrollBar()->value(), 0);
    QTest::qWait(400);
    QVERIFY2(grid->verticalScrollBar()->value() > 0, "The smooth scroll should land after its animation.");
    grid->verticalScrollBar()->setValue(0);

    settings->setEnableSmoothScroll(false);
    settings->sendChangeNotification();

    // without smooth scrolling the notch scrolls at once
    wheelDown(grid->viewport());
    QVERIFY2(grid->verticalScrollBar()->value() > 0, "The new setting should apply without a restart.");
}

TG_BEHAVIOR_TEST_MAIN(WheelScrollSettingChangeAppliesWithoutRestartTest)

#include "test_wheel_scroll_setting_change_applies_without_restart.moc"
