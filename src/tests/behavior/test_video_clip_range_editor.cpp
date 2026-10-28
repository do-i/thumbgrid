#include "support/thumbgrid_test_support.h"

#include <QHBoxLayout>
#include <QSignalSpy>

#include "components/actionmanager/actionmanager.h"
#include "gui/customwidgets/actionbutton.h"
#include "gui/customwidgets/videoslider.h"
#include "gui/overlays/videocontrols.h"
#include "gui/viewers/videoclip.h"

// The clip button shows two pentagon handles on the seek bar (the start at the
// playhead, the end halfway from there to the end of the video, at least 1 s
// later) and playback stays between them; they can be dragged whenever they
// are shown. Pressing it again hides them and plays the whole video, keeping
// the range for when they are shown again (its start at the playhead, if that
// moved). The loop button repeats the clip (or, with no clip, the whole file).
// The handles never cross, and a dragged handle stands in for the playhead.

class VideoClipRangeEditorTest : public QObject {
    Q_OBJECT

private slots:
    void aNewClipStartsAtThePlayhead();
    void aNewClipNearTheEndSpansTheWholeVideo();
    void theHandlesNeverCross();
    void hidingTheClipKeepsItsRange();
    void markingWithoutAClipMakesOne();
    void anActiveClipKeepsPlaybackInside();
    void anotherFileClearsTheClipAndTheLoop();
    void theSliderDrawsPentagonsPointingInward();
    void draggingAHandleStopsShortOfTheOther();
    void aDraggedHandleHidesThePlayheadKnob();
    void narrowClipHandlesDoNotOverlap();
    void theClipAndLoopButtonsShowTheState();
    void rightClickOnTheClipButtonClearsTheClip();
    void theClipKeysAreBound();
};

namespace {
constexpr int TEN_MINUTES = 10 * 60 * 1000;
constexpr int PLAYHEAD = 3 * 60 * 1000;

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

void VideoClipRangeEditorTest::aNewClipStartsAtThePlayhead() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    QVERIFY(!clip.hasRange());
    clip.toggle(PLAYHEAD);
    QVERIFY(clip.isActive());
    // the end halfway from the playhead to the end of the video
    QCOMPARE(clip.start(), PLAYHEAD);
    QCOMPARE(clip.end(), PLAYHEAD + (TEN_MINUTES - PLAYHEAD) / 2);

    // from the very start: the first half
    clip.clear();
    clip.toggle(0);
    QCOMPARE(clip.start(), 0);
    QCOMPARE(clip.end(), TEN_MINUTES / 2);

    // with less than 2 s left, half of it is too short: 1 s
    clip.clear();
    const int late = TEN_MINUTES - 1500;
    clip.toggle(late);
    QCOMPARE(clip.start(), late);
    QCOMPARE(clip.end(), late + VideoClip::NEW_CLIP_MIN_MS);
}

void VideoClipRangeEditorTest::aNewClipNearTheEndSpansTheWholeVideo() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    for(const int pos : {TEN_MINUTES, TEN_MINUTES - 400, TEN_MINUTES - VideoClip::NEW_CLIP_MIN_MS}) {
        clip.clear();
        clip.toggle(pos);
        QVERIFY(clip.isActive());
        QCOMPARE(clip.start(), 0);
        QCOMPARE(clip.end(), TEN_MINUTES);
    }
    // a video shorter than that is all clip wherever the playhead is
    clip.clear();
    clip.setDuration(800);
    clip.toggle(100);
    QCOMPARE(clip.start(), 0);
    QCOMPARE(clip.end(), 800);
}

void VideoClipRangeEditorTest::theHandlesNeverCross() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    clip.toggle(PLAYHEAD);
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

void VideoClipRangeEditorTest::hidingTheClipKeepsItsRange() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    clip.toggle(PLAYHEAD);
    clip.moveHandle(VideoClip::HANDLE_START, 1000);
    clip.moveHandle(VideoClip::HANDLE_END, 5000);

    // hidden: the whole video plays, the range waits
    clip.toggle(3000);
    QVERIFY(!clip.isActive());
    QVERIFY(clip.hasRange());
    QCOMPARE(clip.seekTarget(TEN_MINUTES), TEN_MINUTES);
    QCOMPARE(clip.resumeTarget(TEN_MINUTES - 1000), -1);
    QVERIFY(!clip.reachedEnd(5000));
    // and its handles can't be moved
    clip.moveHandle(VideoClip::HANDLE_START, 2000);
    QCOMPARE(clip.start(), 1000);

    // shown again with the playhead where it was hidden: the same range
    clip.toggle(3000 + VideoClip::PLAYHEAD_MOVED_MS);
    QVERIFY(clip.isActive());
    QCOMPARE(clip.start(), 1000);
    QCOMPARE(clip.end(), 5000);

    // the playhead moved meanwhile: the start comes to it, the end stays
    clip.toggle(3000);
    clip.toggle(2000);
    QCOMPARE(clip.start(), 2000);
    QCOMPARE(clip.end(), 5000);
    // also before the clip
    clip.toggle(2000);
    clip.toggle(500);
    QCOMPARE(clip.start(), 500);
    QCOMPARE(clip.end(), 5000);
    // less than NEW_CLIP_MIN_MS before the end, or past it: a new clip there
    clip.toggle(500);
    clip.toggle(4500);
    QCOMPARE(clip.start(), 4500);
    QCOMPARE(clip.end(), 4500 + (TEN_MINUTES - 4500) / 2);
    const int end = clip.end();
    clip.toggle(4500);
    clip.toggle(end + 1000);
    QCOMPARE(clip.start(), end + 1000);
    QCOMPARE(clip.end(), end + 1000 + (TEN_MINUTES - end - 1000) / 2);

    // a marking key shows a hidden clip, keeping its other end
    const int markedEnd = clip.end();
    clip.toggle(0);
    clip.markStart(5000);
    QVERIFY(clip.isActive());
    QCOMPARE(clip.start(), 5000);
    QCOMPARE(clip.end(), markedEnd);

    // clearing forgets it: the next clip starts at the playhead
    clip.toggle(0);
    clip.clear();
    QVERIFY(!clip.hasRange());
    clip.toggle(PLAYHEAD);
    QCOMPARE(clip.start(), PLAYHEAD);
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
}

