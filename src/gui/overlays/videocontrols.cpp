#include "videocontrols.h"
#include "ui_videocontrols.h"
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QStyle>

VideoControls::VideoControls(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VideoControls),
    loopABButton(new QPushButton("A-B", this)),
    volumeSlider(new QSlider(Qt::Horizontal, this)),
    speedSlider(new QSlider(Qt::Horizontal, this)),
    speedLabel(new QLabel("1.00x", this)),
    loopABState(LOOP_AB_CLEAR),
    loopStartPosition(0)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_NoMousePropagation, true);
    setFixedHeight(26);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    ui->horizontalLayout->setContentsMargins(3, 1, 3, 1);
    ui->horizontalLayout->setSpacing(4);
    ui->horizontalLayout_2->setSpacing(2);
    hide();
    ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/play24.png");
    ui->pauseButton->setAction("pauseVideo");
    ui->prevFrameButton->setIconPath(":res/icons/common/buttons/videocontrols/skip-backwards24.png");
    ui->prevFrameButton->setAction("frameStepBack");
    ui->nextFrameButton->setIconPath(":res/icons/common/buttons/videocontrols/skip-forward24.png");
    ui->nextFrameButton->setAction("frameStep");
    ui->muteButton->setIconPath(":/res/icons/common/buttons/videocontrols/mute-on24.png");
    ui->muteButton->setAction("toggleMute");
    ui->pauseButton->setFixedSize(24, 24);
    ui->prevFrameButton->setFixedSize(24, 24);
    ui->nextFrameButton->setFixedSize(24, 24);
    ui->muteButton->setFixedSize(24, 24);
    ui->seekBar->setFixedSize(160, 18);
    ui->seekBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->positionLabel->setMinimumWidth(0);
    ui->durationLabel->setMinimumWidth(0);

    loopABButton->setAccessibleName("VideoControlTextButton");
    loopABButton->setFocusPolicy(Qt::NoFocus);
    loopABButton->setFixedSize(34, 24);
    ui->horizontalLayout_2->insertWidget(2, loopABButton);

    volumeSlider->setAccessibleName("VideoControlSlider");
    volumeSlider->setFocusPolicy(Qt::NoFocus);
    volumeSlider->setRange(0, 100);
    volumeSlider->setFixedSize(70, 18);
    volumeSlider->setValue(settings->volume());
    ui->horizontalLayout->insertWidget(3, volumeSlider);

    speedSlider->setAccessibleName("VideoControlSlider");
    speedSlider->setFocusPolicy(Qt::NoFocus);
    speedSlider->setRange(25, 400);
    speedSlider->setSingleStep(25);
    speedSlider->setPageStep(25);
    speedSlider->setFixedSize(90, 18);
    speedSlider->setValue(100);
    speedLabel->setFixedWidth(38);
    speedLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->horizontalLayout->insertWidget(4, speedLabel);
    ui->horizontalLayout->insertWidget(5, speedSlider);

    lastPosition = -1;

    connect(ui->seekBar, &VideoSlider::sliderMovedX, this, &VideoControls::seek);
    connect(volumeSlider, &QSlider::valueChanged, this, &VideoControls::volumeChanged);
    connect(speedSlider, &QSlider::valueChanged, this, [this](int value) {
        const double speed = value / 100.0;
        speedLabel->setText(QString::number(speed, 'f', 2) + "x");
        emit playbackSpeedChanged(speed);
    });
    connect(loopABButton, &QPushButton::clicked, this, [this]() {
        if(loopABState == LOOP_AB_CLEAR) {
            loopABState = LOOP_AB_START_SET;
            loopStartPosition = qMax(0, lastPosition);
            updateLoopButton();
        } else if(loopABState == LOOP_AB_START_SET) {
            const int endPosition = qMax(loopStartPosition + 1, lastPosition);
            loopABState = LOOP_AB_ACTIVE;
            emit loopABChanged(loopStartPosition, endPosition);
            updateLoopButton();
        } else {
            resetLoopAB();
            emit loopABChanged(-1, -1);
        }
    });
    updateLoopButton();
}

VideoControls::~VideoControls() {
    delete ui;
}

void VideoControls::setMode(PlaybackMode _mode) {
    mode = _mode;
    ui->muteButton->setVisible( (mode == PLAYBACK_VIDEO) );
    volumeSlider->setVisible( (mode == PLAYBACK_VIDEO) );
    speedSlider->setVisible( (mode == PLAYBACK_VIDEO) );
    speedLabel->setVisible( (mode == PLAYBACK_VIDEO) );
    loopABButton->setVisible( (mode == PLAYBACK_VIDEO) );
}

void VideoControls::setPlaybackDuration(int duration) {
    QString durationStr;
    if(mode == PLAYBACK_VIDEO) {
        int _time = duration;
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
    ui->seekBar->setRange(0, duration - 1);
    ui->durationLabel->setText(durationStr);
    ui->positionLabel->setText(durationStr);
    updateGeometry();
    ui->positionLabel->setText("");
    resetLoopAB();
}

void VideoControls::setPlaybackPosition(int position) {
    if(position == lastPosition)
        return;
    QString positionStr;
    if(mode == PLAYBACK_VIDEO) {
        int _time = position;
        int hours = _time / 3600;
        _time -= hours * 3600;
        int minutes = _time / 60;
        int seconds = _time - minutes * 60;
        positionStr = QString("%1").arg(minutes, 2, 10, QChar('0')) + ":" +
                      QString("%1").arg(seconds, 2, 10, QChar('0'));
        if(hours)
            positionStr.prepend(QString("%1").arg(hours, 2, 10, QChar('0')) + ":");
    } else {
        positionStr = QString::number(position + 1);
    }
    ui->positionLabel->setText(positionStr);
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
    volumeSlider->setValue(qBound(0, volume, 100));
}

void VideoControls::resetLoopAB() {
    loopABState = LOOP_AB_CLEAR;
    loopStartPosition = 0;
    updateLoopButton();
}

void VideoControls::updateLoopButton() {
    loopABButton->setProperty("checked", loopABState != LOOP_AB_CLEAR);
    if(loopABState == LOOP_AB_CLEAR)
        loopABButton->setText("A-B");
    else if(loopABState == LOOP_AB_START_SET)
        loopABButton->setText("A...");
    else
        loopABButton->setText("A-B");
    loopABButton->style()->unpolish(loopABButton);
    loopABButton->style()->polish(loopABButton);
}
