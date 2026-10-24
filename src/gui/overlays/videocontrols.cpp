#include "videocontrols.h"
#include "ui_videocontrols.h"
#include "gui/viewers/videoclip.h"
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QWidget *createVideoControlSeparator(QWidget *parent) {
    QWidget *separator = new QWidget(parent);
    separator->setAccessibleName("VideoControlSeparator");
    separator->setFixedSize(1, 14);
    return separator;
}

// a toggle that is on: pressed look, icon in the seek bar's clip color
void setButtonOn(ActionButton *button, bool on, const QColor &onColor) {
    button->setProperty("checked", on);
    if(on) {
        button->setColor(onColor);
    } else {
        button->setColor(settings->colorScheme().icons);
        button->setColorMode(ICON_COLOR_THEME);
    }
    button->style()->unpolish(button);
    button->style()->polish(button);
}
}

VideoControls::VideoControls(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VideoControls),
    mode(PLAYBACK_VIDEO),
    clipButton(new ActionButton(this)),
    loopButton(new ActionButton(this)),
    speedButton(new QPushButton("1.00x", this)),
    resetSpeedButton(new QPushButton(tr("Reset 1.00x"), this)),
    volumeSlider(new QSlider(Qt::Horizontal, this)),
    speedSlider(new QSlider(Qt::Horizontal)),
    speedPopupLabel(new QLabel("1.00x")),
    speedPopup(new QFrame(this, Qt::Popup))
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_NoMousePropagation, true);
    setFixedHeight(26);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    ui->horizontalLayout->setContentsMargins(3, 1, 3, 1);
    ui->horizontalLayout->setSpacing(3);
    ui->horizontalLayout_2->setSpacing(2);
    ui->horizontalSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    ui->horizontalSpacer_2->changeSize(4, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    speedPopup->setAccessibleName("VideoControlPopup");
    auto *speedPopupLayout = new QVBoxLayout(speedPopup);
    speedPopupLayout->setContentsMargins(8, 8, 8, 8);
    speedPopupLayout->setSpacing(6);
    hide();
    ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/play24.png");
    ui->pauseButton->setAction("pauseVideo");
    ui->pauseButton->setToolTip(tr("Play / pause"));
    ui->prevFrameButton->setIconPath(":res/icons/common/buttons/videocontrols/skip-backwards24.png");
    ui->prevFrameButton->setAction("frameStepBack");
    ui->prevFrameButton->setToolTip(tr("Previous frame"));
    ui->nextFrameButton->setIconPath(":res/icons/common/buttons/videocontrols/skip-forward24.png");
    ui->nextFrameButton->setAction("frameStep");
    ui->nextFrameButton->setToolTip(tr("Next frame"));
    ui->muteButton->setIconPath(":/res/icons/common/buttons/videocontrols/mute-on24.png");
    ui->muteButton->setAction("");
    ui->muteButton->setToolTip(tr("Mute / unmute"));
    ui->pauseButton->setFixedSize(24, 24);
    ui->prevFrameButton->setFixedSize(24, 24);
    ui->nextFrameButton->setFixedSize(24, 24);
    ui->muteButton->setFixedSize(24, 24);
    ui->seekBar->setAccessibleName("VideoControlSeekSlider");
    ui->seekBar->setToolTip(tr("Seek"));
    ui->seekBar->setMinimumSize(81, 24);
    ui->seekBar->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->seekBar->setMaximumHeight(24);
    ui->seekBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont timeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    timeFont.setPointSize(qMax(8, timeFont.pointSize() - 1));
    ui->positionLabel->setFont(timeFont);
    ui->durationLabel->setFont(timeFont);
    ui->positionLabel->setFixedWidth(50);
    ui->durationLabel->setFixedWidth(50);
    ui->positionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->durationLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    clipButton->setObjectName("clipButton");
    clipButton->setAccessibleName("Button");
    clipButton->setAction("toggleClipEdit");
    clipButton->setFixedSize(24, 24);
    // right-click clears the clip
    clipButton->installEventFilter(this);
    loopButton->setObjectName("loopButton");
    loopButton->setAccessibleName("Button");
    loopButton->setIconPath(":res/icons/common/buttons/videocontrols/loop24.png");
    loopButton->setAction("toggleVideoLoop");
    loopButton->setFixedSize(24, 24);
    ui->horizontalLayout_2->addSpacing(3);
    ui->horizontalLayout_2->addWidget(clipButton);
    ui->horizontalLayout_2->addWidget(loopButton);
    updateClipButton(false, false);
    setLoop(false);
    ui->seekBar->setClipMinGap(VideoClip::MIN_GAP_MS);

    ui->horizontalLayout->insertWidget(1, createVideoControlSeparator(this));

    volumeSlider->setAccessibleName("VideoControlVolumeSlider");
    volumeSlider->setFocusPolicy(Qt::NoFocus);
    volumeSlider->setRange(0, 100);
    volumeSlider->setSingleStep(5);
    volumeSlider->setPageStep(10);
    volumeSlider->setFixedSize(60, 24);
    volumeSlider->setValue(settings->volume());
    updateVolumeText(volumeSlider->value());
    ui->horizontalLayout->insertWidget(3, volumeSlider);

    ui->horizontalLayout->insertWidget(4, createVideoControlSeparator(this));

    speedButton->setAccessibleName("VideoControlTextButton");
    speedButton->setFocusPolicy(Qt::NoFocus);
    speedButton->setToolTip(tr("Playback speed"));
    speedButton->setFixedSize(44, 24);
    ui->horizontalLayout->insertWidget(5, speedButton);

    speedSlider->setAccessibleName("VideoControlSpeedSlider");
    speedSlider->setFocusPolicy(Qt::NoFocus);
    speedSlider->setToolTip(tr("Playback speed"));
    speedSlider->setRange(25, 400);
    speedSlider->setSingleStep(25);
    speedSlider->setPageStep(25);
    speedSlider->setTickPosition(QSlider::TicksBelow);
    speedSlider->setTickInterval(25);
    speedSlider->setFixedSize(180, 28);
    speedSlider->setValue(100);
    resetSpeedButton->setAccessibleName("VideoControlPopupButton");
    resetSpeedButton->setFocusPolicy(Qt::NoFocus);
    resetSpeedButton->setFixedSize(180, 24);
    speedPopupLabel->setAlignment(Qt::AlignCenter);
    speedPopupLabel->setToolTip(tr("Playback speed"));
    speedPopupLayout->addWidget(speedPopupLabel);
    speedPopupLayout->addWidget(resetSpeedButton);
    speedPopupLayout->addWidget(speedSlider);
    ui->horizontalLayout->insertWidget(6, createVideoControlSeparator(this));

    lastPosition = -1;

    connect(ui->seekBar, &VideoSlider::sliderMovedX, this, &VideoControls::seek);
    connect(ui->seekBar, &VideoSlider::clipHandlePressed, this, &VideoControls::clipHandlePressed);
    connect(ui->seekBar, &VideoSlider::clipHandleMoved, this, &VideoControls::clipHandleMoved);
    // on press, not release: a click's press-to-release time would add to the
    // time it takes the sound to stop
    connect(ui->muteButton, &ActionButton::pressed, this, &VideoControls::toggleMuteRequested);
    connect(volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        updateVolumeText(value);
        emit volumeChanged(value);
    });
    connect(speedSlider, &QSlider::valueChanged, this, [this](int value) {
        const double speed = value / 100.0;
        updateSpeedText(value);
        emit playbackSpeedChanged(speed);
    });
    connect(speedButton, &QPushButton::clicked, this, [this]() {
        showPopupBelow(speedButton, speedPopup);
    });
    connect(resetSpeedButton, &QPushButton::clicked, this, &VideoControls::resetPlaybackSpeed);
    updateResponsiveVisibility();
}

