#include "support/thumbgrid_test_support.h"

#include <clocale>

#include <QDir>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core.h"
#include "gui/mainwindow.h"
#include "gui/viewers/videoplayer.h"
#include "gui/viewers/viewerwidget.h"

// Space used to pause a video through a hard-coded check ahead of the
// bindings. It is now ordinary: Space runs toggleFitMode, and the fit actions
// pause/resume a video because a video has nothing to fit.

class SpacePausesVideoTest : public QObject {
    Q_OBJECT

private slots:
    void spaceRunsItsBindingWhichPausesTheVideo();
};

void SpacePausesVideoTest::spaceRunsItsBindingWhichPausesTheVideo() {
    // mpv embeds into a native X window; the default offscreen platform has
    // none (run with QT_QPA_PLATFORM=xcb on a virtual display, e.g. Xvfb)
    if(QGuiApplication::platformName() != QLatin1String("xcb"))
        QSKIP("Video playback needs a real window system (QT_QPA_PLATFORM=xcb).");
    const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if(ffmpeg.isEmpty())
        QSKIP("ffmpeg is needed to make the test clip.");

    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    const QString clip = QDir(fixture.path()).filePath("clip.mp4");
    // silent test pattern; no audio track
    const int rc = QProcess::execute(ffmpeg, {"-v", "error", "-f", "lavfi", "-i",
                                              "testsrc=duration=4:size=160x120:rate=10",
                                              "-pix_fmt", "yuv420p", "-an", clip});
    if(rc != 0 || !QFile::exists(clip))
        QSKIP("ffmpeg could not make the test clip.");

    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "Space"), QStringLiteral("toggleFitMode"));
    // mpv refuses to start under a non-C numeric locale (main() sets this too)
    std::setlocale(LC_NUMERIC, "C");

    Core core;
    QVERIFY(core.loadPath(clip));
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    auto viewerWidget = window->findChild<ViewerWidget *>();
    QVERIFY(viewerWidget);
    QTest::qWait(1500);
    if(!viewerWidget->isShowingVideo())
        QSKIP("The video player is not available in this environment.");
    auto player = viewerWidget->findChild<VideoPlayer *>();
    QVERIFY(player);
    QSignalSpy paused(player, &VideoPlayer::videoPaused);

    QTest::keyClick(viewerWidget, Qt::Key_Space);
    QTRY_VERIFY2(!paused.isEmpty(), "Space should pause the playing video.");
    QCOMPARE(paused.last().first().toBool(), true);
}

TG_BEHAVIOR_TEST_MAIN(SpacePausesVideoTest)

#include "test_space_pauses_video.moc"
