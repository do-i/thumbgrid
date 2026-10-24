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
    int clipStart = -1;
    int clipEnd = -1;
    bool clipEditing = false;
    bool loop = false;
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
    void toggleMuteRequested();
    void clipHandlePressed(int handle);
    void clipHandleMoved(int handle, int pos);

public slots:
    void setPlaybackDuration(int);
    void setPlaybackPosition(int);
    void setMode(PlaybackMode);
    void onPlaybackPaused(bool);
    void onVideoMuted(bool);
    void setVolume(int);
    void setClip(int start, int end, bool editing);
    void setLoop(bool mode);

private:
    VideoControls *videoControls;
    VideoControlsStateBuffer stateBuf;
    QHBoxLayout layout;
};