VideoControls::~VideoControls() {
    delete ui;
}

void VideoControls::setMode(PlaybackMode _mode) {
    mode = _mode;
    ui->muteButton->setVisible( (mode == PLAYBACK_VIDEO) );
    speedButton->setVisible( (mode == PLAYBACK_VIDEO) );
    clipButton->setVisible( (mode == PLAYBACK_VIDEO) );
    loopButton->setVisible( (mode == PLAYBACK_VIDEO) );
    if(mode != PLAYBACK_VIDEO) {
        speedPopup->hide();
        setClip(-1, -1, false);
    }
    updateResponsiveVisibility();
}

// a video's duration and position are in ms, an animation's in frames
void VideoControls::setPlaybackDuration(int duration) {
    QString durationStr;
    if(mode == PLAYBACK_VIDEO) {
        int _time = duration / 1000;
        int hours = _time / 3600;
        _time -= hours * 3600;
        int minutes = _time / 60;
        int seconds = _time - minutes * 60;
        durationStr = QString("%1").arg(minutes, 2, 10, QChar('0')) + ":" +
                      QString("%1").arg(seconds, 2, 10, QChar('0'));
        if(hours)
            durationStr.prepend(QString("%1").arg(hours, 2, 10, QChar('0')) + ":");
    } else {
        durationStr = QString::number(duration);
    }
    ui->seekBar->setRange(0, (mode == PLAYBACK_VIDEO) ? qMax(0, duration) : duration - 1);
    ui->durationLabel->setText(durationStr);
    ui->positionLabel->setText(durationStr);
    updateGeometry();
    ui->positionLabel->setText("");
    shownSeconds = -1;
}

