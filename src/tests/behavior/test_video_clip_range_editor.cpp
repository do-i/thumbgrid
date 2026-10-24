#include "support/thumbgrid_test_support.h"

#include <QHBoxLayout>
#include <QSignalSpy>

#include "components/actionmanager/actionmanager.h"
#include "gui/customwidgets/actionbutton.h"
#include "gui/customwidgets/videoslider.h"
#include "gui/overlays/videocontrols.h"
#include "gui/viewers/videoclip.h"

// The A-B button became a clip editor: the bracket button puts two pentagon
// handles on the seek bar (30% and 70% of the video to start with), dragging
// them sets the clip, and pressing the button again commits it; playback then
// stays inside the clip, and the loop button repeats it (or, with no clip,
// the whole file). The handles never cross.

class VideoClipRangeEditorTest : public QObject {
    Q_OBJECT

private slots:
    void editingStartsAtThirtyAndSeventyPercent();
    void theHandlesNeverCross();
    void cancelRestoresTheClipAndCommitKeepsIt();
    void markingWithoutAClipMakesOne();
    void anActiveClipKeepsPlaybackInside();
    void anotherFileClearsTheClipAndTheLoop();
    void theSliderDrawsPentagonsPointingInward();
    void draggingAHandleStopsShortOfTheOther();
    void narrowClipHandlesDoNotOverlap();
    void theClipAndLoopButtonsShowTheState();
    void rightClickOnTheClipButtonClearsTheClip();
    void theClipKeysAreBoundInEveryContextTheyNeed();
    void escapeCancelsEditingOnlyWhileEditing();
};

namespace {
constexpr int TEN_MINUTES = 10 * 60 * 1000;

struct Bar {
    QWidget row;
    VideoControls *controls;
    VideoSlider *seekBar = nullptr;
    ActionButton *clipButton = nullptr;
    ActionButton *loopButton = nullptr;

    Bar() : controls(new VideoControls(&row)) {
        settings->loadStylesheet();
        row.setAccessibleName("VideoControlsRow");
        auto *layout = new QHBoxLayout(&row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(controls);
        controls->setMode(PLAYBACK_VIDEO);
        controls->setPlaybackDuration(TEN_MINUTES);
        controls->show();
        row.resize(640, 26);
        row.show();
        seekBar = controls->findChild<VideoSlider *>("seekBar");
        clipButton = dynamic_cast<ActionButton *>(controls->findChild<QWidget *>("clipButton"));
        loopButton = dynamic_cast<ActionButton *>(controls->findChild<QWidget *>("loopButton"));
    }
};

QPoint centerOf(const QPolygonF &shape) {
    return shape.boundingRect().center().toPoint();
}
}

void VideoClipRangeEditorTest::editingStartsAtThirtyAndSeventyPercent() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    QVERIFY(!clip.hasRange());
    clip.toggleEdit();
    QVERIFY(clip.isEditing());
    QCOMPARE(clip.start(), 3 * 60 * 1000);
    QCOMPARE(clip.end(), 7 * 60 * 1000);
    clip.toggleEdit();
    QVERIFY(clip.isActive());
}

void VideoClipRangeEditorTest::theHandlesNeverCross() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    clip.toggleEdit();
    clip.moveHandle(VideoClip::HANDLE_START, TEN_MINUTES);
    QCOMPARE(clip.start(), clip.end() - VideoClip::MIN_GAP_MS);
    clip.moveHandle(VideoClip::HANDLE_END, 0);
    QCOMPARE(clip.end(), clip.start() + VideoClip::MIN_GAP_MS);
    QVERIFY(clip.start() < clip.end());
    // the marking keys are held back the same way
    clip.markStart(TEN_MINUTES);
    QVERIFY(clip.start() < clip.end());
    clip.markEnd(-5);
    QVERIFY(clip.end() > clip.start());
    // and nothing leaves the video
    clip.moveHandle(VideoClip::HANDLE_START, -1000);
    QCOMPARE(clip.start(), 0);
    clip.moveHandle(VideoClip::HANDLE_END, TEN_MINUTES * 2);
    QCOMPARE(clip.end(), TEN_MINUTES);
}

void VideoClipRangeEditorTest::cancelRestoresTheClipAndCommitKeepsIt() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    clip.toggleEdit();
    clip.cancelEdit();
    QVERIFY2(!clip.hasRange(), "Cancelling a new clip leaves no clip.");

    clip.toggleEdit();
    clip.moveHandle(VideoClip::HANDLE_START, 1000);
    clip.commitEdit();
    QVERIFY(clip.isActive());
    QCOMPARE(clip.start(), 1000);

    clip.toggleEdit();
    clip.moveHandle(VideoClip::HANDLE_START, 2000);
    clip.cancelEdit();
    QVERIFY(clip.isActive());
    QCOMPARE(clip.start(), 1000);

    clip.clear();
    QVERIFY(!clip.hasRange());
}

