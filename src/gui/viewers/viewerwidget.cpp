/*
 * This widget combines ImageViewer / VideoPlayer.
 * Only one is displayed at a time.
 */

#include "viewerwidget.h"

#include <QApplication>

#include <utility>

#include "platform/platformdesktop.h"

ViewerWidget::ViewerWidget(QWidget *parent)
    : FloatingWidgetContainer(parent),
      imageViewer(nullptr),
      videoPlayer(nullptr),
      contextMenu(nullptr),
      videoControls(nullptr),
      currentWidget(UNSET),
      mInteractionEnabled(false),
      mWaylandCursorWorkaround(false),
      mIsFullscreen(false)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    mWaylandCursorWorkaround = PlatformDesktop::needsWaylandCursorWorkaround();
    layout.setContentsMargins(0, 0, 0, 0);
    layout.setSpacing(0);
    this->setLayout(&layout);

    imageViewer.reset(new ImageViewerV2(this));
    layout.addWidget(imageViewer.get());
    imageViewer->hide();

    connect(imageViewer.get(), &ImageViewerV2::scalingRequested, this, &ViewerWidget::scalingRequested);
    connect(imageViewer.get(), &ImageViewerV2::scaleChanged, this, &ViewerWidget::onScaleChanged);
    connect(imageViewer.get(), &ImageViewerV2::playbackFinished, this, &ViewerWidget::onAnimationPlaybackFinished);
    connect(this, &ViewerWidget::toggleTransparencyGrid, imageViewer.get(), &ImageViewerV2::toggleTransparencyGrid);
    connect(this, &ViewerWidget::setFilterNearest,       imageViewer.get(), &ImageViewerV2::setFilterNearest);
    connect(this, &ViewerWidget::setFilterBilinear,      imageViewer.get(), &ImageViewerV2::setFilterBilinear);
    connect(this, &ViewerWidget::setScalingFilter,       imageViewer.get(), &ImageViewerV2::setScalingFilter);


    videoPlayer.reset(new VideoPlayerInitProxy(this));
    layout.addWidget(videoPlayer.get());
    videoPlayer->hide();

    textViewer.reset(new TextViewer(this));
    layout.addWidget(textViewer.get());
    textViewer->hide();
    // the zoom actions change the text size of a shown text file
    for(auto zoom : {&ViewerWidget::zoomIn, &ViewerWidget::zoomInCursor}) {
        connect(this, zoom, this, [this]() {
            if(currentWidget == TEXTVIEWER)
                textViewer->zoom(1);
        });
    }
    for(auto zoom : {&ViewerWidget::zoomOut, &ViewerWidget::zoomOutCursor}) {
        connect(this, zoom, this, [this]() {
            if(currentWidget == TEXTVIEWER)
                textViewer->zoom(-1);
        });
    }
    videoControls = new VideoControlsProxyWrapper(this);

    // tmp no wrapper
    zoomIndicator = new ZoomIndicatorOverlayProxy(this);
    clickZoneOverlay = new ClickZoneOverlay(this);

    connect(videoPlayer.get(), &VideoPlayer::playbackFinished, this, &ViewerWidget::onVideoPlaybackFinished);
    connect(videoPlayer.get(), &VideoPlayer::durationChanged, this, [this](int duration) {
        videoClip.setDuration(duration);
        updateVideoControlsVisibility();
    });
    connect(videoPlayer.get(), &VideoPlayer::positionChanged, this, &ViewerWidget::onVideoPositionChanged);
    connect(videoPlayer.get(), &VideoPlayer::videoPaused, this, [this](bool paused) {
        mVideoPaused = paused;
    });
    // show the real audio-output volume on the slider
    connect(videoPlayer.get(), &VideoPlayer::volumeChanged, videoControls, &VideoControlsProxyWrapper::setVolume);

    connect(videoControls, &VideoControlsProxyWrapper::seekBackward,  this, &ViewerWidget::seekBackward);
    connect(videoControls, &VideoControlsProxyWrapper::seekForward, this, &ViewerWidget::seekForward);
    connect(videoControls, &VideoControlsProxyWrapper::seek,      this, &ViewerWidget::seek);
    connect(videoControls, &VideoControlsProxyWrapper::volumeChanged, this, &ViewerWidget::setVolume);
    connect(videoControls, &VideoControlsProxyWrapper::playbackSpeedChanged, this, &ViewerWidget::setPlaybackSpeed);
    connect(videoControls, &VideoControlsProxyWrapper::toggleMuteRequested, this, &ViewerWidget::toggleMute);
    // VideoSlider::ClipHandle and VideoClip::Handle number the handles alike
    connect(videoControls, &VideoControlsProxyWrapper::clipHandlePressed, this, &ViewerWidget::onClipHandlePressed);
    connect(videoControls, &VideoControlsProxyWrapper::clipHandleMoved, this, &ViewerWidget::onClipHandleMoved);
    connect(&videoClip, &VideoClip::changed, this, &ViewerWidget::applyVideoClip);
    mPreviewSeekTimer.setSingleShot(true);
    mPreviewSeekTimer.setInterval(PREVIEW_SEEK_MS);
    connect(&mPreviewSeekTimer, &QTimer::timeout, this, [this]() {
        if(mPreviewSeekTarget != mPreviewSeekSent)
            previewSeek(mPreviewSeekTarget);
    });

    enableImageViewer();
    setInteractionEnabled(true);

    connect(&cursorTimer, &QTimer::timeout, this, &ViewerWidget::hideCursor);

    connect(settings, &Settings::settingsChanged, this, &ViewerWidget::readSettings);
    readSettings();
}