void VideoControls::setPlaybackPosition(int position) {
    if(position == lastPosition)
        return;
    if(mode == PLAYBACK_VIDEO) {
        int _time = position / 1000;
        if(_time != shownSeconds) {
            shownSeconds = _time;
            int hours = _time / 3600;
            _time -= hours * 3600;
            int minutes = _time / 60;
            int seconds = _time - minutes * 60;
            QString positionStr = QString("%1").arg(minutes, 2, 10, QChar('0')) + ":" +
                                  QString("%1").arg(seconds, 2, 10, QChar('0'));
            if(hours)
                positionStr.prepend(QString("%1").arg(hours, 2, 10, QChar('0')) + ":");
            ui->positionLabel->setText(positionStr);
        }
    } else {
        ui->positionLabel->setText(QString::number(position + 1));
    }
    ui->seekBar->blockSignals(true);
    ui->seekBar->setValue(position);
    ui->seekBar->blockSignals(false);
    lastPosition = position;
}

void VideoControls::onPlaybackPaused(bool mode) {
    if(mode)
        ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/play24.png");
    else
        ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/pause24.png");
}

void VideoControls::onVideoMuted(bool mode) {
    if(mode)
        ui->muteButton->setIconPath(":res/icons/common/buttons/videocontrols/mute-on24.png");
    else
        ui->muteButton->setIconPath(":res/icons/common/buttons/videocontrols/mute-off24.png");
}

void VideoControls::setVolume(int volume) {
    const QSignalBlocker blocker(volumeSlider);
    const int clampedVolume = qBound(0, volume, 100);
    volumeSlider->setValue(clampedVolume);
    updateVolumeText(clampedVolume);
}

void VideoControls::setClip(int start, int end, bool editing) {
    const bool clip = start >= 0;
    ui->seekBar->setClip(start, end, clip && editing);
    updateClipButton(clip, clip && editing);
}

void VideoControls::setLoop(bool mode) {
    setButtonOn(loopButton, mode, ui->seekBar->palette().color(QPalette::Highlight));
    loopButton->setToolTip(mode ? tr("Loop: on") : tr("Loop: off"));
}

void VideoControls::updateClipButton(bool clip, bool editing) {
    clipButton->setIconPath(editing ? ":res/icons/common/buttons/videocontrols/clip-edit24.png"
                                    : ":res/icons/common/buttons/videocontrols/clip24.png");
    if(editing)
        clipButton->setToolTip(tr("Done: play the clip between the handles (Esc cancels)"));
    else if(clip)
        clipButton->setToolTip(tr("Edit the clip (right-click clears it)"));
    else
        clipButton->setToolTip(tr("Clip: set a start and an end to play"));
    setButtonOn(clipButton, clip, ui->seekBar->palette().color(QPalette::Highlight));
}

bool VideoControls::eventFilter(QObject *watched, QEvent *event) {
    if(watched == clipButton && event->type() == QEvent::MouseButtonPress &&
       static_cast<QMouseEvent *>(event)->button() == Qt::RightButton) {
        actionManager->invokeAction("clearClip");
        return true;
    }
    if(watched == clipButton && event->type() == QEvent::MouseButtonRelease &&
       static_cast<QMouseEvent *>(event)->button() == Qt::RightButton)
        return true;
    return QWidget::eventFilter(watched, event);
}

void VideoControls::showPopupBelow(QWidget *anchor, QFrame *popup) {
    popup->adjustSize();
    QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height() + 2));
    popup->move(pos);
    popup->show();
}

void VideoControls::resetPlaybackSpeed() {
    if(speedSlider->value() == 100) {
        updateSpeedText(100);
        emit playbackSpeedChanged(1.0);
        return;
    }
    speedSlider->setValue(100);
}

void VideoControls::updateVolumeText(int volume) {
    volumeSlider->setToolTip(tr("Volume: %1%").arg(qBound(0, volume, 100)));
}

void VideoControls::updateSpeedText(int value) {
    const QString text = QString::number(value / 100.0, 'f', 2) + "x";
    speedButton->setText(text);
    speedPopupLabel->setText(text);
}

void VideoControls::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateResponsiveVisibility();
}

void VideoControls::updateResponsiveVisibility() {
    const bool compact = width() < 340;
    const bool veryCompact = width() < 285;
    ui->durationLabel->setVisible(!compact);
    ui->separatorLabel->setVisible(!compact);
    ui->positionLabel->setVisible(!veryCompact);
    volumeSlider->setVisible(mode == PLAYBACK_VIDEO && !veryCompact);
}