void VideoClipRangeEditorTest::markingWithoutAClipMakesOne() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    clip.markStart(60000);
    QVERIFY(clip.isActive());
    QCOMPARE(clip.start(), 60000);
    QCOMPARE(clip.end(), TEN_MINUTES);

    clip.clear();
    clip.markEnd(90000);
    QVERIFY(clip.isActive());
    QCOMPARE(clip.start(), 0);
    QCOMPARE(clip.end(), 90000);
    QCOMPARE(clip.lastHandle(), VideoClip::HANDLE_END);
}

void VideoClipRangeEditorTest::anActiveClipKeepsPlaybackInside() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    // no clip: seeks go anywhere, play resumes in place
    QCOMPARE(clip.seekTarget(12345), 12345);
    QCOMPARE(clip.resumeTarget(12345), -1);
    QVERIFY(!clip.reachedEnd(TEN_MINUTES));

    clip.toggleEdit();
    // while editing nothing is held back yet
    QCOMPARE(clip.seekTarget(0), 0);
    clip.commitEdit();
    const int start = clip.start(), end = clip.end();
    QCOMPARE(clip.seekTarget(0), start);
    QCOMPARE(clip.seekTarget(TEN_MINUTES), end);
    QCOMPARE(clip.seekTarget(start + 5), start + 5);
    // play resumes mid-clip, restarts from outside or at the end
    QCOMPARE(clip.resumeTarget(start + 1000), -1);
    QCOMPARE(clip.resumeTarget(start - 1), start);
    QCOMPARE(clip.resumeTarget(end), start);
    // without loop playback stops at the end; with loop it goes on
    QVERIFY(!clip.reachedEnd(end - 1000));
    QVERIFY(clip.reachedEnd(end));
    clip.setLoop(true);
    QVERIFY(!clip.reachedEnd(end));
}

void VideoClipRangeEditorTest::anotherFileClearsTheClipAndTheLoop() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    QVERIFY2(!clip.loop(), "Loop is off to begin with.");
    clip.toggleEdit();
    clip.commitEdit();
    clip.toggleLoop();
    QVERIFY(clip.loop());
    QSignalSpy changed(&clip, &VideoClip::changed);
    clip.reset();
    QVERIFY(!clip.hasRange());
    QVERIFY(!clip.loop());
    QCOMPARE(changed.count(), 1);
    // a new length also drops a clip
    clip.markStart(1000);
    clip.setDuration(TEN_MINUTES / 2);
    QVERIFY(!clip.hasRange());
}

void VideoClipRangeEditorTest::theSliderDrawsPentagonsPointingInward() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    QVERIFY(bar.seekBar);
    bar.seekBar->setClip(180000, 420000, true);

    const QPolygonF start = bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_START);
    const QPolygonF end = bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_END);
    QCOMPARE(start.size(), 5);
    QCOMPARE(end.size(), 5);
    const int startX = bar.seekBar->xForValue(180000);
    const int endX = bar.seekBar->xForValue(420000);
    // flat edge on the position, tip pointing into the clip
    QCOMPARE(start.boundingRect().left(), qreal(startX));
    QCOMPARE(end.boundingRect().right(), qreal(endX));
    QCOMPARE(start.at(2).x(), start.boundingRect().right());
    QCOMPARE(end.at(2).x(), end.boundingRect().left());

    if(qEnvironmentVariableIsSet("TG_GRAB_DIR")) {
        const QString dir = qEnvironmentVariable("TG_GRAB_DIR");
        bar.controls->setPlaybackPosition(250000);
        bar.controls->setClip(180000, 420000, true);
        bar.row.grab().save(dir + "/clip-editing.png");
        bar.controls->setClip(180000, 420000, false);
        bar.controls->setLoop(true);
        bar.row.grab().save(dir + "/clip-active.png");
        bar.controls->setClip(-1, -1, false);
        bar.controls->setLoop(false);
        bar.row.grab().save(dir + "/clip-none.png");
    }
}

void VideoClipRangeEditorTest::draggingAHandleStopsShortOfTheOther() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    bar.seekBar->setClipMinGap(VideoClip::MIN_GAP_MS);
    bar.seekBar->setClip(180000, 420000, true);
    QSignalSpy moved(bar.seekBar, &VideoSlider::clipHandleMoved);
    QSignalSpy seeks(bar.seekBar, &VideoSlider::sliderMovedX);

    const QPoint grab = centerOf(bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_START));
    QTest::mousePress(bar.seekBar, Qt::LeftButton, Qt::NoModifier, grab);
    for(int x = grab.x(); x < bar.seekBar->width(); x += 20)
        QTest::mouseMove(bar.seekBar, QPoint(x, grab.y()));
    QTest::mouseRelease(bar.seekBar, Qt::LeftButton, Qt::NoModifier, QPoint(bar.seekBar->width() - 1, grab.y()));

    QVERIFY(!moved.isEmpty());
    for(const auto &args : moved) {
        QCOMPARE(args.at(0).toInt(), int(VideoSlider::CLIP_HANDLE_START));
        QVERIFY2(args.at(1).toInt() <= 420000 - VideoClip::MIN_GAP_MS,
                 qPrintable(QString("start handle dragged to %1").arg(args.at(1).toInt())));
    }
    QCOMPARE(bar.seekBar->clipStart(), 420000 - VideoClip::MIN_GAP_MS);
    QVERIFY2(seeks.isEmpty(), "Dragging a handle is not a seek.");

    // a press away from the handles still seeks
    QTest::mouseClick(bar.seekBar, Qt::LeftButton, Qt::NoModifier, QPoint(5, grab.y()));
    QCOMPARE(seeks.count(), 1);
}

