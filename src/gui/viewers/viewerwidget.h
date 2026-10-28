#pragma once

#include "gui/customwidgets/floatingwidgetcontainer.h"
#include <QVBoxLayout>
#include <cstdint>
#include "gui/viewers/imageviewerv2.h"
#include "gui/viewers/textviewer.h"
#include "gui/viewers/videoclip.h"
#include "gui/viewers/videoplayerinitproxy.h"
#include "gui/overlays/videocontrolsproxy.h"
#include "gui/overlays/zoomindicatoroverlayproxy.h"
#include "gui/overlays/clickzoneoverlay.h"
#include "gui/contextmenu.h"

enum CurrentWidget : std::uint8_t {
    IMAGEVIEWER,
    VIDEOPLAYER,
    TEXTVIEWER,
    UNSET
};

class ViewerWidget : public FloatingWidgetContainer
{
    Q_OBJECT
public:
    explicit ViewerWidget(QWidget *parent = nullptr);
    QRect imageRect();
    float currentScale();
    QSize sourceSize();

    void setInteractionEnabled(bool mode);
    bool interactionEnabled();

    bool showImage(std::unique_ptr<QPixmap> pixmap);
    bool showAnimation(const std::shared_ptr<QMovie>& movie);
    void onScalingFinished(std::unique_ptr<QPixmap> scaled);
    bool isDisplaying();
    bool isShowingVideo() const;
    bool lockZoomEnabled();
    bool lockViewEnabled();
    ScalingFilter scalingFilter();
    bool toggleScalingEngine();
    QWidget *videoControlsWidget();

private:
    void scrollView(int dx, int dy, bool fine);
    void scrollOrChangeFile(int dy);
    QVBoxLayout layout;
    std::unique_ptr<ImageViewerV2> imageViewer;
    std::unique_ptr<VideoPlayerInitProxy> videoPlayer;
    std::unique_ptr<TextViewer> textViewer;
    std::unique_ptr<ContextMenu> contextMenu;
    VideoControlsProxyWrapper *videoControls;
    ZoomIndicatorOverlayProxy *zoomIndicator;
    ClickZoneOverlay *clickZoneOverlay;

    void enableImageViewer();
    void enableVideoPlayer();
    void enableTextViewer();

    CurrentWidget currentWidget;
    bool mInteractionEnabled, mWaylandCursorWorkaround;
    QTimer cursorTimer;
    const int CURSOR_HIDE_TIMEOUT_MS = 1000;
    bool mIsFullscreen;

    void disableImageViewer();
    void disableVideoPlayer();
    void disableTextViewer();

    bool shouldShowVideoControls();
    void updateVideoControlsVisibility();

    // the shown video's clip; applyVideoClip() puts it on the controls and the
    // player whenever it changes
    VideoClip videoClip;
    VideoClip::State mLastClipState = VideoClip::CLIP_NONE;
    int mVideoPosition = 0;
    bool mVideoPaused = true;
    // a dragged handle's frame is shown, at most every PREVIEW_SEEK_MS
    QTimer mPreviewSeekTimer;
    int mPreviewSeekTarget = -1;
    int mPreviewSeekSent = -1;
    static constexpr int PREVIEW_SEEK_MS = 40;
    void applyVideoClip();
    // a frame step while editing moves the last-touched handle a frame
    void previewSeek(int pos);
    void seekVideoTo(int pos);

    bool eventFilter(QObject *object, QEvent *event) override;
    ActiveHighlightZone clickZoneAt(const QPoint &pos);
    QString clickZoneAction(ActiveHighlightZone zone, const QString &click);

private slots:
    void onScaleChanged(qreal);
    void onVideoPlaybackFinished();
    void onAnimationPlaybackFinished();
    void onVideoPositionChanged(int pos);
    void onClipHandlePressed(int handle);
    void onClipHandleMoved(int handle, int pos);

signals:
    void scalingRequested(QSize, ScalingFilter);
    void zoomIn();
    void zoomOut();
    void zoomInCursor();
    void zoomOutCursor();
    void fitWindow();
    void fitWidth();
    void fitOriginal();
    void fitWindowStretch();
    void toggleTransparencyGrid();
    void draggedOut();
    void setFilterNearest();
    void setFilterBilinear();
    void setScalingFilter(ScalingFilter filter);
    void playbackFinished();
    void toggleLockZoom();
    void toggleLockView();
    void showScriptSettings();
    void scaleChanged();

public slots:
    // Scroll the text viewer / pan a zoomed image (Fine: small steps). When
    // there is nothing to scroll that way the key keeps its old meaning:
    // left/right go to the previous/next file, fine left/right seek a video.
    void scrollUp();
    void scrollDown();
    void scrollLeft();
    void scrollRight();
    void scrollUpFine();
    void scrollDownFine();
    void scrollLeftFine();
    void scrollRightFine();
    // The wheel's actions: scroll a text file that can scroll, stopping at its
    // ends; anywhere else go to the previous/next file.
    void scrollOrPrevImage();
    void scrollOrNextImage();

    bool showVideo(QString file);
    bool showText(const QString& file);
    void stopPlayback();
    void setFitMode(ImageFitMode mode);
    ImageFitMode fitMode();
    void closeImage();
    void prepareForLoad(bool nextIsVideo);
    void hideCursor();
    void showCursor();
    void hideCursorTimed(bool restartTimer);

    // video control
    void pauseResumePlayback();
    void seek(int pos);
    void seekRelative(int pos);
    void seekBackward();
    void seekForward();
    void frameStep();
    void frameStepBack();
    void toggleMute();
    void volumeUp();
    void volumeDown();
    void setVolume(int volume);
    void setPlaybackSpeed(double speed);
    // the clip: the bracket button / its keys, and loop (clip or whole file)
    void toggleClip();
    void clearClip();
    void setClipStart();
    void setClipEnd();
    void toggleVideoLoop();

    void startPlayback();
    void showContextMenu();
    void hideContextMenu();
    void showContextMenu(QPoint pos);
    void onFullscreenModeChanged(bool);
    void readSettings();
    void setLoopPlayback(bool mode);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void hideEvent(QHideEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool focusNextPrevChild(bool mode) override;
};
