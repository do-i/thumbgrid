#include "videoplayermpv.h"
#include "mpvwidget.h"
#include <QLayout>
#include <QTimer>
#ifdef PLAYER_MPV_USE_WID
#include <QPalette>
#include <QResizeEvent>
#include <QShowEvent>
#endif

VideoPlayerMpv::VideoPlayerMpv(QWidget *parent) : VideoPlayer(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);

    m_mpv = new MpvWidget(this);
    QVBoxLayout *vl = new QVBoxLayout();
    vl->setContentsMargins(0,0,0,0);
    vl->addWidget(m_mpv);
    setLayout(vl);

#ifdef PLAYER_MPV_USE_WID
    // mpv's native window stacks above non-native siblings, so the cover has
    // to be native too for raise() to restack it at the X level
    mBlankOverlay = new QWidget(this);
    mBlankOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    mBlankOverlay->setAttribute(Qt::WA_NativeWindow);
    mBlankOverlay->setMouseTracking(true);
    // palette, not a stylesheet: unreliable on a native, layout-less child
    mBlankOverlay->setAutoFillBackground(true);
    QPalette ovpal = mBlankOverlay->palette();
    ovpal.setColor(QPalette::Window, Qt::black);
    mBlankOverlay->setPalette(ovpal);
    mBlankOverlay->hide();
#endif

    setFocusPolicy(Qt::NoFocus);
    m_mpv->setFocusPolicy(Qt::NoFocus);

    connect(m_mpv, SIGNAL(durationChanged(int)), this, SIGNAL(durationChanged(int)));
    connect(m_mpv, SIGNAL(positionChanged(int)), this, SIGNAL(positionChanged(int)));
    connect(m_mpv, SIGNAL(videoPaused(bool)), this, SIGNAL(videoPaused(bool)));
    connect(m_mpv, SIGNAL(playbackFinished()), this, SIGNAL(playbackFinished()));
    connect(m_mpv, SIGNAL(volumeChanged(int)), this, SIGNAL(volumeChanged(int)));
    connect(m_mpv, SIGNAL(playbackRestarted()), this, SLOT(onPlaybackRestarted()));
}

bool VideoPlayerMpv::showVideo(QString file) {
    if(file.isEmpty())
        return false;
    // onPlaybackRestarted() reveals the player again
    mPendingReveal = true;
#ifdef PLAYER_MPV_USE_WID
    raiseBlankOverlay();
#else
    QWidget::hide();
#endif
    m_mpv->command(QStringList() << "loadfile" << file);
    m_mpv->setLoopAB(-1, -1);
    setPaused(false);
    return true;
}

void VideoPlayerMpv::onPlaybackRestarted() {
    QTimer::singleShot(50, this, [this]() {
        if(mPendingReveal) {
            mPendingReveal = false;
#ifdef PLAYER_MPV_USE_WID
            mBlankOverlay->hide();
#else
            QWidget::show();
#endif
        }
    });
}

// exact: seeking shows that very frame, not the keyframe before it
void VideoPlayerMpv::seek(int pos) {
    m_mpv->command(QVariantList() << "seek" << pos / 1000.0 << "absolute+exact");
}

void VideoPlayerMpv::seekRelative(int pos) {
    m_mpv->command(QVariantList() << "seek" << pos << "relative");
}

void VideoPlayerMpv::pauseResume() {
    const bool paused = m_mpv->getProperty("pause").toBool();
    // stopped on the last frame: play again from the start
    if(paused && m_mpv->getProperty("eof-reached").toBool())
        seek(0);
    setPaused(!paused);
}

void VideoPlayerMpv::frameStep() {
    m_mpv->command(QVariantList() << "frame-step");
}

void VideoPlayerMpv::frameStepBack() {
    m_mpv->command(QVariantList() << "frame-back-step");
}

void VideoPlayerMpv::stop() {
    m_mpv->command(QVariantList() << "stop");
}

void VideoPlayerMpv::setPaused(bool mode) {
    m_mpv->setProperty("pause", mode);
}

void VideoPlayerMpv::setMuted(bool mode) {
    m_mpv->setMuted(mode);
}

bool VideoPlayerMpv::muted() {
    return m_mpv->muted();
}

void VideoPlayerMpv::volumeUp() {
    m_mpv->setVolume(m_mpv->volume() + 5);
}

void VideoPlayerMpv::volumeDown() {
    m_mpv->setVolume(m_mpv->volume() - 5);
}

void VideoPlayerMpv::setVolume(int vol) {
    m_mpv->setVolume(vol);
}

int VideoPlayerMpv::volume() {
    return m_mpv->volume();
}

double VideoPlayerMpv::frameRate() {
    const double fps = m_mpv->getProperty("container-fps").toDouble();
    return (fps > 0) ? fps : m_mpv->getProperty("estimated-vf-fps").toDouble();
}

void VideoPlayerMpv::setHardwareDecoding(bool mode) {
    m_mpv->setHardwareDecoding(mode);
}

void VideoPlayerMpv::setVideoUnscaled(bool mode) {
    m_mpv->setOption("video-unscaled", mode ? "downscale-big" : "no");
}

void VideoPlayerMpv::paintEvent(QPaintEvent *) {
}

#ifdef PLAYER_MPV_USE_WID
void VideoPlayerMpv::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if(mBlankOverlay)
        mBlankOverlay->setGeometry(rect());
}

// Opened from the grid, showVideo() ran while this widget was still hidden, so
// the cover was not on a real window yet: re-assert it now.
void VideoPlayerMpv::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if(mPendingReveal)
        raiseBlankOverlay();
}

void VideoPlayerMpv::raiseBlankOverlay() {
    mBlankOverlay->setGeometry(rect());
    mBlankOverlay->show();
    mBlankOverlay->raise();
}
#endif

void VideoPlayerMpv::mousePressEvent(QMouseEvent *event) {
#ifndef PLAYER_MPV_USE_WID
    if(event->button() == Qt::LeftButton && event->type() != QEvent::MouseButtonDblClick) {
        event->accept();
        pauseResume();
        return;
    }
#endif
    QWidget::mousePressEvent(event);
    event->ignore();
}

void VideoPlayerMpv::mouseMoveEvent(QMouseEvent *event) {
    QWidget::mouseMoveEvent(event);
    event->ignore();
}

void VideoPlayerMpv::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);
    event->ignore();
}

void VideoPlayerMpv::show() {
    QWidget::show();
}

void VideoPlayerMpv::hide() {
    // a late playback-restart must not pop the video back up after switching away
    mPendingReveal = false;
#ifdef PLAYER_MPV_USE_WID
    mBlankOverlay->hide();
#endif
    QWidget::hide();
}

void VideoPlayerMpv::setLoopPlayback(bool mode) {
    m_mpv->setRepeat(mode);
}

void VideoPlayerMpv::setPlaybackSpeed(double speed) {
    m_mpv->setPlaybackSpeed(speed);
}

void VideoPlayerMpv::setLoopAB(int startPosition, int endPosition) {
    m_mpv->setLoopAB(startPosition, endPosition);
}

VideoPlayer *CreatePlayerWidget() {
    return new VideoPlayerMpv();
}

int PlayerInterfaceVersion() {
    return VIDEO_PLAYER_INTERFACE_VERSION;
}
