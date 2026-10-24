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
    // Reflect the real audio-output volume (reported by the player) on the
    // controls slider, so it matches the actual OS/per-app sound level.
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
        // Hide the player when leaving it. Otherwise its last decoded frame
        // lingers and briefly flashes when we switch to an image.
        // The old "do not hide, late GL frames corrupt the imageviewer" caveat
        // applied to the OpenGL render-API backend; the default wid backend
        // renders into its own native window, so hiding is clean.
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
        // Coming from a video. The mpv native window stacks above our Qt widgets,
        // so paint the new image into the (still-occluded) image viewer FIRST and
        // flush it, then hide the video window. That way the image is already in
        // the framebuffer the instant the video window is unmapped -- otherwise
        // there's a one-frame gap where the background shows through.
        imageViewer->show();
        imageViewer->showImage(std::move(pixmap));
        imageViewer->repaint();
        enableImageViewer();
    } else {
        enableImageViewer();
        // enableImageViewer() only show()s on a widget switch; re-show explicitly
        // in case prepareForLoad() hid the image viewer while we were already on
        // it (e.g. a misdetected video that turned out to be an image).
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
    // Restore the pointer on every switch; it auto-hides again after the idle
    // timeout. Without this the cursor stays blanked from the previous item
    // (the mpv window doesn't track our auto-hide on its own).
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

// Called when navigating to another item, before the next one is loaded (the
// load is usually async). Blanks the outgoing view so it doesn't linger on
// screen during the load, but only across an image<->video change; plain
// image -> image keeps its current frame so browsing stays flicker-free.
void ViewerWidget::prepareForLoad(bool nextIsVideo) {
    if(currentWidget == VIDEOPLAYER) {
        // Leaving a video. Pause it so audio stops, but do NOT stop()/unload the
        // file: unloading tears down mpv's video output, and re-creating it on
        // the next load crashes on machines with broken GL acceleration. A plain
        // reload (loadfile) reuses the existing output and is safe.
        //
        // We deliberately do NOT blank the video here. showImage()/showVideo()
        // swap to the next item atomically, and Core loads a video -> image
        // switch synchronously (see loadFileIndex), so the new image is on
        // screen the instant the video is hidden -- no empty/background frame
        // flashes in between, and no stale video frame lingers either.
        videoPlayer->setPaused(true);
    } else if(currentWidget == IMAGEVIEWER && nextIsVideo) {
        // image -> video: blank the image so it doesn't linger during the
        // (async) video load; the plugin blanks the player until its first frame.
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

// With an active clip, play resumes inside it, and starts over from the clip's
// start when stopped outside it or at its end.
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
        if(videoClip.isEditing())
            stepClipHandle(1);
        else
            videoPlayer.get()->frameStep();
    } else if(imageViewer->hasAnimation()) {
        imageViewer->stopAnimation();
        imageViewer->nextFrame();
    }
}

void ViewerWidget::frameStepBack() {
    if(currentWidget == VIDEOPLAYER) {
        if(videoClip.isEditing())
            stepClipHandle(-1);
        else
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

void ViewerWidget::toggleClipEdit() {
    if(currentWidget == VIDEOPLAYER)
        videoClip.toggleEdit();
}

void ViewerWidget::commitClipEdit() {
    if(currentWidget == VIDEOPLAYER)
        videoClip.commitEdit();
}

void ViewerWidget::cancelClipEdit() {
    if(currentWidget == VIDEOPLAYER)
        videoClip.cancelEdit();
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

// Shows the clip on the controls and hands it to the player: mpv repeats an
// active clip that loops (A-B loop), and without a clip loop repeats the file.
// A clip that doesn't loop is stopped at its end in onVideoPositionChanged().
void ViewerWidget::applyVideoClip() {
    const VideoClip::State state = videoClip.state();
    videoControls->setClip(videoClip.hasRange() ? videoClip.start() : -1, videoClip.end(), videoClip.isEditing());
    videoControls->setLoop(videoClip.loop());
    if(videoClip.isActive() && videoClip.loop())
        videoPlayer->setLoopAB(videoClip.start(), videoClip.end());
    else
        videoPlayer->setLoopAB(-1, -1);
    videoPlayer->setLoopPlayback(videoClip.loop() && !videoClip.isActive());
    // Esc / Enter cancel / commit while editing
    if(videoClip.isEditing())
        actionManager->setOverlayContext(MODE_CLIPEDIT);
    else
        actionManager->clearOverlayContext();
    // leaving edit mode: the clip is ready to play from its start
    if(state == VideoClip::CLIP_ACTIVE && mLastClipState == VideoClip::CLIP_EDITING)
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
    if(currentWidget != VIDEOPLAYER || !videoClip.isEditing())
        return;
    if(!mVideoPaused) {
        videoPlayer->setPaused(true);
        mVideoPaused = true;
    }
    const auto which = VideoClip::Handle(handle);
    const int pos = (which == VideoClip::HANDLE_START) ? videoClip.start() : videoClip.end();
    // marks it as the one a frame step moves
    videoClip.moveHandle(which, pos);
    previewSeek(pos);
}

void ViewerWidget::onClipHandleMoved(int handle, int pos) {
    if(currentWidget != VIDEOPLAYER || !videoClip.isEditing())
        return;
    const auto which = VideoClip::Handle(handle);
    videoClip.moveHandle(which, pos);
    previewSeek((which == VideoClip::HANDLE_START) ? videoClip.start() : videoClip.end());
}

// to the next / previous frame boundary, shown paused
void ViewerWidget::stepClipHandle(int frames) {
    const double fps = videoPlayer->frameRate();
    const double frame = 1000.0 / (fps > 0 ? fps : 25.0);
    const VideoClip::Handle which = videoClip.lastHandle();
    const int pos = (which == VideoClip::HANDLE_START) ? videoClip.start() : videoClip.end();
    videoClip.moveHandle(which, qRound((qRound(pos / frame) + frames) * frame));
    if(!mVideoPaused) {
        videoPlayer->setPaused(true);
        mVideoPaused = true;
    }
    previewSeek((which == VideoClip::HANDLE_START) ? videoClip.start() : videoClip.end());
}

// Seeks to pos at most every PREVIEW_SEEK_MS while a handle is dragged; the
// last position is always seeked to.
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
            //if(settings->enableClickZoneThing())
            // ignore when we are hovering the click zone
            if(clickZoneOverlay->leftZone().contains(posMapped) ||
                clickZoneOverlay->leftZone().contains(posMapped))
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

// click zone input crutch
// --
// we can't process mouse events in the overlay
// cause they won't propagate to the ImageViewer, only to overlay's container (this widget)
// so we just grab them before they reach ImageViewer and do the needful
ActiveHighlightZone ViewerWidget::clickZoneAt(const QPoint &pos) {
    if(clickZoneOverlay->leftZone().contains(pos))
        return HIGHLIGHT_LEFT;
    if(clickZoneOverlay->rightZone().contains(pos))
        return HIGHLIGHT_RIGHT;
    return HIGHLIGHT_NONE;
}

// the action bound to click (e.g. "LMB", "Ctrl+LMB") in zone: the bindings
// LMB_LeftEdge / LMB_RightEdge, with any modifiers in front
QString ViewerWidget::clickZoneAction(ActiveHighlightZone zone, const QString &click) {
    const QString edge = (zone == HIGHLIGHT_LEFT) ? QStringLiteral("_LeftEdge") : QStringLiteral("_RightEdge");
    return actionManager->actionForShortcut(MODE_DOCUMENT, click + edge);
}

bool ViewerWidget::eventFilter(QObject *object, QEvent *event) {
    // A left click in an edge zone runs its LMB_LeftEdge / LMB_RightEdge
    // binding (previous / next file by default) instead of reaching the
    // picture; a double-click counts as one more click, so quick clicks keep
    // stepping. Unbound, it is an ordinary click on the picture.
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
    // right click produces QEvent::ContextMenu instead of QEvent::MouseButtonPress
    // this is NOT a QMouseEvent
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
    // The video player hosts its own (native) window which doesn't inherit our
    // cursor changes, so set it explicitly too. Otherwise the pointer stays
    // hidden over a playing video even after we mean to show it.
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
    // a pointer hidden only in full screen comes back on leaving it; entering
    // it hides an idle one
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

// The slideshow turns looping off so it can move on. A video's loop is its
// own button's (off for every new file), so ending the slideshow leaves it be.
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

// Text that can scroll scrolls vertically by wheel-lines (fine: one line),
// stopping at its ends; an image pans only along an axis it overflows, stopping
// at its edge, and up switches a window-fitted image to fit-width. When there
// is nothing to scroll that way the action runs its fallback (Settings >
// Shortcuts): by default left/right change file and fine left/right seek a
// video. A video has its own fallbacks, by default seeking for both.
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

// A text file that can scroll scrolls by wheel-lines and stops at its ends, so
// the end of a long file doesn't open the next one; with image scrolling set to
// trackpad and wheel, an image taller than the window pans the same way. A
// video runs the action's video fallback (seek by default); anything else
// changes file. Over the text or image itself the viewer does the
// wheel scrolling (see TextViewer and ImageViewerV2::wheelEvent).
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
