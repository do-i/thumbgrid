#pragma once

#include "videoplayer.h"
#include <QKeyEvent>

#if defined QIMGV_PLAYER_MPV_LIBRARY
 #define TEST_COMMON_DLLSPEC Q_DECL_EXPORT
#else
 #define TEST_COMMON_DLLSPEC Q_DECL_IMPORT
#endif

class MpvWidget;

class VideoPlayerMpv : public VideoPlayer {
    Q_OBJECT
public:
    explicit VideoPlayerMpv(QWidget *parent = nullptr);
    bool showVideo(QString file);
    void setVideoUnscaled(bool mode);
    int volume();

public slots:
    void seek(int pos);
    void seekRelative(int pos);
    void pauseResume();
    void frameStep();
    void frameStepBack();
    void stop();
    void setPaused(bool mode);
    void setMuted(bool);
    bool muted();
    void volumeUp();
    void volumeDown();
    void setVolume(int);
    void show();
    void hide();
    void setLoopPlayback(bool mode);

protected:
    void paintEvent(QPaintEvent *event);

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

signals:
    void playbackFinished();

private slots:
    void readSettings();
    void onPlaybackRestarted();

private:
    MpvWidget *m_mpv;
    // While true, the player is hidden waiting for the next file's first frame
    // so the previous file's last frame doesn't flash during a switch.
    bool mPendingReveal = false;

};

extern "C" TEST_COMMON_DLLSPEC VideoPlayer *CreatePlayerWidget();
