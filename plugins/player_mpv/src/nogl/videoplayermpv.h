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
    bool showVideo(QString file) override;
    void setVideoUnscaled(bool mode) override;
    int volume() override;

public slots:
    void seek(int pos) override;
    void seekRelative(int pos) override;
    void pauseResume() override;
    void frameStep() override;
    void frameStepBack() override;
    void stop() override;
    void setPaused(bool mode) override;
    void setMuted(bool) override;
    bool muted() override;
    void volumeUp() override;
    void volumeDown() override;
    void setVolume(int) override;
    void show() override;
    void hide() override;
    void setLoopPlayback(bool mode) override;
    void setPlaybackSpeed(double speed) override;
    void setLoopAB(int startPosition, int endPosition) override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

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
