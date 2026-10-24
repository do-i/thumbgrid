#pragma once

#include "settings.h"
#include "gui/customwidgets/actionbutton.h"
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>
#include <cstdint>

namespace Ui {
class VideoControls;
}

enum PlaybackMode : std::uint8_t {
    PLAYBACK_ANIMATION,
    PLAYBACK_VIDEO
};

class VideoControls : public QWidget
{
    Q_OBJECT

public:
    explicit VideoControls(QWidget *parent = nullptr);
    ~VideoControls() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

public slots:
    void setPlaybackDuration(int);
    void setPlaybackPosition(int);
    void onPlaybackPaused(bool);
    void onVideoMuted(bool);
    void setMode(PlaybackMode _mode);
    void setVolume(int);
    // the clip on the seek bar (ms); start < 0: none. While editing the
    // handles can be dragged
    void setClip(int start, int end, bool editing);
    void setLoop(bool mode);

signals:
    void seek(int pos);
    void seekForward();
    void seekBackward();
    void volumeChanged(int volume);
    void playbackSpeedChanged(double speed);
    void toggleMuteRequested();
    // a clip handle (VideoSlider::ClipHandle) grabbed, dragged to pos
    void clipHandlePressed(int handle);
    void clipHandleMoved(int handle, int pos);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateClipButton(bool clip, bool editing);
    void showPopupBelow(QWidget *anchor, QFrame *popup);
    void resetPlaybackSpeed();
    void updateVolumeText(int volume);
    void updateSpeedText(int value);
    void updateResponsiveVisibility();

    Ui::VideoControls *ui;
    int lastPosition;
    PlaybackMode mode;
    ActionButton *clipButton;
    ActionButton *loopButton;
    QPushButton *speedButton;
    QPushButton *resetSpeedButton;
    QSlider *volumeSlider;
    QSlider *speedSlider;
    QLabel *speedPopupLabel;
    QFrame *speedPopup;
    // the last time shown, so a video's per-frame updates don't redo the text
    int shownSeconds = -1;
};
