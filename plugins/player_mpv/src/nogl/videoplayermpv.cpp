#include "videoplayermpv.h"
#include "mpvwidget.h"
#include <QPushButton>
#include <QSlider>
#include <QLayout>
#include <QFileDialog>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QPalette>

// TODO: window flashes white when opening a video (straight from file manager)
VideoPlayerMpv::VideoPlayerMpv(QWidget *parent) : VideoPlayer(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);

    m_mpv = new MpvWidget(this);
    QVBoxLayout *vl = new QVBoxLayout();
    vl->setContentsMargins(0,0,0,0);
    vl->addWidget(m_mpv);
    setLayout(vl);

    mBlankOverlay = new QWidget(this);
    mBlankOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    // mpv renders into MpvWidget's own *native* X window, which stacks above
    // ordinary (non-native) Qt sibling widgets no matter how we raise() them.
    // So the cover must itself be a native window -- then raise() restacks it
    // above mpv at the X level and it actually hides the video.
    mBlankOverlay->setAttribute(Qt::WA_NativeWindow);
    // Paint solid black via the palette; a stylesheet background is unreliable
    // on a native, layout-less child window.
    mBlankOverlay->setAutoFillBackground(true);
    QPalette ovpal = mBlankOverlay->palette();
    ovpal.setColor(QPalette::Window, Qt::black);
    mBlankOverlay->setPalette(ovpal);
    mBlankOverlay->hide();

    setFocusPolicy(Qt::NoFocus);
    m_mpv->setFocusPolicy(Qt::NoFocus);

    readSettings();
    //connect(settings, SIGNAL(settingsChanged()), this, SLOT(readSettings()));
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
    // Blank the player until the new file's first frame is ready, so the
    // previous file's last frame doesn't briefly flash during the switch.
    // onPlaybackRestarted() reveals it again.
    mPendingReveal = true;
    raiseBlankOverlay();
    m_mpv->command(QStringList() << "loadfile" << file);
    m_mpv->setLoopAB(-1, -1);
    setPaused(false);
    return true;
}

void VideoPlayerMpv::onPlaybackRestarted() {
    QTimer::singleShot(50, this, [this]() {
        if(mPendingReveal) {
            mPendingReveal = false;
            mBlankOverlay->hide();
        }
    });
}

void VideoPlayerMpv::seek(int pos) {
    m_mpv->command(QVariantList() << "seek" << pos << "absolute");
}

void VideoPlayerMpv::seekRelative(int pos) {
    m_mpv->command(QVariantList() << "seek" << pos << "relative");
}

void VideoPlayerMpv::pauseResume() {
    const bool paused = m_mpv->getProperty("pause").toBool();
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

void VideoPlayerMpv::setVideoUnscaled(bool mode) {
    if(mode)
        m_mpv->setOption("video-unscaled", "downscale-big");
    else
        m_mpv->setOption("video-unscaled", "no");
}

void VideoPlayerMpv::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
}

void VideoPlayerMpv::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if(mBlankOverlay)
        mBlankOverlay->setGeometry(rect());
}

void VideoPlayerMpv::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // When opening a video from the grid, showVideo() runs while this widget
    // (and the whole document view) is still hidden, so the cover wasn't on a
    // real on-screen window yet. Now that we're actually being shown, re-assert
    // it on top so mpv's old frame doesn't flash before the new one is ready.
    if(mPendingReveal)
        raiseBlankOverlay();
}

// Size the black cover to the player and stack it above mpv's native window.
void VideoPlayerMpv::raiseBlankOverlay() {
    mBlankOverlay->setGeometry(rect());
    mBlankOverlay->show();
    mBlankOverlay->raise();
}

void VideoPlayerMpv::readSettings() {
    //setMuted(!settings->playVideoSounds());
    //setVideoUnscaled(!settings->expandImage());
}

void VideoPlayerMpv::mousePressEvent(QMouseEvent *event) {
    // different platforms, double-click the mouse to judge inconsistencies
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
    // Cancel any pending reveal so a late playback-restart can't pop the video
    // back up after we've switched away (e.g. to an image).
    mPendingReveal = false;
    mBlankOverlay->hide();
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
