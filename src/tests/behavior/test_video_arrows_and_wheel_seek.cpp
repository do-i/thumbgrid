#include "support/thumbgrid_test_support.h"

#include <clocale>

#include <QDir>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/mainwindow.h"
#include "gui/viewers/videoplayer.h"
#include "gui/viewers/viewerwidget.h"

// Left/Right and the wheel changed file in a video, where there is nothing to
// scroll. A video now runs the actions' video fallbacks, which seek by default
// and can be set back to changing file.

class VideoArrowsAndWheelSeekTest : public QObject {
    Q_OBJECT

private slots:
    void arrowsAndWheelSeekInAVideo();
};

void VideoArrowsAndWheelSeekTest::arrowsAndWheelSeekInAVideo() {
    // mpv embeds into a native X window; the default offscreen platform has
    // none (run with QT_QPA_PLATFORM=xcb on a virtual display, e.g. Xvfb)
    if(QGuiApplication::platformName() != QLatin1String("xcb"))
        QSKIP("Video playback needs a real window system (QT_QPA_PLATFORM=xcb).");
    const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if(ffmpeg.isEmpty())
        QSKIP("ffmpeg is needed to make the test clip.");

    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    const QDir dir(fixture.path());
    const QString clip = dir.filePath("b-clip.mp4");
    // silent test pattern; no audio track
    const int rc = QProcess::execute(ffmpeg, {"-v", "error", "-f", "lavfi", "-i",
                                              "testsrc=duration=60:size=160x120:rate=5",
                                              "-pix_fmt", "yuv420p", "-an", clip});
    if(rc != 0 || !QFile::exists(clip))
        QSKIP("ffmpeg could not make the test clip.");
    for(const QString &name : {"a-before.png", "c-after.png"}) {
        QImage image(64, 48, QImage::Format_RGB32);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(dir.filePath(name)));
    }

    QCOMPARE(actionManager->actionForShortcut(MODE_DOCUMENT, "Right"), QStringLiteral("scrollRight"));
    QCOMPARE(actionManager->videoScrollFallback("scrollLeft"), QStringLiteral("seekVideoBackward"));
    QCOMPARE(actionManager->videoScrollFallback("scrollRight"), QStringLiteral("seekVideoForward"));
    QCOMPARE(actionManager->videoScrollFallback("scrollOrPrevImage"), QStringLiteral("seekVideoForward"));
    QCOMPARE(actionManager->videoScrollFallback("scrollOrNextImage"), QStringLiteral("seekVideoBackward"));
    QVERIFY(actionManager->videoScrollFallback("scrollDown").isEmpty());
    // outside a video the arrows still change file
    QCOMPARE(actionManager->scrollFallback("scrollRight"), QStringLiteral("nextImage"));
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
    player->setPaused(true);
    player->seek(20);
    int position = -1;
    connect(player, &VideoPlayer::positionChanged, this, [&](int value) { position = value; });
    QTRY_COMPARE(position, 20);

    QTest::keyClick(viewerWidget, Qt::Key_Right);
    QTRY_VERIFY2(position >= 28, "Right should seek the video forward.");
    QTest::keyClick(viewerWidget, Qt::Key_Left);
    QTRY_VERIFY2(position <= 22, "Left should seek the video back.");
    // the wheel seeks like mpv's and a slider's: up forward, down back
    actionManager->invokeAction("scrollOrPrevImage");
    QTRY_VERIFY2(position >= 28, "The wheel up should seek the video forward.");
    actionManager->invokeAction("scrollOrNextImage");
    QTRY_VERIFY2(position <= 22, "The wheel down should seek the video back.");
    QVERIFY2(window->windowTitle().contains("b-clip.mp4"), "Seeking must not change file.");

    // set back to changing file, Right opens the next one
    actionManager->setVideoScrollFallbackOverrides({{"scrollRight", "nextImage"}});
    QMap<QString, QString> saved;
    settings->readShortcutVideoFallbacks(saved);
    QCOMPARE(saved, (QMap<QString, QString>{{"scrollRight", "nextImage"}}));
    QTest::keyClick(viewerWidget, Qt::Key_Right);
    QTRY_VERIFY2(window->windowTitle().contains("c-after.png"), "Right set to nextImage should change file.");
    actionManager->setVideoScrollFallbackOverrides({});
}

TG_BEHAVIOR_TEST_MAIN(VideoArrowsAndWheelSeekTest)

#include "test_video_arrows_and_wheel_seek.moc"