QRect ViewerWidget::imageRect() {
    if(imageViewer && currentWidget == IMAGEVIEWER)
        return imageViewer->scaledRectR();
    else
        return QRect(0,0,0,0);
}

float ViewerWidget::currentScale() {
    if(currentWidget == IMAGEVIEWER)
        return imageViewer->currentScale();
    else
        return 1.0f;
}

QSize ViewerWidget::sourceSize() {
    if(currentWidget == IMAGEVIEWER)
        return imageViewer->sourceSize();
    else
        return QSize(0,0);
}

// hide videoPlayer, show imageViewer
void ViewerWidget::enableImageViewer() {
    if(currentWidget != IMAGEVIEWER) {
        disableVideoPlayer();
        disableTextViewer();
        videoControls->setMode(PLAYBACK_ANIMATION);
        connect(imageViewer.get(), &ImageViewerV2::durationChanged, videoControls, &VideoControlsProxyWrapper::setPlaybackDuration);
        connect(imageViewer.get(), &ImageViewerV2::frameChanged,    videoControls, &VideoControlsProxyWrapper::setPlaybackPosition);
        connect(imageViewer.get(), &ImageViewerV2::animationPaused, videoControls, &VideoControlsProxyWrapper::onPlaybackPaused);
        imageViewer->show();
        currentWidget = IMAGEVIEWER;
        updateVideoControlsVisibility();
    }
}

// hide imageViewer, show videoPlayer
void ViewerWidget::enableVideoPlayer() {
    if(currentWidget != VIDEOPLAYER) {
        disableImageViewer();
        disableTextViewer();
        videoControls->setMode(PLAYBACK_VIDEO);
        connect(videoPlayer.get(), &VideoPlayer::durationChanged, videoControls, &VideoControlsProxyWrapper::setPlaybackDuration);
        connect(videoPlayer.get(), &VideoPlayer::positionChanged, videoControls, &VideoControlsProxyWrapper::setPlaybackPosition);
        connect(videoPlayer.get(), &VideoPlayer::videoPaused,     videoControls, &VideoControlsProxyWrapper::onPlaybackPaused);
        videoPlayer->show();
        currentWidget = VIDEOPLAYER;
        updateVideoControlsVisibility();
    }
}

void ViewerWidget::disableImageViewer() {
    if(currentWidget == IMAGEVIEWER) {
        currentWidget = UNSET;
        imageViewer->closeImage();
        imageViewer->hide();
        zoomIndicator->hide();
        disconnect(imageViewer.get(), &ImageViewerV2::durationChanged, videoControls, &VideoControlsProxyWrapper::setPlaybackDuration);
        disconnect(imageViewer.get(), &ImageViewerV2::frameChanged,    videoControls, &VideoControlsProxyWrapper::setPlaybackPosition);
        disconnect(imageViewer.get(), &ImageViewerV2::animationPaused, videoControls, &VideoControlsProxyWrapper::onPlaybackPaused);
        updateVideoControlsVisibility();
    }
}

// hide other widgets, show textViewer
void ViewerWidget::enableTextViewer() {
    if(currentWidget != TEXTVIEWER) {
        disableImageViewer();
        disableVideoPlayer();
        videoControls->hide();
        textViewer->show();
        currentWidget = TEXTVIEWER;
    }
}

void ViewerWidget::disableTextViewer() {
    if(currentWidget == TEXTVIEWER) {
        currentWidget = UNSET;
        textViewer->clear();
        textViewer->hide();
    }
}

