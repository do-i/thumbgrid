#include "support/thumbgrid_test_support.h"

#include <QSignalSpy>
#include <QSlider>

#include "gui/customwidgets/actionbutton.h"
#include "gui/overlays/videocontrols.h"

// The speaker button in the video controls only toggles mute; the volume is a
// slider sitting right next to it in the bar (2-3x the speaker's width), not a
// popup the speaker opens.
class VideoMuteAndVolumeAreSeparateControlsTest : public QObject {
    Q_OBJECT

private slots:
    void theSpeakerTogglesMuteWithoutOpeningAPopup();
    void theVolumeSliderSitsInlineNextToTheSpeaker();
};

namespace {
struct Bar {
    QWidget row;
    VideoControls *controls;
    ActionButton *speaker = nullptr;
    QSlider *volume = nullptr;

    Bar() : controls(new VideoControls(&row)) {
        settings->loadStylesheet();
        row.setAccessibleName("VideoControlsRow");
        auto *layout = new QHBoxLayout(&row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(controls);
        controls->setMode(PLAYBACK_VIDEO);
        controls->show();
        row.resize(640, 26);
        row.show();
        speaker = dynamic_cast<ActionButton *>(controls->findChild<QWidget *>("muteButton"));
        for(QSlider *slider : controls->findChildren<QSlider *>()) {
            if(slider->accessibleName() == "VideoControlVolumeSlider")
                volume = slider;
        }
    }
};
}

void VideoMuteAndVolumeAreSeparateControlsTest::theSpeakerTogglesMuteWithoutOpeningAPopup() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    QVERIFY(bar.speaker);

    QSignalSpy mute(bar.controls, &VideoControls::toggleMuteRequested);
    QTest::mouseClick(bar.speaker, Qt::LeftButton);
    QCOMPARE(mute.count(), 1);
    QCOMPARE(QApplication::activePopupWidget(), nullptr);

    QTest::mouseClick(bar.speaker, Qt::LeftButton);
    QCOMPARE(mute.count(), 2);
}

void VideoMuteAndVolumeAreSeparateControlsTest::theVolumeSliderSitsInlineNextToTheSpeaker() {
    Bar bar;
    QVERIFY(QTest::qWaitForWindowExposed(&bar.row));
    QVERIFY(bar.speaker);
    QVERIFY(bar.volume);

    QVERIFY(bar.volume->isVisible());
    QCOMPARE(bar.volume->window(), bar.speaker->window());
    const QRect speaker = bar.speaker->geometry();
    const QRect volume = bar.volume->geometry();
    QVERIFY2(volume.left() > speaker.right() && volume.left() - speaker.right() <= 8,
             qPrintable(QStringLiteral("volume slider at x=%1, speaker ends at x=%2")
                            .arg(volume.left()).arg(speaker.right())));
    QVERIFY(volume.width() >= 2 * speaker.width());
    QVERIFY(volume.width() <= 3 * speaker.width());

    QSignalSpy changed(bar.controls, &VideoControls::volumeChanged);
    bar.volume->setValue(bar.volume->value() == 30 ? 40 : 30);
    QCOMPARE(changed.count(), 1);

    if(qEnvironmentVariableIsSet("TG_GRAB_DIR"))
        bar.row.grab().save(qEnvironmentVariable("TG_GRAB_DIR") + "/videocontrols.png");

    // Not a video: no speaker, no volume.
    bar.controls->setMode(PLAYBACK_ANIMATION);
    QVERIFY(!bar.speaker->isVisible());
    QVERIFY(!bar.volume->isVisible());
}

TG_BEHAVIOR_TEST_MAIN(VideoMuteAndVolumeAreSeparateControlsTest)
#include "test_video_mute_and_volume_are_separate_controls.moc"
