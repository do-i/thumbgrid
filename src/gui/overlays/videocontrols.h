#pragma once

#include "settings.h"
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

namespace Ui {
class VideoControls;
}

enum PlaybackMode {
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

signals:
    void seek(int pos);
    void seekForward();
    void seekBackward();
    void volumeChanged(int volume);
    void playbackSpeedChanged(double speed);
    void loopABChanged(int startPosition, int endPosition);
    void toggleMuteRequested();

private:
    enum LoopABState {
        LOOP_AB_CLEAR,
        LOOP_AB_START_SET,
        LOOP_AB_ACTIVE
    };

    void resetLoopAB();
    void updateLoopButton();
    void showPopupBelow(QWidget *anchor, QFrame *popup);
    void resetPlaybackSpeed();
    void updateVolumeText(int volume);
    void updateSpeedText(int value);
    void updateResponsiveVisibility();

    Ui::VideoControls *ui;
    int lastPosition;
    PlaybackMode mode;
    QPushButton *loopABButton;
    QPushButton *speedButton;
    QPushButton *volumeMuteButton;
    QPushButton *resetSpeedButton;
    QSlider *volumeSlider;
    QSlider *speedSlider;
    QLabel *volumePopupLabel;
    QLabel *speedPopupLabel;
    QFrame *volumePopup;
    QFrame *speedPopup;
    LoopABState loopABState;
    int loopStartPosition;
};
