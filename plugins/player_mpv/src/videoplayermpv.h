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
    double frameRate() override;
    void setHardwareDecoding(bool mode) override;

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
#ifdef PLAYER_MPV_USE_WID
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
#endif
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onPlaybackRestarted();

private:
    MpvWidget *m_mpv;
    // covered/hidden until the next file has fresh output, so the previous
    // file's last frame doesn't flash
    bool mPendingReveal = false;
#ifdef PLAYER_MPV_USE_WID
    void raiseBlankOverlay();
    QWidget *mBlankOverlay;
#endif
};

extern "C" TEST_COMMON_DLLSPEC VideoPlayer *CreatePlayerWidget();
// VIDEO_PLAYER_INTERFACE_VERSION this plugin was built against
extern "C" TEST_COMMON_DLLSPEC int PlayerInterfaceVersion();