void VideoClipRangeEditorTest::anActiveClipKeepsPlaybackInside() {
    VideoClip clip;
    clip.setDuration(TEN_MINUTES);
    // no clip: seeks go anywhere, play resumes in place
    QCOMPARE(clip.seekTarget(12345), 12345);
    QCOMPARE(clip.resumeTarget(12345), -1);
    QVERIFY(!clip.reachedEnd(TEN_MINUTES));

    clip.toggle(PLAYHEAD);
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
    clip.toggle(PLAYHEAD);
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
    bar.seekBar->setClip(180000, 420000);

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
        bar.controls->setClip(180000, 420000);
        bar.row.grab().save(dir + "/clip-on.png");
        bar.controls->setLoop(true);
        bar.row.grab().save(dir + "/clip-active.png");
        bar.controls->setClip(-1, -1);
        bar.controls->setLoop(false);
        bar.row.grab().save(dir + "/clip-none.png");
    }
}

void VideoClipRangeEditorTest::draggingAHandleStopsShortOfTheOther() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    bar.seekBar->setClipMinGap(VideoClip::MIN_GAP_MS);
    bar.seekBar->setClip(180000, 420000);
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

// The handle's frame is on the video while it's dragged: the playhead knob
// goes away rather than trailing beside it, and comes back on release.
void VideoClipRangeEditorTest::aDraggedHandleHidesThePlayheadKnob() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    bar.controls->setPlaybackPosition(300000);
    bar.controls->setClip(180000, 420000);
    const int knobX = bar.seekBar->xForValue(300000);
    const QRect knobArea(knobX - 4, 0, 9, bar.seekBar->height());
    auto knobPixels = [&]() { return bar.seekBar->grab().toImage().copy(knobArea); };
    const QImage idle = knobPixels();

    const QPoint grab = centerOf(bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_START));
    QTest::mousePress(bar.seekBar, Qt::LeftButton, Qt::NoModifier, grab);
    QVERIFY(bar.seekBar->draggingClipHandle());
    QVERIFY2(knobPixels() != idle, "The playhead knob should not be drawn while a handle is dragged.");
    QTest::mouseRelease(bar.seekBar, Qt::LeftButton, Qt::NoModifier, grab);
    QVERIFY(!bar.seekBar->draggingClipHandle());
    QCOMPARE(knobPixels(), idle);
}

void VideoClipRangeEditorTest::narrowClipHandlesDoNotOverlap() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    bar.seekBar->setClip(300000, 300000 + VideoClip::MIN_GAP_MS);
    const QRectF start = bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_START).boundingRect();
    const QRectF end = bar.seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_END).boundingRect();
    QVERIFY2(start.right() <= end.left(),
             qPrintable(QString("start ends at %1, end begins at %2").arg(start.right()).arg(end.left())));
    // and both stay full size, so they can still be grabbed
    bar.seekBar->setClip(100000, 500000);
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

    bar.controls->setClip(1000, 2000);
    QVERIFY(bar.clipButton->property("checked").toBool());
    QCOMPARE(bar.seekBar->clipStart(), 1000);
    bar.controls->setClip(-1, -1);
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
    QSignalSpy edit(actionManager, &ActionManager::toggleClip);
    QTest::mouseClick(bar.clipButton, Qt::RightButton);
    QCOMPARE(clear.count(), 1);
    QCOMPARE(edit.count(), 0);
    QTest::mouseClick(bar.clipButton, Qt::LeftButton);
    QCOMPARE(edit.count(), 1);

    QSignalSpy loop(actionManager, &ActionManager::toggleVideoLoop);
    QTest::mouseClick(bar.loopButton, Qt::LeftButton);
    QCOMPARE(loop.count(), 1);
}

void VideoClipRangeEditorTest::theClipKeysAreBound() {
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "["), QStringLiteral("setClipStart"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "]"), QStringLiteral("setClipEnd"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "\\"), QStringLiteral("toggleVideoLoop"));
    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "|"), QStringLiteral("clearClip"));
}

TG_BEHAVIOR_TEST_MAIN(VideoClipRangeEditorTest)

#include "test_video_clip_range_editor.moc"
