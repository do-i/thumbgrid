#include "videocontrolsproxy.h"

VideoControlsProxyWrapper::VideoControlsProxyWrapper(QWidget *parent)
    : QWidget(parent),
      videoControls(nullptr)
{
    setAccessibleName("VideoControlsRow");
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout.setContentsMargins(0, 0, 0, 0);
    layout.setSpacing(0);
    setLayout(&layout);
    QWidget::hide();
}

VideoControlsProxyWrapper::~VideoControlsProxyWrapper() {
}

void VideoControlsProxyWrapper::init() {
    if(videoControls)
        return;
    videoControls = new VideoControls(this);
    layout.addWidget(videoControls);
    layout.addStretch(1);

    connect(videoControls, &VideoControls::seekBackward,  this, &VideoControlsProxyWrapper::seekBackward);
    connect(videoControls, &VideoControls::seekForward, this, &VideoControlsProxyWrapper::seekForward);
    connect(videoControls, &VideoControls::seek,      this, &VideoControlsProxyWrapper::seek);
    connect(videoControls, &VideoControls::volumeChanged, this, &VideoControlsProxyWrapper::volumeChanged);
    connect(videoControls, &VideoControls::playbackSpeedChanged, this, &VideoControlsProxyWrapper::playbackSpeedChanged);
    connect(videoControls, &VideoControls::loopABChanged, this, &VideoControlsProxyWrapper::loopABChanged);

    videoControls->setMode(stateBuf.mode);
    videoControls->setPlaybackDuration(stateBuf.duration);
    videoControls->setPlaybackPosition(stateBuf.position);
    videoControls->setVolume(stateBuf.volume);
    videoControls->onPlaybackPaused(stateBuf.paused);
    videoControls->onVideoMuted(stateBuf.videoMuted);
}

void VideoControlsProxyWrapper::show() {
    init();
    QWidget::show();
    videoControls->show();
}

void VideoControlsProxyWrapper::hide() {
    QWidget::hide();
}

void VideoControlsProxyWrapper::setPlaybackDuration(int _duration) {
    if(videoControls) {
        videoControls->setPlaybackDuration(_duration);
    } else {
        stateBuf.duration = _duration;
    }
}

void VideoControlsProxyWrapper::setPlaybackPosition(int _position) {
    if(videoControls) {
        videoControls->setPlaybackPosition(_position);
    } else {
        stateBuf.position = _position;
    }
}

void VideoControlsProxyWrapper::setMode(PlaybackMode _mode) {
    if(videoControls) {
        videoControls->setMode(_mode);
    } else {
        stateBuf.mode = _mode;
    }
}

void VideoControlsProxyWrapper::onPlaybackPaused(bool _mode) {
    if(videoControls) {
        videoControls->onPlaybackPaused(_mode);
    } else {
        stateBuf.paused = _mode;
    }
}

void VideoControlsProxyWrapper::onVideoMuted(bool _mode) {
    if(videoControls) {
        videoControls->onVideoMuted(_mode);
    } else {
        stateBuf.videoMuted = _mode;
    }
}

void VideoControlsProxyWrapper::setVolume(int _volume) {
    if(videoControls) {
        videoControls->setVolume(_volume);
    } else {
        stateBuf.volume = _volume;
    }
}

bool VideoControlsProxyWrapper::underMouse() {
    return QWidget::underMouse();
}
