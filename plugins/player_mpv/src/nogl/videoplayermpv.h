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
    void setPlaybackSpeed(double speed);
    void setLoopAB(int startPosition, int endPosition);

protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);
    void showEvent(QShowEvent *event);

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

signals:
    void playbackFinished();

private slots:
    void readSettings();
    void onPlaybackRestarted();

private:
    void raiseBlankOverlay();

    MpvWidget *m_mpv;
    QWidget *mBlankOverlay;
    // While true, the player is covered until the next file has started
    // painting, so the previous file's last frame doesn't flash.
    bool mPendingReveal = false;

};

extern "C" TEST_COMMON_DLLSPEC VideoPlayer *CreatePlayerWidget();