void ViewerWidget::disableVideoPlayer() {
    if(currentWidget == VIDEOPLAYER) {
        currentWidget = UNSET;
        disconnect(videoPlayer.get(), &VideoPlayer::durationChanged, videoControls, &VideoControlsProxyWrapper::setPlaybackDuration);
        disconnect(videoPlayer.get(), &VideoPlayer::positionChanged, videoControls, &VideoControlsProxyWrapper::setPlaybackPosition);
        disconnect(videoPlayer.get(), &VideoPlayer::videoPaused,     videoControls, &VideoControlsProxyWrapper::onPlaybackPaused);
        videoPlayer->setPaused(true);
        videoClip.reset();
        applyVideoClip();
        // hide on leave so the last frame can't flash over the next image
        videoPlayer->hide();
        updateVideoControlsVisibility();
    }
}

void ViewerWidget::onScaleChanged(qreal scale) {
    emit scaleChanged();
    if(!this->isVisible())
        return;
    if(scale != 1.0f) {
        zoomIndicator->setScale(scale);
        if(settings->zoomIndicatorMode() == ZoomIndicatorMode::INDICATOR_ENABLED)
            zoomIndicator->show();
        else if((settings->zoomIndicatorMode() == ZoomIndicatorMode::INDICATOR_AUTO))
            zoomIndicator->show(1500);
    } else {
        zoomIndicator->hide();
    }
}

void ViewerWidget::onVideoPlaybackFinished() {
    if(currentWidget != VIDEOPLAYER)
        return;
    // mpv's A-B loop may not repeat a clip that runs to the very end
    if(videoClip.isActive() && videoClip.loop()) {
        seekVideoTo(videoClip.start());
        videoPlayer->setPaused(false);
        return;
    }
    emit playbackFinished();
}

void ViewerWidget::onAnimationPlaybackFinished() {
    if(currentWidget == IMAGEVIEWER)
        emit playbackFinished();
}

void ViewerWidget::setInteractionEnabled(bool mode) {
    if(mInteractionEnabled == mode)
        return;
    mInteractionEnabled = mode;
    if(mInteractionEnabled) {
        connect(this, &ViewerWidget::toggleLockZoom, imageViewer.get(), &ImageViewerV2::toggleLockZoom);
        connect(this, &ViewerWidget::toggleLockView, imageViewer.get(), &ImageViewerV2::toggleLockView);
        connect(this, &ViewerWidget::zoomIn,         imageViewer.get(), &ImageViewerV2::zoomIn);
        connect(this, &ViewerWidget::zoomOut,        imageViewer.get(), &ImageViewerV2::zoomOut);
        connect(this, &ViewerWidget::zoomInCursor,   imageViewer.get(), &ImageViewerV2::zoomInCursor);
        connect(this, &ViewerWidget::zoomOutCursor,  imageViewer.get(), &ImageViewerV2::zoomOutCursor);
        connect(this, &ViewerWidget::fitWindow,      imageViewer.get(), &ImageViewerV2::setFitWindow);
        connect(this, &ViewerWidget::fitWidth,       imageViewer.get(), &ImageViewerV2::setFitWidth);
        connect(this, &ViewerWidget::fitOriginal,    imageViewer.get(), &ImageViewerV2::setFitOriginal);
        connect(this, &ViewerWidget::fitWindowStretch, imageViewer.get(), &ImageViewerV2::setFitWindowStretch);
        connect(imageViewer.get(), &ImageViewerV2::draggedOut, this, &ViewerWidget::draggedOut);
        imageViewer->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    } else {
        disconnect(this, &ViewerWidget::zoomIn,        imageViewer.get(), &ImageViewerV2::zoomIn);
        disconnect(this, &ViewerWidget::zoomOut,       imageViewer.get(), &ImageViewerV2::zoomOut);
        disconnect(this, &ViewerWidget::zoomInCursor,  imageViewer.get(), &ImageViewerV2::zoomInCursor);
        disconnect(this, &ViewerWidget::zoomOutCursor, imageViewer.get(), &ImageViewerV2::zoomOutCursor);
        disconnect(this, &ViewerWidget::fitWindow,     imageViewer.get(), &ImageViewerV2::setFitWindow);
        disconnect(this, &ViewerWidget::fitWidth,      imageViewer.get(), &ImageViewerV2::setFitWidth);
        disconnect(this, &ViewerWidget::fitOriginal,   imageViewer.get(), &ImageViewerV2::setFitOriginal);
        disconnect(this, &ViewerWidget::fitWindowStretch, imageViewer.get(), &ImageViewerV2::setFitWindowStretch);
        disconnect(imageViewer.get(), &ImageViewerV2::draggedOut, this, &ViewerWidget::draggedOut);
        imageViewer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        hideContextMenu();
    }
}