void VideoClipRangeEditorTest::narrowClipHandlesDoNotOverlap() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    bar.seekBar->setClip(300000, 300000 + VideoClip::MIN_GAP_MS, true);
    const QRectF start = bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_START).boundingRect();
    const QRectF end = bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_END).boundingRect();
    QVERIFY2(start.right() <= end.left(),
             qPrintable(QString("start ends at %1, end begins at %2").arg(start.right()).arg(end.left())));
    // and both stay full size, so they can still be grabbed
    bar.seekBar->setClip(100000, 500000, true);
    QCOMPARE(start.width(), bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_START).boundingRect().width());
    QCOMPARE(end.width(), bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_END).boundingRect().width());
}

void VideoClipRangeEditorTest::theClipAndLoopButtonsShowTheState() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    QVERIFY(bar.clipButton);
    QVERIFY(bar.loopButton);
    QVERIFY(bar.clipButton->isVisible());
    QVERIFY(bar.loopButton->isVisible());
    QVERIFY(!bar.clipButton->property("checked").toBool());
    QVERIFY(!bar.loopButton->property("checked").toBool());

    bar.controls->setClip(1000, 2000, true);
    QVERIFY(bar.clipButton->property("checked").toBool());
    QVERIFY(bar.seekBar->clipEditable());
    bar.controls->setClip(1000, 2000, false);
    QVERIFY(bar.clipButton->property("checked").toBool());
    QVERIFY(!bar.seekBar->clipEditable());
    QCOMPARE(bar.seekBar->clipStart(), 1000);
    bar.controls->setClip(-1, -1, false);
    QVERIFY(!bar.clipButton->property("checked").toBool());
    QCOMPARE(bar.seekBar->clipStart(), -1);

    bar.controls->setLoop(true);
    QVERIFY(bar.loopButton->property("checked").toBool());

    // an animation has neither
    bar.controls->setMode(PLAYBACK_ANIMATION);
    QVERIFY(!bar.clipButton->isVisible());
    QVERIFY(!bar.loopButton->isVisible());
}

void VideoClipRangeEditorTest::rightClickOnTheClipButtonClearsTheClip() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    QSignalSpy clear(actionManager, &ActionManager::clearClip);
    QSignalSpy edit(actionManager, &ActionManager::toggleClipEdit);
    QTest::mouseClick(bar.clipButton, Qt::RightButton);
    QCOMPARE(clear.count(), 1);
    QCOMPARE(edit.count(), 0);
    QTest::mouseClick(bar.clipButton, Qt::LeftButton);
    QCOMPARE(edit.count(), 1);

    QSignalSpy loop(actionManager, &ActionManager::toggleVideoLoop);
    QTest::mouseClick(bar.loopButton, Qt::LeftButton);
    QCOMPARE(loop.count(), 1);
}

void VideoClipRangeEditorTest::theClipKeysAreBoundInEveryContextTheyNeed() {
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "["), QStringLiteral("setClipStart"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "]"), QStringLiteral("setClipEnd"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "\\"), QStringLiteral("toggleVideoLoop"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "|"), QStringLiteral("clearClip"));
    QCOMPARE(actionManager->actionForShortcut(MODE_CLIPEDIT, "Esc"), QStringLiteral("cancelClipEdit"));
    QCOMPARE(actionManager->actionForShortcut(MODE_CLIPEDIT, "Enter"), QStringLiteral("commitClipEdit"));
    // global keys don't reach into the clip edit context
    QVERIFY(actionManager->actionForShortcut(MODE_CLIPEDIT, "[").isEmpty());
}

void VideoClipRangeEditorTest::escapeCancelsEditingOnlyWhileEditing() {
    QSignalSpy cancel(actionManager, &ActionManager::cancelClipEdit);
    QSignalSpy exit(actionManager, &ActionManager::closeFullScreenOrExit);
    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);

    actionManager->setOverlayContext(MODE_CLIPEDIT);
    actionManager->processEvent(&esc);
    QCOMPARE(cancel.count(), 1);
    QCOMPARE(exit.count(), 0);
    // keys the context doesn't bind work as usual
    QSignalSpy loop(actionManager, &ActionManager::toggleVideoLoop);
    QKeyEvent backslash(QEvent::KeyPress, Qt::Key_Backslash, Qt::NoModifier, "\\");
    actionManager->processEvent(&backslash);
    QCOMPARE(loop.count(), 1);

    actionManager->clearOverlayContext();
    actionManager->processEvent(&esc);
    QCOMPARE(cancel.count(), 1);
    QCOMPARE(exit.count(), 1);
}

TG_BEHAVIOR_TEST_MAIN(VideoClipRangeEditorTest)

#include "test_video_clip_range_editor.moc"
