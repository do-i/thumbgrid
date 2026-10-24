#pragma once

#include <QWidget>

// Bumped whenever this interface changes; the app refuses a player plugin built
// against another version (CreatePlayerWidget alone can't tell them apart).
// 2: positions, durations, seek() and setLoopAB() are in milliseconds;
//    frameRate() added.
constexpr int VIDEO_PLAYER_INTERFACE_VERSION = 2;

class VideoPlayer : public QWidget {
    Q_OBJECT
public:
    explicit VideoPlayer(QWidget *parent = nullptr);
    virtual bool showVideo(QString file) = 0;
    // ms
    virtual void seek(int pos) = 0;
    // seconds
    virtual void seekRelative(int pos) = 0;
    virtual void pauseResume() = 0;
    virtual void frameStep() = 0;
    virtual void frameStepBack() = 0;
    virtual void stop() = 0;
    virtual void setPaused(bool mode) = 0;
    virtual void setMuted(bool) = 0;
    virtual bool muted() = 0;
    virtual void volumeUp() = 0;
    virtual void volumeDown() = 0;
    virtual void setVolume(int) = 0;
    virtual int volume() = 0;
    virtual void setVideoUnscaled(bool mode) = 0;
    virtual void setLoopPlayback(bool mode) = 0;
    virtual void setPlaybackSpeed(double speed) = 0;
    // ms; start < 0 clears the loop
    virtual void setLoopAB(int startPosition, int endPosition) = 0;
    // frames per second, 0 when unknown
    virtual double frameRate() = 0;

signals:
    // ms
    void durationChanged(int value);
    void positionChanged(int value);
    void videoPaused(bool);
    void playbackFinished();
    void volumeChanged(int value);

public slots:
    virtual void show();
    virtual void hide();
};