bool ViewerWidget::interactionEnabled() {
    return mInteractionEnabled;
}

bool ViewerWidget::showImage(std::unique_ptr<QPixmap> pixmap) {
    if(!pixmap)
        return false;
    stopPlayback();
    videoControls->hide();
    if(currentWidget == VIDEOPLAYER) {
        // from a video: paint the image under mpv's native window first, then hide the video,
        // so no background frame shows between them
        imageViewer->show();
        imageViewer->showImage(std::move(pixmap));
        imageViewer->repaint();
        enableImageViewer();
    } else {
        enableImageViewer();
        // prepareForLoad() may have hidden it while already on it
        imageViewer->show();
        imageViewer->showImage(std::move(pixmap));
    }
    showCursor();
    hideCursorTimed(false);
    return true;
}

bool ViewerWidget::showAnimation(const std::shared_ptr<QMovie>& movie) {
    if(!movie)
        return false;
    stopPlayback();
    enableImageViewer();
    imageViewer->show();
    imageViewer->showAnimation(movie);
    updateVideoControlsVisibility();
    showCursor();
    hideCursorTimed(false);
    return true;
}

bool ViewerWidget::showVideo(QString file) {
    stopPlayback();
    enableVideoPlayer();
    // every file starts without a clip and with loop off
    videoClip.reset();
    mVideoPosition = 0;
    applyVideoClip();
    videoPlayer->showVideo(std::move(file));
    updateVideoControlsVisibility();
    // restore the pointer on every switch; mpv's window doesn't track our autohide
    showCursor();
    hideCursorTimed(false);
    return true;
}

bool ViewerWidget::showText(const QString& file) {
    stopPlayback();
    enableTextViewer();
    bool ok = textViewer->showFile(file);
    showCursor();
    hideCursorTimed(false);
    return ok;
}

// blank the outgoing view only across an image<->video change; image->image keeps its frame
void ViewerWidget::prepareForLoad(bool nextIsVideo) {
    if(currentWidget == VIDEOPLAYER) {
        // Pause, don't stop: re-creating mpv's video output crashes with broken GL acceleration.
        // Not blanked here: showImage()/showVideo() swap atomically.
        videoPlayer->setPaused(true);
    } else if(currentWidget == IMAGEVIEWER && nextIsVideo) {
        // the plugin blanks the player until its first frame
        imageViewer->hide();
    }
}

void ViewerWidget::stopPlayback() {
    if(currentWidget == IMAGEVIEWER && imageViewer->hasAnimation())
        imageViewer->stopAnimation();
    if(currentWidget == VIDEOPLAYER) {
        videoPlayer->setPaused(true);
    }
}

void ViewerWidget::startPlayback() {
    if(currentWidget == IMAGEVIEWER && imageViewer->hasAnimation())
        imageViewer->startAnimation();
    if(currentWidget == VIDEOPLAYER) {
        videoPlayer->setPaused(false);
    }
}

void ViewerWidget::setFitMode(ImageFitMode mode) {
    if(mode == FIT_WINDOW)
        emit fitWindow();
    else if(mode == FIT_WIDTH)
        emit fitWidth();
    else if(mode == FIT_ORIGINAL)
        emit fitOriginal();
    else if(mode == FIT_WINDOW_STRETCH)
        emit fitWindowStretch();
}

ImageFitMode ViewerWidget::fitMode() {
    return imageViewer->fitMode();
}

void ViewerWidget::onScalingFinished(std::unique_ptr<QPixmap> scaled) {
    imageViewer->setScaledPixmap(std::move(scaled));
}

void ViewerWidget::closeImage() {
    imageViewer->closeImage();
    videoPlayer->stop();
    showCursor();
}

bool ViewerWidget::isShowingVideo() const {
    return currentWidget == VIDEOPLAYER;
}

// with a clip, play resumes inside it or restarts at its start
void ViewerWidget::pauseResumePlayback() {
    if(currentWidget == VIDEOPLAYER) {
        const int target = mVideoPaused ? videoClip.resumeTarget(mVideoPosition) : -1;
        if(target >= 0) {
            seekVideoTo(target);
            videoPlayer->setPaused(false);
        } else {
            videoPlayer.get()->pauseResume();
        }
    } else if(imageViewer->hasAnimation())
        imageViewer->pauseResume();
}

void ViewerWidget::seek(int pos) {
    if(currentWidget == VIDEOPLAYER) {
        seekVideoTo(videoClip.seekTarget(pos));
    } else if(imageViewer->hasAnimation()) {
        imageViewer->stopAnimation();
        imageViewer->showAnimationFrame(pos);
    }
}

