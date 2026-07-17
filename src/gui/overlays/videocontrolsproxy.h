#pragma once

#include "gui/overlays/videocontrols.h"
#include <QHBoxLayout>

struct VideoControlsStateBuffer {
    int duration = 0;
    int position = 0;
    int volume = 100;
    bool paused = true;
    bool videoMuted = true;
    PlaybackMode mode = PLAYBACK_VIDEO;
};

class VideoControlsProxyWrapper : public QWidget {
    Q_OBJECT
public:
    explicit VideoControlsProxyWrapper(QWidget *parent = nullptr);
    ~VideoControlsProxyWrapper() override;
    void init();

    void show();
    void hide();
    bool underMouse();

signals:
    void seek(int pos);
    void seekForward();
    void seekBackward();
    void volumeChanged(int volume);
    void playbackSpeedChanged(double speed);
    void loopABChanged(int startPosition, int endPosition);
    void toggleMuteRequested();

public slots:
    void setPlaybackDuration(int);
    void setPlaybackPosition(int);
    void setMode(PlaybackMode);
    void onPlaybackPaused(bool);
    void onVideoMuted(bool);
    void setVolume(int);

private:
    VideoControls *videoControls;
    VideoControlsStateBuffer stateBuf;
    QHBoxLayout layout;
};
