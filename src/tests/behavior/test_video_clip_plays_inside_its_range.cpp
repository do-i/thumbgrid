#include "support/thumbgrid_test_support.h"

#include <climits>
#include <clocale>

#include <QDir>
#include <QElapsedTimer>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core.h"
#include "components/actionmanager/actionmanager.h"
#include "gui/customwidgets/videoslider.h"
#include "gui/mainwindow.h"
#include "gui/viewers/videoplayerinitproxy.h"
#include "gui/viewers/viewerwidget.h"

// A real video in mpv: positions are milliseconds, a committed clip (30% ..
// 70% to begin with) plays from its start and stops at its end, loop repeats
// it, and without a clip a video that doesn't loop stops on its last frame and
// plays again from the start (and reports the end, for the slideshow). While
// the clip is edited, the video shows the frame of the handle being moved.

class VideoClipPlaysInsideItsRangeTest : public QObject {
    Q_OBJECT

private slots:
    void theClipPlaysStopsAndLoopsInsideItsRange();
};

void VideoClipPlaysInsideItsRangeTest::theClipPlaysStopsAndLoopsInsideItsRange() {
    // mpv embeds into a native X window. Offscreen has none, and mpv aborts on
    // its fake one unless there is no display at all (then it plays audio only,
    // which is all this needs): run with QT_QPA_PLATFORM=xcb on a virtual
    // display, or offscreen with DISPLAY and WAYLAND_DISPLAY unset.
    const bool xcb = QGuiApplication::platformName() == QLatin1String("xcb");
    if(!xcb && (qEnvironmentVariableIsSet("DISPLAY") || qEnvironmentVariableIsSet("WAYLAND_DISPLAY")))
        QSKIP("Needs QT_QPA_PLATFORM=xcb, or offscreen with DISPLAY/WAYLAND_DISPLAY unset.");
    const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if(ffmpeg.isEmpty())
        QSKIP("ffmpeg is needed to make the test clip.");

    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    const QString clip = QDir(fixture.path()).filePath("clip.mp4");
    // 6 s; a quiet low tone so it plays without a video output (and the
    // player is muted anyway)
    const int rc = QProcess::execute(ffmpeg, {"-v", "error",
                                              "-f", "lavfi", "-i", "testsrc=duration=6:size=160x120:rate=10",
                                              "-f", "lavfi", "-i", "sine=frequency=220:duration=6",
                                              "-af", "volume=0.05", "-pix_fmt", "yuv420p", "-shortest", clip});
    if(rc != 0 || !QFile::exists(clip))
        QSKIP("ffmpeg could not make the test clip.");

    settings->setPlayVideoSounds(false);
    // mpv refuses to start under a non-C numeric locale (main() sets this too)
    std::setlocale(LC_NUMERIC, "C");

    Core core;
    QVERIFY(core.loadPath(clip));
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    auto viewerWidget = window->findChild<ViewerWidget *>();
    QVERIFY(viewerWidget);
    QTest::qWait(1500);
    VideoPlayerInitProxy *player = nullptr;
    for(VideoPlayer *candidate : viewerWidget->findChildren<VideoPlayer *>())
        player = player ? player : dynamic_cast<VideoPlayerInitProxy *>(candidate);
    // e.g. only an installed player plugin for another interface version
    if(!viewerWidget->isShowingVideo() || !player || !player->isInitialized())
        QSKIP("The video player is not available in this environment.");
    int position = -1;
    bool paused = false;
    connect(player, &VideoPlayer::positionChanged, this, [&](int pos) { position = pos; });
    connect(player, &VideoPlayer::videoPaused, this, [&](bool mode) { paused = mode; });
    QTRY_VERIFY_WITH_TIMEOUT(position > 0, 8000);
    // the file is loaded by now; its length is the seek bar's
    auto seekBar = window->findChild<VideoSlider *>("seekBar");
    QVERIFY(seekBar);
    const int duration = seekBar->maximum();
    QVERIFY2(duration > 5500 && duration < 6500, qPrintable(QString("duration %1 should be ~6000 ms").arg(duration)));

    // edit: the handles start at 30% and 70%; committing goes to the start
    actionManager->invokeAction("toggleClipEdit");
    QVERIFY(seekBar->clipEditable());
    const int start = seekBar->clipStart(), end = seekBar->clipEnd();
    QCOMPARE(start, duration * 3 / 10);
    QCOMPARE(end, duration * 7 / 10);
    actionManager->invokeAction("toggleClipEdit");
    QVERIFY(!seekBar->clipEditable());
    QCOMPARE(seekBar->clipStart(), start);

    // play stops at the clip's end
    if(!paused)
        actionManager->invokeAction("pauseVideo");
    QTRY_VERIFY(paused);
    QTRY_VERIFY2(qAbs(position - start) < 300, qPrintable(QString("at %1, clip starts at %2").arg(position).arg(start)));
    actionManager->invokeAction("pauseVideo");
    QTRY_VERIFY(!paused);
    QTRY_VERIFY_WITH_TIMEOUT(paused, 8000);
    QTRY_VERIFY2(qAbs(position - end) < 300, qPrintable(QString("stopped at %1, clip ends at %2").arg(position).arg(end)));

    // seeking outside the clip lands inside it
    viewerWidget->seek(0);
    QTRY_VERIFY2(qAbs(position - start) < 300, qPrintable(QString("seek to 0 landed at %1").arg(position)));

    // loop: play from the end goes back to the start and keeps repeating
    viewerWidget->seek(end);
    actionManager->invokeAction("toggleVideoLoop");
    actionManager->invokeAction("pauseVideo");
    QTRY_VERIFY(!paused);
    int lowest = INT_MAX, highest = 0, wraps = 0, last = position;
    QElapsedTimer watch;
    watch.start();
    while(watch.elapsed() < 6000) {
        QTest::qWait(50);
        if(position < last - 500)
            wraps++;
        last = position;
        lowest = qMin(lowest, position);
        highest = qMax(highest, position);
    }
    QVERIFY2(!paused, "A looping clip keeps playing.");
    QVERIFY2(wraps >= 1, "The clip should repeat.");
    QVERIFY2(lowest >= start - 300 && highest <= end + 300,
             qPrintable(QString("played %1..%2, clip is %3..%4").arg(lowest).arg(highest).arg(start).arg(end)));

    // no clip, loop off: stops on the last frame, then plays from the start
    actionManager->invokeAction("clearClip");
    QCOMPARE(seekBar->clipStart(), -1);
    actionManager->invokeAction("toggleVideoLoop");
    QSignalSpy finished(viewerWidget, &ViewerWidget::playbackFinished);
    viewerWidget->seek(duration - 700);
    QTRY_VERIFY_WITH_TIMEOUT(paused, 8000);
    QVERIFY2(viewerWidget->isShowingVideo(), "The video stays open at its end.");
    // the slideshow moves on when this comes
    QTRY_VERIFY2(!finished.isEmpty(), "The end of the video should be reported.");
    QVERIFY2(position > duration - 500, qPrintable(QString("stopped at %1 of %2").arg(position).arg(duration)));
    actionManager->invokeAction("pauseVideo");
    QTRY_VERIFY(!paused);
    QTRY_VERIFY2(position < 1500, qPrintable(QString("playing again from %1").arg(position)));

    // editing: grabbing a handle pauses on its frame, dragging shows the
    // frame it is moved to, a frame step moves it a frame, Esc cancels
    actionManager->invokeAction("toggleClipEdit");
    QVERIFY(seekBar->clipEditable());
    const QPoint grab = seekBar->clipHandleShape(VideoSlider::CLIP_HANDLE_START).boundingRect().center().toPoint();
    const QPoint dropped = grab - QPoint(qMax(10, seekBar->width() / 10), 0);
    QTest::mousePress(seekBar, Qt::LeftButton, Qt::NoModifier, grab);
    QTest::mouseMove(seekBar, dropped);
    QTest::mouseRelease(seekBar, Qt::LeftButton, Qt::NoModifier, dropped);
    const int dragged = seekBar->clipStart();
    QVERIFY2(dragged < start, qPrintable(QString("start handle dragged to %1").arg(dragged)));
    QTRY_VERIFY(paused);
    QTRY_VERIFY2(qAbs(position - dragged) < 150, qPrintable(QString("showing %1, handle at %2").arg(position).arg(dragged)));
    actionManager->invokeAction("frameStep");
    QTRY_VERIFY2(seekBar->clipStart() > dragged, "A frame step moves the handle.");
    QVERIFY2(seekBar->clipStart() - dragged < 300, qPrintable(QString("moved to %1").arg(seekBar->clipStart())));
    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    actionManager->processEvent(&esc);
    QCOMPARE(seekBar->clipStart(), -1);
    QVERIFY2(window->isVisible(), "Esc only cancels the edit.");
}

TG_BEHAVIOR_TEST_MAIN(VideoClipPlaysInsideItsRangeTest)

#include "test_video_clip_plays_inside_its_range.moc"