// seconds; an active clip keeps the seek inside it
void ViewerWidget::seekRelative(int pos) {
    if(currentWidget != VIDEOPLAYER)
        return;
    if(videoClip.isActive())
        seekVideoTo(videoClip.seekTarget(mVideoPosition + pos * 1000));
    else
        videoPlayer.get()->seekRelative(pos);
}

void ViewerWidget::seekBackward() {
    seekRelative(-10);
}

void ViewerWidget::seekForward() {
    seekRelative(10);
}

void ViewerWidget::seekVideoTo(int pos) {
    videoPlayer->seek(pos);
    mVideoPosition = pos;
}

void ViewerWidget::frameStep() {
    if(currentWidget == VIDEOPLAYER) {
        videoPlayer.get()->frameStep();
    } else if(imageViewer->hasAnimation()) {
        imageViewer->stopAnimation();
        imageViewer->nextFrame();
    }
}

void ViewerWidget::frameStepBack() {
    if(currentWidget == VIDEOPLAYER) {
        videoPlayer.get()->frameStepBack();
    } else if(imageViewer->hasAnimation()) {
        imageViewer->stopAnimation();
        imageViewer->prevFrame();
    }
}

void ViewerWidget::toggleMute() {
    if(currentWidget == VIDEOPLAYER) {
        videoPlayer->setMuted(!videoPlayer->muted());
        videoControls->onVideoMuted(videoPlayer->muted());
    }
}

void ViewerWidget::volumeUp() {
    if(currentWidget == VIDEOPLAYER) {
        videoPlayer->volumeUp();
        videoControls->setVolume(videoPlayer->volume());
    }
}

void ViewerWidget::volumeDown() {
    if(currentWidget == VIDEOPLAYER) {
        videoPlayer->volumeDown();
        videoControls->setVolume(videoPlayer->volume());
    }
}

void ViewerWidget::setVolume(int volume) {
    if(currentWidget == VIDEOPLAYER)
        videoPlayer->setVolume(volume);
}

void ViewerWidget::setPlaybackSpeed(double speed) {
    if(currentWidget == VIDEOPLAYER)
        videoPlayer->setPlaybackSpeed(speed);
}

// showing the clip pauses; a start at the playhead shows that exact frame
void ViewerWidget::toggleClip() {
    if(currentWidget != VIDEOPLAYER)
        return;
    const bool showing = !videoClip.isActive();
    const bool fresh = !videoClip.hasRange();
    const int oldStart = videoClip.start();
    if(showing && !mVideoPaused) {
        videoPlayer->setPaused(true);
        mVideoPaused = true;
    }
    videoClip.toggle(mVideoPosition);
    if(showing && videoClip.isActive() && (fresh || videoClip.start() != oldStart))
        seekVideoTo(videoClip.start());
}

void ViewerWidget::clearClip() {
    if(currentWidget == VIDEOPLAYER)
        videoClip.clear();
}

void ViewerWidget::setClipStart() {
    if(currentWidget == VIDEOPLAYER)
        videoClip.markStart(mVideoPosition);
}

void ViewerWidget::setClipEnd() {
    if(currentWidget == VIDEOPLAYER)
        videoClip.markEnd(mVideoPosition);
}

void ViewerWidget::toggleVideoLoop() {
    if(currentWidget == VIDEOPLAYER)
        videoClip.toggleLoop();
}

// mpv A-B loops a looping clip; a non-looping clip stops in onVideoPositionChanged()
void ViewerWidget::applyVideoClip() {
    const VideoClip::State state = videoClip.state();
    videoControls->setClip(videoClip.isActive() ? videoClip.start() : -1, videoClip.end());
    videoControls->setLoop(videoClip.loop());
    if(videoClip.isActive() && videoClip.loop())
        videoPlayer->setLoopAB(videoClip.start(), videoClip.end());
    else
        videoPlayer->setLoopAB(-1, -1);
    videoPlayer->setLoopPlayback(videoClip.loop() && !videoClip.isActive());
    // a clip shown again around a playhead outside it: go to its start
    if(state == VideoClip::CLIP_ACTIVE && mLastClipState != VideoClip::CLIP_ACTIVE &&
       (mVideoPosition < videoClip.start() || mVideoPosition > videoClip.end()))
        seekVideoTo(videoClip.start());
    mLastClipState = state;
}

void ViewerWidget::onVideoPositionChanged(int pos) {
    if(currentWidget != VIDEOPLAYER)
        return;
    mVideoPosition = pos;
    if(!mVideoPaused && videoClip.reachedEnd(pos)) {
        videoPlayer->setPaused(true);
        mVideoPaused = true;
        seekVideoTo(videoClip.end());
    }
}

// grabbing a handle pauses the video on the handle's frame
void ViewerWidget::onClipHandlePressed(int handle) {
    if(currentWidget != VIDEOPLAYER || !videoClip.isActive())
        return;
    if(!mVideoPaused) {
        videoPlayer->setPaused(true);
        mVideoPaused = true;
    }
    previewSeek((VideoClip::Handle(handle) == VideoClip::HANDLE_START) ? videoClip.start() : videoClip.end());
}

void ViewerWidget::onClipHandleMoved(int handle, int pos) {
    if(currentWidget != VIDEOPLAYER || !videoClip.isActive())
        return;
    const auto which = VideoClip::Handle(handle);
    videoClip.moveHandle(which, pos);
    previewSeek((which == VideoClip::HANDLE_START) ? videoClip.start() : videoClip.end());
}

// throttled to PREVIEW_SEEK_MS while dragging; the last position always seeks
void ViewerWidget::previewSeek(int pos) {
    mPreviewSeekTarget = pos;
    if(mPreviewSeekTimer.isActive())
        return;
    mPreviewSeekSent = pos;
    seekVideoTo(pos);
    mPreviewSeekTimer.start();
}

bool ViewerWidget::isDisplaying() {
    if(currentWidget == IMAGEVIEWER && imageViewer->isDisplaying())
        return true;
    if(currentWidget == VIDEOPLAYER)
        return true;
    else
        return false;
}

bool ViewerWidget::lockZoomEnabled() {
    return imageViewer->lockZoomEnabled();
}

bool ViewerWidget::lockViewEnabled() {
    return imageViewer->lockViewEnabled();
}

ScalingFilter ViewerWidget::scalingFilter() {
    return imageViewer->scalingFilter();
}

bool ViewerWidget::toggleScalingEngine() {
    return imageViewer->toggleScalingEngine();
}

void ViewerWidget::mousePressEvent(QMouseEvent *event) {
    hideContextMenu();
    event->ignore();
}

void ViewerWidget::mouseReleaseEvent(QMouseEvent *event) {
    showCursor();
    hideCursorTimed(false);
    event->ignore();
}

void ViewerWidget::mouseMoveEvent(QMouseEvent *event) {
    mWaylandCursorWorkaround = false;
    if(!(event->buttons() & Qt::LeftButton) && !(event->buttons() & Qt::RightButton)) {
        showCursor();
        hideCursorTimed(true);
    }
    event->ignore();
}

void ViewerWidget::hideCursorTimed(bool restartTimer) {
    if(restartTimer || !cursorTimer.isActive())
        cursorTimer.start(CURSOR_HIDE_TIMEOUT_MS);
}

void ViewerWidget::hideCursor() {
    cursorTimer.stop();
    // ignore if we have something else open like settings window
    if(!isDisplaying() || !isActiveWindow())
        return;
    // ignore when menu is up
    if(contextMenu && contextMenu->isVisible())
        return;
    const CursorAutohideMode autohide = settings->cursorAutohide();
    if(autohide == CURSOR_AUTOHIDE_ALWAYS || (autohide == CURSOR_AUTOHIDE_FULLSCREEN && mIsFullscreen)) {
        // force hide on wayland until we can get the cursor pos
        if(mWaylandCursorWorkaround) {
            setCursor(QCursor(Qt::BlankCursor));
        } else {
            QPoint posMapped = mapFromGlobal(QCursor::pos());
            // ignore when we are hovering the click zone
            if(clickZoneOverlay->leftZone().contains(posMapped) ||
                clickZoneOverlay->rightZone().contains(posMapped))
            {
                return;
            }

            // only hide when we are under viewer or player widget
            QWidget *w = qApp->widgetAt(QCursor::pos());
            if(w && (w == imageViewer.get()->viewport() || w == videoPlayer->getPlayer().get())) {
                setCursor(QCursor(Qt::BlankCursor));
                // also blank the video player's own (native) window
                videoPlayer->setCursor(QCursor(Qt::BlankCursor));
            }
        }
    }
}

// overlay mouse events don't reach the ImageViewer, so click zones are handled here
ActiveHighlightZone ViewerWidget::clickZoneAt(const QPoint &pos) {
    if(clickZoneOverlay->leftZone().contains(pos))
        return HIGHLIGHT_LEFT;
    if(clickZoneOverlay->rightZone().contains(pos))
        return HIGHLIGHT_RIGHT;
    return HIGHLIGHT_NONE;
}

// the action bound to click in zone (LMB_LeftEdge / LMB_RightEdge with modifiers)
QString ViewerWidget::clickZoneAction(ActiveHighlightZone zone, const QString &click) {
    const QString edge = (zone == HIGHLIGHT_LEFT) ? QStringLiteral("_LeftEdge") : QStringLiteral("_RightEdge");
    return actionManager->actionForShortcut(MODE_DOCUMENT, click + edge);
}

bool ViewerWidget::eventFilter(QObject *object, QEvent *event) {
    // edge clicks run their binding; a double-click counts as another click; unbound passes through
    if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        // disable feature for very small windows
        if(width() <= 250)
            return false;

        auto mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if(mouseEvent->button() != Qt::LeftButton) {
            clickZoneOverlay->disableHighlight();
            return false;
        }
        const ActiveHighlightZone zone = clickZoneAt(mouseEvent->pos());
        if(zone != HIGHLIGHT_NONE) {
            QString click = ShortcutBuilder::fromEvent(mouseEvent);
            click.remove(QStringLiteral("_DoubleClick"));
            const QString action = clickZoneAction(zone, click);
            if(action.isEmpty()) {
                clickZoneOverlay->disableHighlight();
                return false;
            }
            clickZoneOverlay->setPressed(true);
            clickZoneOverlay->setHighlightedZone(zone);
            imageViewer.get()->disableDrags();
            actionManager->invokeAction(action);
            return true; // do not pass the event to imageViewer
        }
    }
    // right click arrives as QEvent::ContextMenu, not a QMouseEvent
    if(event->type() == QEvent::ContextMenu) {
        clickZoneOverlay->disableHighlight();
        return false;
    }

    if(event->type() == QEvent::MouseButtonRelease) {
        clickZoneOverlay->setPressed(false);
        imageViewer.get()->enableDrags();
    }

    if(event->type() == QEvent::MouseMove || event->type() == QEvent::Enter) {
        QPoint mousePos;
        if(event->type() == QEvent::MouseMove) {
            auto mouseEvent = dynamic_cast<QMouseEvent*>(event);
            mousePos = mouseEvent->pos();
            if(mouseEvent->buttons())
                return false;
        } else {
            auto enterEvent = dynamic_cast<QEnterEvent*>(event);
            mousePos = enterEvent->position().toPoint();
        }
        // only an edge whose plain click is bound lights up
        const ActiveHighlightZone zone = clickZoneAt(mousePos);
        if(zone != HIGHLIGHT_NONE && !clickZoneAction(zone, QStringLiteral("LMB")).isEmpty()) {
            clickZoneOverlay->setPressed(false);
            clickZoneOverlay->setHighlightedZone(zone);
            setCursor(Qt::PointingHandCursor);
            return true;
        } else {
            clickZoneOverlay->disableHighlight();
            setCursor(Qt::ArrowCursor);
        }
    }

    if(event->type() == QEvent::Leave) {
        clickZoneOverlay->disableHighlight();
        setCursor(Qt::ArrowCursor);
    }

    return false; // send event to imageViewer / videoplayer
}

void ViewerWidget::showCursor() {
    cursorTimer.stop();
    if(cursor().shape() == Qt::BlankCursor)
        setCursor(QCursor(Qt::ArrowCursor));
    // the player's native window doesn't inherit our cursor
    if(videoPlayer->cursor().shape() == Qt::BlankCursor)
        videoPlayer->setCursor(QCursor(Qt::ArrowCursor));
}

void ViewerWidget::showContextMenu() {
    QPoint pos = cursor().pos();
    showContextMenu(pos);
}

void ViewerWidget::showContextMenu(QPoint pos) {
    if(isVisible() && interactionEnabled()) {
        if(!contextMenu) {
            contextMenu.reset(new ContextMenu(this));
            connect(contextMenu.get(), &ContextMenu::showScriptSettings, this, &ViewerWidget::showScriptSettings);
        }
        contextMenu->setImageEntriesEnabled(isDisplaying());
        if(!contextMenu->isVisible())
            contextMenu->showAt(pos);
        else
            contextMenu->hide();
    }
}

void ViewerWidget::onFullscreenModeChanged(bool mode) {
    imageViewer->onFullscreenModeChanged(mode);
    mIsFullscreen = mode;
    if(!mode && settings->cursorAutohide() == CURSOR_AUTOHIDE_FULLSCREEN)
        showCursor();
    else
        hideCursorTimed(false);
}

void ViewerWidget::readSettings() {
    videoControls->onVideoMuted(!settings->playVideoSounds());
    updateVideoControlsVisibility();
    if(settings->clickableEdges()) {
        imageViewer->viewport()->installEventFilter(this);
        videoPlayer->installEventFilter(this);
        clickZoneOverlay->show();
    } else {
        imageViewer->viewport()->removeEventFilter(this);
        videoPlayer->removeEventFilter(this);
        imageViewer.get()->enableDrags();
        clickZoneOverlay->hide();
    }
}

// the slideshow turns looping off; a video's own loop button is left alone
void ViewerWidget::setLoopPlayback(bool mode) {
    imageViewer->setLoopPlayback(mode);
    if(!mode)
        videoClip.setLoop(false);
}

void ViewerWidget::hideContextMenu() {
    if(contextMenu)
        contextMenu->hide();
}

void ViewerWidget::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    hideContextMenu();
}

// block native tab-switching so we can use it in shortcuts
bool ViewerWidget::focusNextPrevChild(bool mode) {
    return false;
}

void ViewerWidget::keyPressEvent(QKeyEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void ViewerWidget::scrollUp()        { scrollView(0, -1, false); }
void ViewerWidget::scrollDown()      { scrollView(0,  1, false); }
void ViewerWidget::scrollLeft()      { scrollView(-1, 0, false); }
void ViewerWidget::scrollRight()     { scrollView( 1, 0, false); }
void ViewerWidget::scrollUpFine()    { scrollView(0, -1, true); }
void ViewerWidget::scrollDownFine()  { scrollView(0,  1, true); }
void ViewerWidget::scrollLeftFine()  { scrollView(-1, 0, true); }
void ViewerWidget::scrollRightFine() { scrollView( 1, 0, true); }

// Scrolls text by lines and pans overflowing images, stopping at edges; otherwise runs the
// action's fallback (Settings > Shortcuts), a video its video fallback.
void ViewerWidget::scrollView(int dx, int dy, bool fine) {
    if(currentWidget == TEXTVIEWER && dy && textViewer->canScroll()) {
        textViewer->scrollLines(dy * (fine ? 1 : QApplication::wheelScrollLines()));
        return;
    }
    if(currentWidget == IMAGEVIEWER && mInteractionEnabled && imageViewer->isDisplaying()) {
        if(imageViewer->keyPan(dx, dy, fine))
            return;
        if(dy < 0 && !fine && imageViewer->fitMode() == FIT_WINDOW && imageViewer->scaledImageFits()) {
            imageViewer->setFitWidth();
            return;
        }
    }
    QString action = (dx < 0) ? "scrollLeft" : (dx > 0) ? "scrollRight" : (dy < 0) ? "scrollUp" : "scrollDown";
    if(fine)
        action += "Fine";
    const QString fallback = (currentWidget == VIDEOPLAYER) ? actionManager->videoScrollFallback(action)
                                                            : actionManager->scrollFallback(action);
    if(!fallback.isEmpty())
        actionManager->invokeAction(fallback);
}

void ViewerWidget::scrollOrPrevImage() { scrollOrChangeFile(-1); }
void ViewerWidget::scrollOrNextImage() { scrollOrChangeFile( 1); }

// Text and (trackpad-mode) tall images scroll and stop at their ends; a video runs the video
// fallback, anything else changes file.
void ViewerWidget::scrollOrChangeFile(int dy) {
    if(currentWidget == TEXTVIEWER && textViewer->canScroll()) {
        textViewer->scrollLines(dy * QApplication::wheelScrollLines());
        return;
    }
    if(currentWidget == IMAGEVIEWER && mInteractionEnabled && imageViewer->isDisplaying() &&
       settings->imageScrolling() == SCROLL_BY_TRACKPAD_AND_WHEEL && imageViewer->keyPan(0, dy, false))
        return;
    if(currentWidget == VIDEOPLAYER) {
        const QString fallback = actionManager->videoScrollFallback((dy < 0) ? "scrollOrPrevImage" : "scrollOrNextImage");
        if(!fallback.isEmpty())
            actionManager->invokeAction(fallback);
        return;
    }
    actionManager->invokeAction((dy < 0) ? "prevImage" : "nextImage");
}

void ViewerWidget::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
}

QWidget *ViewerWidget::videoControlsWidget() {
    return videoControls;
}

bool ViewerWidget::shouldShowVideoControls() {
    return settings->showVideoControls() &&
        (currentWidget == VIDEOPLAYER || (currentWidget == IMAGEVIEWER && imageViewer->hasAnimation()));
}

void ViewerWidget::updateVideoControlsVisibility() {
    if(shouldShowVideoControls())
        videoControls->show();
    else
        videoControls->hide();
}
